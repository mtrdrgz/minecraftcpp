#include "Connection.h"
#include "../core/Log.h"
#include <miniz.h>
#include <bcrypt.h>
#include <stdexcept>
#include <cstring>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace mc::net {

// ── WinsockInit ───────────────────────────────────────────────────────────────
WinsockInit::WinsockInit() {
    WSADATA wsa{};
    int r = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (r != 0) throw std::runtime_error("WSAStartup failed: " + std::to_string(r));
}
WinsockInit::~WinsockInit() { WSACleanup(); }
WinsockInit& WinsockInit::instance() {
    static WinsockInit inst;
    return inst;
}

// ── AES-128-CFB8 via Windows CNG (bcrypt) ────────────────────────────────────
struct Connection::EncryptState {
    BCRYPT_ALG_HANDLE  algo   = nullptr;
    BCRYPT_KEY_HANDLE  key    = nullptr;
    std::vector<uint8_t> iv;

    explicit EncryptState(std::span<const uint8_t> sharedSecret) {
        iv.assign(sharedSecret.begin(), sharedSecret.end()); // IV = shared secret for MC
        BCryptOpenAlgorithmProvider(&algo, BCRYPT_AES_ALGORITHM, nullptr, 0);
        BCryptSetProperty(algo, BCRYPT_CHAINING_MODE,
                          (PUCHAR)BCRYPT_CHAIN_MODE_CFB, sizeof(BCRYPT_CHAIN_MODE_CFB), 0);
        // Import key
        struct KeyBlob {
            BCRYPT_KEY_DATA_BLOB_HEADER hdr;
            uint8_t key[16];
        } blob;
        blob.hdr.dwMagic   = BCRYPT_KEY_DATA_BLOB_MAGIC;
        blob.hdr.dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
        blob.hdr.cbKeyData = 16;
        memcpy(blob.key, sharedSecret.data(), 16);
        BCryptImportKey(algo, nullptr, BCRYPT_KEY_DATA_BLOB, &key,
                        nullptr, 0, (PUCHAR)&blob, sizeof(blob), 0);
    }
    ~EncryptState() {
        if (key)  BCryptDestroyKey(key);
        if (algo) BCryptCloseAlgorithmProvider(algo, 0);
    }

    void process(uint8_t* data, size_t len, bool encrypt) {
        ULONG out = 0;
        std::vector<uint8_t> ivCopy = iv;
        if (encrypt)
            BCryptEncrypt(key, data, (ULONG)len, nullptr, ivCopy.data(), (ULONG)ivCopy.size(),
                          data, (ULONG)len, &out, 0);
        else
            BCryptDecrypt(key, data, (ULONG)len, nullptr, ivCopy.data(), (ULONG)ivCopy.size(),
                          data, (ULONG)len, &out, 0);
        // Update IV: for CFB8 the IV advances by len bytes
        if (len >= 16) {
            memcpy(iv.data(), data + len - 16, 16);
        } else {
            memmove(iv.data(), iv.data() + len, 16 - len);
            memcpy(iv.data() + 16 - len, data + (encrypt ? 0 : 0), len);
        }
    }
};

// ── Connection ────────────────────────────────────────────────────────────────
Connection::~Connection() {
    disconnect();
    delete m_encryptSend; m_encryptSend = nullptr;
    delete m_encryptRecv; m_encryptRecv = nullptr;
}

bool Connection::connect(std::string_view host, uint16_t port) {
    WinsockInit::instance(); // ensure WinSock is up

    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string portStr = std::to_string(port);
    std::string hostStr(host);
    if (getaddrinfo(hostStr.c_str(), portStr.c_str(), &hints, &res) != 0) {
        lastError = "DNS resolution failed for " + hostStr;
        MC_LOG_ERROR("{}", lastError);
        return false;
    }

    m_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (m_socket == INVALID_SOCKET) {
        freeaddrinfo(res);
        lastError = "socket() failed";
        return false;
    }

    if (::connect(m_socket, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(res);
        lastError = "connect() failed";
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }
    freeaddrinfo(res);

    // Set TCP_NODELAY (reduce latency for small packets)
    int flag = 1;
    setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    m_connected = true;
    MC_LOG_INFO("Connected to {}:{}", hostStr, port);
    return true;
}

void Connection::disconnect() {
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    m_connected = false;
}

bool Connection::sendRaw(const uint8_t* data, size_t len) {
    if (!m_connected) return false;
    if (m_encryptEnabled && m_encryptSend) {
        std::vector<uint8_t> buf(data, data + len);
        m_encryptSend->process(buf.data(), len, true);
        data = buf.data();
        // send encrypted
        size_t sent = 0;
        while (sent < len) {
            int r = ::send(m_socket, (const char*)data + sent, (int)(len - sent), 0);
            if (r <= 0) { m_connected = false; return false; }
            sent += r;
        }
        return true;
    }
    size_t sent = 0;
    while (sent < len) {
        int r = ::send(m_socket, (const char*)data + sent, (int)(len - sent), 0);
        if (r <= 0) { m_connected = false; return false; }
        sent += r;
    }
    return true;
}

bool Connection::recvRaw(uint8_t* data, size_t len) {
    if (!m_connected) return false;
    size_t got = 0;
    while (got < len) {
        int r = ::recv(m_socket, (char*)data + got, (int)(len - got), 0);
        if (r <= 0) { m_connected = false; return false; }
        got += r;
    }
    if (m_encryptEnabled && m_encryptRecv) {
        m_encryptRecv->process(data, len, false);
    }
    return true;
}

bool Connection::recvVarInt(int32_t& out) {
    out = 0;
    int shift = 0;
    while (true) {
        uint8_t b;
        if (!recvRaw(&b, 1)) return false;
        out |= (int32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) return true;
        shift += 7;
        if (shift >= 35) return false;
    }
}

bool Connection::sendPacket(PacketBuffer& payload) {
    // Packet format: [VarInt length][data]
    // If compression enabled and data >= threshold: [VarInt totalLen][VarInt dataLen][compressed data]
    // If compression enabled and data < threshold:  [VarInt totalLen][VarInt 0][uncompressed data]

    const auto& raw = payload.data();

    if (m_compressionThreshold < 0) {
        // No compression: [VarInt len][data]
        PacketBuffer frame;
        frame.writeVarInt((int32_t)raw.size());
        frame.writeBytes(raw);
        return sendRaw(frame.data().data(), frame.data().size());
    } else {
        // With compression
        PacketBuffer frame;
        if ((int32_t)raw.size() >= m_compressionThreshold) {
            // Compress
            mz_ulong compLen = mz_compressBound((mz_ulong)raw.size());
            std::vector<uint8_t> comp(compLen);
            if (mz_compress(comp.data(), &compLen, raw.data(), (mz_ulong)raw.size()) != MZ_OK)
                return false;
            comp.resize(compLen);

            PacketBuffer inner;
            inner.writeVarInt((int32_t)raw.size()); // uncompressed length
            inner.writeBytes(comp);

            frame.writeVarInt((int32_t)inner.size());
            frame.writeBytes(inner.rawSpan());
        } else {
            // Don't compress, write 0 for data length
            PacketBuffer inner;
            inner.writeVarInt(0); // 0 = not compressed
            inner.writeBytes(raw);

            frame.writeVarInt((int32_t)inner.size());
            frame.writeBytes(inner.rawSpan());
        }
        return sendRaw(frame.data().data(), frame.data().size());
    }
}

bool Connection::receivePacket(PacketBuffer& outPacket, int32_t& outPacketId) {
    // Read length
    int32_t packetLen;
    if (!recvVarInt(packetLen) || packetLen <= 0) return false;

    std::vector<uint8_t> data(packetLen);
    if (!recvRaw(data.data(), packetLen)) return false;

    PacketBuffer buf(data);

    if (m_compressionThreshold >= 0) {
        int32_t dataLen = buf.readVarInt();
        if (dataLen > 0) {
            // Decompress
            std::vector<uint8_t> decomp(dataLen);
            mz_ulong dLen = (mz_ulong)dataLen;
            auto compData = buf.readBytes(buf.remaining());
            if (mz_uncompress(decomp.data(), &dLen, compData.data(), (mz_ulong)compData.size()) != MZ_OK)
                return false;
            buf = PacketBuffer(decomp);
        }
        // else dataLen == 0: not compressed, continue reading
    }

    outPacketId = buf.readVarInt();
    // Remaining bytes are the packet payload
    outPacket = PacketBuffer(std::span<const uint8_t>(
        buf.data().data() + buf.readPos(),
        buf.remaining()
    ));
    return true;
}

bool Connection::hasData() const {
    if (!m_connected || m_socket == INVALID_SOCKET) return false;
    fd_set fds; FD_ZERO(&fds); FD_SET(m_socket, &fds);
    TIMEVAL tv{0, 0};
    return select(0, &fds, nullptr, nullptr, &tv) > 0;
}

void Connection::setCompression(int32_t threshold) {
    m_compressionThreshold = threshold;
    MC_LOG_INFO("Compression enabled (threshold={})", threshold);
}

void Connection::enableEncryption(std::span<const uint8_t> sharedSecret) {
    delete m_encryptSend; m_encryptSend = new EncryptState(sharedSecret);
    delete m_encryptRecv; m_encryptRecv = new EncryptState(sharedSecret);
    m_encryptEnabled = true;
    MC_LOG_INFO("Encryption enabled");
}

} // namespace mc::net

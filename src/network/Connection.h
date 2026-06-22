#pragma once
#include "PacketBuffer.h"
#include <cstdint>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <functional>
#include <atomic>
#include <memory>

// Network — WinSock2 on Windows, BSD sockets on Linux
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
// SOCKET is already defined by winsock2.h on Windows
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

namespace mc::net {

enum class ConnectionState { Handshake, Status, Login, Config, Play };

// Port of net.minecraft.network.Connection
// TCP connection to a Minecraft server, handles framing + optional zlib compression
class Connection {
public:
    ~Connection();

    // Connect to server (blocking DNS lookup + connect)
    bool connect(std::string_view host, uint16_t port);
    void disconnect();
    bool isConnected() const { return m_connected; }

    // Packet I/O (thread-safe)
    bool sendPacket(PacketBuffer& buf);
    bool receivePacket(PacketBuffer& outPacket, int32_t& outPacketId);

    // Compression (threshold >= 0 enables it)
    void setCompression(int32_t threshold);

    // Encryption (AES-128-CFB8, called after login success)
    void enableEncryption(std::span<const uint8_t> sharedSecret);

    ConnectionState state = ConnectionState::Handshake;

    // Non-blocking check: returns true if at least 1 byte is available to read
    bool hasData() const;


    std::string lastError;

private:
    // Send/receive raw bytes on the socket
    bool sendRaw(const uint8_t* data, size_t len);
    bool recvRaw(uint8_t* data, size_t len);
    bool recvVarInt(int32_t& out);

    SOCKET m_socket = INVALID_SOCKET;
    std::atomic<bool> m_connected{false};
    int32_t m_compressionThreshold = -1; // -1 = disabled

    // Encryption state (AES-128-CFB8) — raw pointer to avoid unique_ptr<incomplete type>
    struct EncryptState;
    EncryptState* m_encryptSend = nullptr;
    EncryptState* m_encryptRecv = nullptr;
    bool m_encryptEnabled = false;
};

// RAII WinSock init
struct WinsockInit {
    WinsockInit();
    ~WinsockInit();
    static WinsockInit& instance();
};

} // namespace mc::net

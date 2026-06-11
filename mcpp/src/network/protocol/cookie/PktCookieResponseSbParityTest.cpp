// Byte-exact parity gate for
// net.minecraft.network.protocol.cookie.ServerboundCookieResponsePacket.
//
// Real codec (ServerboundCookieResponsePacket.STREAM_CODEC, 26.1.2) is
// Packet.codec(write, new) so the wire is just the body, no packet-id prefix.
//
// write (ServerboundCookieResponsePacket.java:20-23):
//   output.writeIdentifier(this.key);
//   output.writeNullable(this.payload, ClientboundStoreCookiePacket.PAYLOAD_STREAM_CODEC);
//
// FriendlyByteBuf.writeIdentifier (FriendlyByteBuf.java:585-586):
//   writeUtf(identifier.toString())  ==  Utf8String.write:
//     VarInt(UTF-8 byteLength) then the UTF-8 bytes (default maxLength 32767).
//   PacketBuffer.writeString is the certified 1:1 port of Utf8String.write.
//
// FriendlyByteBuf.writeNullable (FriendlyByteBuf.java:267-274):
//   value != null -> writeBoolean(true)  then valueEncoder.encode(value)
//   value == null -> writeBoolean(false)
//   PAYLOAD_STREAM_CODEC = ByteBufCodecs.byteArray(5120); its encode is
//   FriendlyByteBuf.writeByteArray (FriendlyByteBuf.java:289-292):
//       VarInt.write(output, bytes.length); output.writeBytes(bytes);
//
// So the body is exactly:
//   VarInt(keyLen) | keyUtf8 | presentByte | [ VarInt(payloadLen) | payloadBytes ]
// which the certified mc::net::PacketBuffer (the FriendlyByteBuf port) implements
// 1:1 (writeString == writeUtf, writeBool, writeVarInt, raw bytes).
//
// This test rebuilds each ground-truth row through PacketBuffer in the exact codec
// order and requires the produced bytes (and size) to match the GT hex, then
// decodes the GT bytes back through PacketBuffer and requires every field to
// round-trip identically.
//
//   pkt_cookie_response_sb_parity --cases mcpp/build/pkt_cookie_response_sb.tsv

#include "../../PacketBuffer.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string toHexLower(const std::vector<uint8_t>& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        out.push_back(digits[(b >> 4) & 0xF]);
        out.push_back(digits[b & 0xF]);
    }
    return out;
}

std::vector<uint8_t> fromHex(const std::string& hex) {
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        int hi = nibble(hex[i]);
        int lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) continue;
        out.push_back((uint8_t)((hi << 4) | lo));
    }
    return out;
}

// Split a tab-separated line into fields (keeps empty fields, strips a trailing CR).
std::vector<std::string> splitTab(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') {
            out.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }
    if (casesPath.empty()) {
        std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]);
        return 2;
    }

    std::ifstream in(casesPath, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open cases file: %s\n", casesPath.c_str());
        return 2;
    }

    int cases = 0;
    int mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> f = splitTab(line);
        if (f.empty() || f[0] != "ENC") continue;
        // ENC \t name \t keyHex \t present \t payloadHex \t readableBytes \t wireHex
        if (f.size() != 7) {
            std::fprintf(stderr, "malformed row (cols=%zu): %s\n", f.size(), line.c_str());
            mismatches++;
            cases++;
            continue;
        }

        const std::string& name    = f[1];
        std::vector<uint8_t> keyUtf8 = fromHex(f[2]);
        int present                 = (int)std::stol(f[3]);
        std::vector<uint8_t> payload = fromHex(f[4]);
        size_t expectReadable       = (size_t)std::stoull(f[5]);
        std::string expectHex       = f[6];

        cases++;

        // --- Encode through PacketBuffer in exact codec order. ---
        mc::net::PacketBuffer buf;
        // writeIdentifier(key) == writeUtf(key.toString()): VarInt(byteLen)+UTF-8 bytes.
        std::string keyStr((const char*)keyUtf8.data(), keyUtf8.size());
        buf.writeString(keyStr);
        // writeNullable(payload, byteArray codec): present flag, then VarInt(len)+bytes.
        buf.writeBool(present != 0);
        if (present != 0) {
            buf.writeVarInt((int32_t)payload.size());
            buf.writeBytes(payload);
        }

        std::string gotHex = toHexLower(buf.data());
        if (gotHex != expectHex || buf.data().size() != expectReadable) {
            std::fprintf(stderr,
                "MISMATCH(enc) %s present=%d: bytes=%zu/%zu\n  got  %s\n  want %s\n",
                name.c_str(), present, buf.data().size(), expectReadable,
                gotHex.c_str(), expectHex.c_str());
            mismatches++;
            continue;
        }

        // --- Decode the expected bytes back and require fields round-trip. ---
        std::vector<uint8_t> raw = fromHex(expectHex);
        mc::net::PacketBuffer rd(raw);
        bool rtOk = true;
        std::string rKey = rd.readString();
        if (rKey != keyStr) rtOk = false;
        bool rPresent = rd.readBool();
        if (rPresent != (present != 0)) rtOk = false;
        if (rtOk && rPresent) {
            int32_t rLen = rd.readVarInt();
            if (rLen < 0 || (size_t)rLen != payload.size()) {
                rtOk = false;
            } else {
                std::vector<uint8_t> rPayload = rd.readBytes((size_t)rLen);
                if (rPayload != payload) rtOk = false;
            }
        }
        rtOk = rtOk && (rd.remaining() == 0);
        if (!rtOk) {
            std::fprintf(stderr,
                "MISMATCH(dec) %s present=%d: rKey=%s rPresent=%d rem=%zu\n",
                name.c_str(), present, rKey.c_str(), (int)rPresent, rd.remaining());
            mismatches++;
            continue;
        }
    }

    std::printf("PktCookieResponseSbParity checks=%d mismatches=%d\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}

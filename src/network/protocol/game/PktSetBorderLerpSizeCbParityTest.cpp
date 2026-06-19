// Byte-exact parity gate for
// net.minecraft.network.protocol.game.ClientboundSetBorderLerpSizePacket.
//
// Real codec (ClientboundSetBorderLerpSizePacket.STREAM_CODEC, 26.1.2) is
// Packet.codec(write, new) so the wire is just the body, no packet-id prefix.
//
// write (ClientboundSetBorderLerpSizePacket.java:29-33):
//   output.writeDouble(this.oldSize);
//   output.writeDouble(this.newSize);
//   output.writeVarLong(this.lerpTime);
//
// FriendlyByteBuf.writeDouble -> ByteBuf.writeDouble: 8 big-endian bytes of
//   Double.doubleToRawLongBits(value). PacketBuffer.writeDouble is the certified
//   1:1 port (memcpy the double's raw bits, then big-endian).
//
// FriendlyByteBuf.writeVarLong -> VarLong.write (VarLong.java:41-49): plain LEB128,
//   NO zig-zag. Every negative long encodes to the full 10 bytes. PacketBuffer.
//   writeVarLong is the certified 1:1 port.
//
// So the body is exactly:
//   8 BE bytes(oldSize) | 8 BE bytes(newSize) | VarLong(lerpTime)
// which the certified mc::net::PacketBuffer (the FriendlyByteBuf port) implements
// 1:1 (writeDouble, writeVarLong).
//
// This test rebuilds each ground-truth row through PacketBuffer in the exact codec
// order and requires the produced bytes (and size) to match the GT hex, then
// decodes the GT bytes back through PacketBuffer and requires every field to
// round-trip identically.
//
//   pkt_set_border_lerp_size_cb_parity --cases mcpp/build/pkt_set_border_lerp_size_cb.tsv

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

double bitsToDouble(uint64_t bits) {
    double d;
    std::memcpy(&d, &bits, 8);
    return d;
}

uint64_t doubleToBits(double d) {
    uint64_t bits;
    std::memcpy(&bits, &d, 8);
    return bits;
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
        // ENC \t name \t oldBits \t newBits \t lerpTime \t readableBytes \t wireHex
        if (f.size() != 7) {
            std::fprintf(stderr, "malformed row (cols=%zu): %s\n", f.size(), line.c_str());
            mismatches++;
            cases++;
            continue;
        }

        const std::string& name = f[1];
        // Floats/doubles carried as raw long bits; VarLong field as a decimal int64.
        uint64_t oldBits        = (uint64_t)std::stoull(f[2]);
        uint64_t newBits        = (uint64_t)std::stoull(f[3]);
        int64_t  lerpTime       = (int64_t)std::stoll(f[4]);
        size_t   expectReadable = (size_t)std::stoull(f[5]);
        std::string expectHex   = f[6];

        cases++;

        double oldSize = bitsToDouble(oldBits);
        double newSize = bitsToDouble(newBits);

        // --- Encode through PacketBuffer in exact codec order. ---
        mc::net::PacketBuffer buf;
        buf.writeDouble(oldSize);
        buf.writeDouble(newSize);
        buf.writeVarLong(lerpTime);

        std::string gotHex = toHexLower(buf.data());
        if (gotHex != expectHex || buf.data().size() != expectReadable) {
            std::fprintf(stderr,
                "MISMATCH(enc) %s: bytes=%zu/%zu\n  got  %s\n  want %s\n",
                name.c_str(), buf.data().size(), expectReadable,
                gotHex.c_str(), expectHex.c_str());
            mismatches++;
            continue;
        }

        // --- Decode the expected bytes back and require fields round-trip. ---
        std::vector<uint8_t> raw = fromHex(expectHex);
        mc::net::PacketBuffer rd(raw);
        bool rtOk = true;
        double rOld = rd.readDouble();
        double rNew = rd.readDouble();
        int64_t rLerp = rd.readVarLong();
        // Compare doubles by their raw bits (NaN/-0.0 exact).
        if (doubleToBits(rOld) != oldBits) rtOk = false;
        if (doubleToBits(rNew) != newBits) rtOk = false;
        if (rLerp != lerpTime) rtOk = false;
        rtOk = rtOk && (rd.remaining() == 0);
        if (!rtOk) {
            std::fprintf(stderr,
                "MISMATCH(dec) %s: rOldBits=%llu rNewBits=%llu rLerp=%lld rem=%zu\n",
                name.c_str(),
                (unsigned long long)doubleToBits(rOld),
                (unsigned long long)doubleToBits(rNew),
                (long long)rLerp, rd.remaining());
            mismatches++;
            continue;
        }
    }

    std::printf("PktSetBorderLerpSizeCbParity checks=%d mismatches=%d\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}

// Byte-exact parity gate for
// net.minecraft.network.protocol.common.ClientboundResourcePackPopPacket.
//
// Real codec (ClientboundResourcePackPopPacket.STREAM_CODEC, 26.1.2) is
// Packet.codec(write, new) so the wire is just the body, no packet-id prefix.
//
// write (ClientboundResourcePackPopPacket.java:20-22):
//   output.writeOptional(this.id, UUIDUtil.STREAM_CODEC)
//
// FriendlyByteBuf.writeOptional (FriendlyByteBuf.java:224-231):
//   present -> writeBoolean(true)  then valueWriter.encode(this, value.get())
//   absent  -> writeBoolean(false)
// UUIDUtil.STREAM_CODEC.encode == FriendlyByteBuf.writeUUID (FriendlyByteBuf.java:498-501):
//   writeLong(mostSignificantBits) then writeLong(leastSignificantBits)  (both BE 8-byte).
//
// So the body is exactly:
//   present=0 -> single byte 0x00
//   present=1 -> 0x01, BE long(msb), BE long(lsb)  (17 bytes)
// Each field reduces to a boolean byte and two big-endian 8-byte longs, which the
// certified mc::net::PacketBuffer (the FriendlyByteBuf port) implements 1:1
// (writeBool, writeUUID == writeLong+writeLong).
//
// This test rebuilds each ground-truth row through PacketBuffer in the exact codec
// order and requires the produced bytes (and readableBytes) to match the GT hex,
// then decodes the GT bytes back through PacketBuffer and requires every field to
// round-trip identically.
//
//   pkt_resource_pack_pop_cb_parity --cases mcpp/build/pkt_resource_pack_pop_cb.tsv

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
        // ENC \t present \t msb \t lsb \t readableBytes \t hexBytes
        if (f.size() != 6) {
            std::fprintf(stderr, "malformed row (cols=%zu): %s\n", f.size(), line.c_str());
            mismatches++;
            cases++;
            continue;
        }

        int      present = (int)std::stol(f[1]);
        int64_t  msb     = (int64_t)std::stoll(f[2]);
        int64_t  lsb     = (int64_t)std::stoll(f[3]);
        size_t   expectReadable = (size_t)std::stoull(f[4]);
        std::string expectHex   = f[5];

        cases++;

        // --- Encode through PacketBuffer in exact codec order. ---
        mc::net::PacketBuffer buf;
        buf.writeBool(present != 0);                       // writeOptional present flag
        if (present != 0) {
            buf.writeUUID((uint64_t)msb, (uint64_t)lsb);   // writeUUID: writeLong(msb), writeLong(lsb)
        }

        std::string gotHex = toHexLower(buf.data());
        if (gotHex != expectHex || buf.data().size() != expectReadable) {
            std::fprintf(stderr,
                "MISMATCH(enc) present=%d msb=%lld lsb=%lld: bytes=%zu/%zu\n  got  %s\n  want %s\n",
                present, (long long)msb, (long long)lsb,
                buf.data().size(), expectReadable, gotHex.c_str(), expectHex.c_str());
            mismatches++;
            continue;
        }

        // --- Decode the expected bytes back and require fields round-trip. ---
        std::vector<uint8_t> raw = fromHex(expectHex);
        mc::net::PacketBuffer rd(raw);
        bool rPresent = rd.readBool();
        bool rtOk = (rPresent == (present != 0));
        if (rtOk && rPresent) {
            uint64_t rHi = 0, rLo = 0;
            rd.readUUID(rHi, rLo);
            if ((int64_t)rHi != msb || (int64_t)rLo != lsb) rtOk = false;
        }
        rtOk = rtOk && (rd.remaining() == 0);
        if (!rtOk) {
            std::fprintf(stderr,
                "MISMATCH(dec) present=%d msb=%lld lsb=%lld: rPresent=%d rem=%zu\n",
                present, (long long)msb, (long long)lsb,
                (int)rPresent, rd.remaining());
            mismatches++;
            continue;
        }
    }

    std::printf("PktResourcePackPopCbParity checks=%d mismatches=%d\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}

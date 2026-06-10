// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundSetCommandBlockPacket.
//
// Real codec order (ServerboundSetCommandBlockPacket.write, 26.1.2):
//   writeBlockPos(pos) -> writeLong(pos.asLong())   big-endian 8-byte long
//   writeUtf(command)  -> VarInt(byteLen) + UTF-8 bytes
//   writeEnum(mode)    -> VarInt(mode.ordinal())     SEQUENCE=0, AUTO=1, REDSTONE=2
//   writeByte(flags)   -> low 8 bits; bit0=trackOutput bit1=conditional bit2=automatic
//
// This test rebuilds each ground-truth row through mc::net::PacketBuffer in the exact
// codec order and requires the produced bytes (and readableBytes) to match the GT hex,
// then decodes the GT bytes back through PacketBuffer and requires every field to
// round-trip identically.

#include "../../PacketBuffer.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
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

// Split a tab-separated line into fields (keeps empty fields).
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

    std::ifstream in(casesPath);
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
        // ENC \t name \t posLong \t commandHex \t modeOrdinal \t flags \t readableBytes \t hexBytes
        if (f.size() < 8) {
            std::fprintf(stderr, "malformed row (cols=%zu): %s\n", f.size(), line.c_str());
            mismatches++;
            cases++;
            continue;
        }

        const std::string& name = f[1];
        int64_t posLong = (int64_t)std::stoll(f[2]);
        std::vector<uint8_t> cmdBytes = fromHex(f[3]);
        std::string command(cmdBytes.begin(), cmdBytes.end());
        int32_t modeOrdinal = (int32_t)std::stol(f[4]);
        int32_t flags = (int32_t)std::stol(f[5]);
        size_t expectReadable = (size_t)std::stoull(f[6]);
        std::string expectHex = f[7];

        cases++;

        // --- Encode through PacketBuffer in exact codec order. ---
        mc::net::PacketBuffer buf;
        buf.writeLong(posLong);                 // writeBlockPos -> writeLong(asLong)
        buf.writeString(command);               // writeUtf
        buf.writeVarInt(modeOrdinal);           // writeEnum -> VarInt(ordinal)
        buf.writeByte((uint8_t)(flags & 0xFF)); // writeByte(flags)

        std::string gotHex = toHexLower(buf.data());
        if (gotHex != expectHex || buf.data().size() != expectReadable) {
            std::fprintf(stderr,
                "MISMATCH(enc) %s: bytes=%zu/%zu hex=%s expected=%s\n",
                name.c_str(), buf.data().size(), expectReadable,
                gotHex.c_str(), expectHex.c_str());
            mismatches++;
            continue;
        }

        // --- Decode the expected bytes back and require fields round-trip. ---
        std::vector<uint8_t> raw = fromHex(expectHex);
        mc::net::PacketBuffer rd(raw);
        int64_t rPos = rd.readLong();
        std::string rCmd = rd.readString();
        int32_t rMode = rd.readVarInt();
        int32_t rFlags = (int32_t)rd.readByte();

        bool rtOk = (rPos == posLong)
                 && (rCmd == command)
                 && (rMode == modeOrdinal)
                 && (rFlags == (flags & 0xFF))
                 && (rd.remaining() == 0);
        if (!rtOk) {
            std::fprintf(stderr,
                "MISMATCH(dec) %s: pos=%lld/%lld cmdEq=%d mode=%d/%d flags=%d/%d rem=%zu\n",
                name.c_str(), (long long)rPos, (long long)posLong,
                (int)(rCmd == command), rMode, modeOrdinal, rFlags, flags & 0xFF,
                rd.remaining());
            mismatches++;
            continue;
        }
    }

    std::printf("ServerboundSetCommandBlock cases=%d mismatches=%d\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}

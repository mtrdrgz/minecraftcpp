// Parity gate for ClientboundDebugSamplePacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktDebugSampleParity.java ground truth).
//
// The packet is a record (long[] sample, RemoteDebugSampleType debugSampleType).
// Its body is exactly:
//   write : output.writeLongArray(this.sample);   output.writeEnum(this.debugSampleType);
//   read  : this(input.readLongArray(), input.readEnum(RemoteDebugSampleType.class));
// (net.minecraft.network.protocol.game.ClientboundDebugSamplePacket lines 14-21.)
//
// FriendlyByteBuf.writeLongArray = VarInt.write(longs.length) + each long via
//   output.writeLong(l) -> a plain BIG-ENDIAN 8-byte long (NOT VarLong)
//   (FriendlyByteBuf lines 343-357). writeEnum = writeVarInt(ordinal) (line 471).
//
// Reuses the certified PacketBuffer (FriendlyByteBuf port): writeVarInt is LEB128,
// writeLong is BE8, matching VarInt.write + ByteBuf.writeLong byte-for-byte.
// RemoteDebugSampleType has one constant TICK_TIME (ordinal 0).
//
//   pkt_debug_sample_parity [--cases mcpp/build/pkt_debug_sample.tsv]
#include "../../PacketBuffer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::net::PacketBuffer;

namespace {
std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}

// Parse the comma-joined signed-decimal 64-bit long list; "-" means an empty list.
std::vector<int64_t> parseLongs(const std::string& col) {
    std::vector<int64_t> out;
    if (col == "-" || col.empty()) return out;
    std::istringstream ss(col);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (tok.empty()) continue;
        out.push_back((int64_t)std::stoll(tok));
    }
    return out;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        bytes.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return bytes;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_debug_sample.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string tag;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;

        // ENC <name> <count> <l0,l1,...|-> <enumOrdinal> <readableBytes> <hexBytes>
        std::string name, countStr, longsCol, ordinalStr, readableStr, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, countStr, '\t')
            || !std::getline(ss, longsCol, '\t') || !std::getline(ss, ordinalStr, '\t')
            || !std::getline(ss, readableStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t count = (int32_t)std::stoll(countStr);
        int32_t ordinal = (int32_t)std::stoll(ordinalStr);
        int32_t expReadable = (int32_t)std::stoll(readableStr);
        std::vector<int64_t> sample = parseLongs(longsCol);

        if ((int32_t)sample.size() != count) {
            ++mismatches;
            std::cerr << "BAD-ROW name=" << name << " count=" << count
                      << " parsedLongs=" << sample.size() << "\n";
            continue;
        }

        // write(): writeLongArray -> writeVarInt(size) + each long BE8;
        //          writeEnum -> writeVarInt(ordinal).
        PacketBuffer enc;
        enc.writeVarInt((int32_t)sample.size());
        for (int64_t l : sample) enc.writeLong(l);
        enc.writeVarInt(ordinal);

        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH name=" << name
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }
        if ((int32_t)enc.size() != expReadable) {
            ++mismatches;
            std::cerr << "READABLE-MISMATCH name=" << name
                      << " got=" << enc.size() << " want=" << expReadable << "\n";
            continue;
        }

        // read(): decode expected bytes -> readVarInt(count) + count*readLong + readVarInt(ordinal).
        PacketBuffer dec(unhex(expHex));
        int32_t decCount = dec.readVarInt();
        if (decCount != count) {
            ++mismatches;
            std::cerr << "DEC-COUNT-MISMATCH name=" << name
                      << " got=" << decCount << " want=" << count << "\n";
            continue;
        }
        bool rowBad = false;
        for (int32_t i = 0; i < decCount; ++i) {
            int64_t l = dec.readLong();
            if (l != sample[(size_t)i]) {
                ++mismatches;
                rowBad = true;
                std::cerr << "DEC-LONG-MISMATCH name=" << name << " idx=" << i
                          << " got=" << l << " want=" << sample[(size_t)i] << "\n";
                break;
            }
        }
        if (rowBad) continue;
        int32_t decOrdinal = dec.readVarInt();
        if (decOrdinal != ordinal) {
            ++mismatches;
            std::cerr << "DEC-ORDINAL-MISMATCH name=" << name
                      << " got=" << decOrdinal << " want=" << ordinal << "\n";
            continue;
        }
    }

    std::cout << "PktDebugSampleParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

// Byte-exact parity gate for ClientboundSetExperiencePacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktSetExperienceParity.java ground truth).
//
// Source (26.1.2/src/.../ClientboundSetExperiencePacket.java):
//   write(buf): buf.writeFloat(experienceProgress);   // 4B big-endian IEEE-754
//               buf.writeVarInt(experienceLevel);      // LEVEL first  (LEB128, no zig-zag)
//               buf.writeVarInt(totalExperience);      // TOTAL second (LEB128, no zig-zag)
//   read(buf) : readFloat(); readVarInt() -> level; readVarInt() -> total;
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body. The whole
// wire payload is FLOAT(progress) ++ VARINT(level) ++ VARINT(total).
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly: writeFloat /
// writeVarInt and readFloat / readVarInt are byte-for-byte / value-for-value identical to
// the real codec.
//
//   pkt_set_experience_parity [--cases mcpp/build/pkt_set_experience.tsv]
//
// Row: ENC <progressBits-08x> <level-dec> <total-dec> <readableBytes-dec> <hex>
#include "../../PacketBuffer.h"

#include <bit>
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
    s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    out.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}

float bitsToFloat(uint32_t bits) { return std::bit_cast<float>(bits); }
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_set_experience.tsv";
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

        // ENC <progressBits-08x> <level-dec> <total-dec> <readableBytes-dec> <hex>
        std::string progStr, levelStr, totalStr, lenStr, expHex;
        if (!std::getline(ss, progStr, '\t') || !std::getline(ss, levelStr, '\t')
            || !std::getline(ss, totalStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;

        uint32_t progBits = (uint32_t)std::stoul(progStr, nullptr, 16);
        float    progress = bitsToFloat(progBits);
        int32_t  level    = (int32_t)std::stoll(levelStr);
        int32_t  total    = (int32_t)std::stoll(totalStr);
        size_t   expLen   = (size_t)std::stoull(lenStr);

        // (1) ENCODE: write the fields in the REAL wire order (progress, LEVEL, TOTAL).
        PacketBuffer enc;
        enc.writeFloat(progress);
        enc.writeVarInt(level);
        enc.writeVarInt(total);

        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH prog=" << progStr << " level=" << level
                      << " total=" << total << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH prog=" << progStr << " level=" << level
                      << " total=" << total << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // (2) DECODE: read the Java bytes back and verify fields bit-exact + ordered.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        float   gotProgress = dec.readFloat();
        int32_t gotLevel    = dec.readVarInt();
        int32_t gotTotal    = dec.readVarInt();

        if (std::bit_cast<uint32_t>(gotProgress) != progBits) {
            ++mismatches;
            std::cerr << "DEC-PROGRESS-MISMATCH hex=" << expHex << " got="
                      << std::hex << std::bit_cast<uint32_t>(gotProgress)
                      << " want=" << progBits << std::dec << "\n";
        }
        if (gotLevel != level) {
            ++mismatches;
            std::cerr << "DEC-LEVEL-MISMATCH hex=" << expHex << " got=" << gotLevel
                      << " want=" << level << "\n";
        }
        if (gotTotal != total) {
            ++mismatches;
            std::cerr << "DEC-TOTAL-MISMATCH hex=" << expHex << " got=" << gotTotal
                      << " want=" << total << "\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING prog=" << progStr << " remaining="
                      << dec.remaining() << "\n";
        }
    }

    std::cout << "PktSetExperienceParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

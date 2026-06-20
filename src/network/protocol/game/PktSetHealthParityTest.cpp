// Parity gate for net.minecraft.network.protocol.game.ClientboundSetHealthPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktSetHealthParity.java GT).
//
// The packet body is exactly (real 26.1.2 source, ClientboundSetHealthPacket 22-32):
//   write : FriendlyByteBuf.writeFloat(this.health)     // raw int bits, big-endian
//           FriendlyByteBuf.writeVarInt(this.food)      // LEB128, no zig-zag
//           FriendlyByteBuf.writeFloat(this.saturation) // raw int bits, big-endian
//   read  : readFloat() ; readVarInt() ; readFloat()
// STREAM_CODEC = Packet.codec(write, new) -> StreamCodec.ofMember: NO packet-id prefix,
// just the body. So the wire payload is: 4B float + VarInt(food) + 4B float.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeFloat (memcpy raw bits -> big-endian uint32) and writeVarInt are byte-for-byte
// the same as the real codec. For each ENC row we:
//   (1) ENCODE: write health/food/saturation in order; require bytes == Java hex AND
//       byte count == readableBytes.
//   (2) DECODE: read the Java bytes back via readFloat/readVarInt/readFloat and require
//       bit-exact float recovery + exact VarInt + no trailing bytes.
//
//   pkt_set_health_parity [--cases mcpp/build/pkt_set_health.tsv]
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
        out.push_back((uint8_t)std::stoul(s.substr(i, 2), nullptr, 16));
    return out;
}

float bitsToFloat(uint32_t bits) { return std::bit_cast<float>(bits); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_set_health.tsv";
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

        // ENC <healthBits-08x> <food-dec> <saturationBits-08x> <readableBytes-dec> <hex>
        std::string healthStr, foodStr, satStr, lenStr, expHex;
        if (!std::getline(ss, healthStr, '\t') || !std::getline(ss, foodStr, '\t')
            || !std::getline(ss, satStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;

        uint32_t healthBits = (uint32_t)std::stoul(healthStr, nullptr, 16);
        uint32_t satBits    = (uint32_t)std::stoul(satStr, nullptr, 16);
        int32_t  food       = (int32_t)std::stoll(foodStr);
        size_t   expLen     = (size_t)std::stoull(lenStr);

        float health     = bitsToFloat(healthBits);
        float saturation = bitsToFloat(satBits);

        // (1) ENCODE: writeFloat(health) ; writeVarInt(food) ; writeFloat(saturation).
        PacketBuffer enc;
        enc.writeFloat(health);
        enc.writeVarInt(food);
        enc.writeFloat(saturation);

        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH health=" << healthStr << " food=" << food
                      << " sat=" << satStr << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH food=" << food << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // (2) DECODE: readFloat()/readVarInt()/readFloat() recover the exact fields.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        float gotHealth = dec.readFloat();
        int32_t gotFood = dec.readVarInt();
        float gotSat = dec.readFloat();

        if (std::bit_cast<uint32_t>(gotHealth) != healthBits) {
            ++mismatches;
            std::cerr << "DEC-HEALTH-MISMATCH hex=" << expHex
                      << " got=" << std::bit_cast<uint32_t>(gotHealth)
                      << " want=" << healthBits << "\n";
        }
        if (gotFood != food) {
            ++mismatches;
            std::cerr << "DEC-FOOD-MISMATCH hex=" << expHex << " got=" << gotFood
                      << " want=" << food << "\n";
        }
        if (std::bit_cast<uint32_t>(gotSat) != satBits) {
            ++mismatches;
            std::cerr << "DEC-SAT-MISMATCH hex=" << expHex
                      << " got=" << std::bit_cast<uint32_t>(gotSat)
                      << " want=" << satBits << "\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING food=" << food << " remaining="
                      << dec.remaining() << "\n";
        }
    }

    std::cout << "PktSetHealthParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

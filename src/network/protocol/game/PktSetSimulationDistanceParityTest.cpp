// Parity gate for ClientboundSetSimulationDistancePacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktSetSimulationDistanceParity.java ground truth).
//
// The packet is a record `ClientboundSetSimulationDistancePacket(int simulationDistance)`.
// Its body is exactly:
//   write : output.writeVarInt(this.simulationDistance)
//   read  : input.readVarInt()
// (net.minecraft.network.protocol.game.ClientboundSetSimulationDistancePacket lines 9-19.)
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body, so the whole
// wire payload is a single VarInt (LEB128) of the signed int `simulationDistance` (no
// zig-zag).
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt(value) / readVarInt() are byte-for-byte / value-for-value the same as the
// real codec (negatives encode as 5 bytes).
//
//   pkt_set_simulation_distance_parity [--cases mcpp/build/pkt_set_simulation_distance.tsv]
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

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_set_simulation_distance.tsv";
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

        // ENC <simulationDistance-dec> <readableBytes-dec> <hex>
        std::string valStr, lenStr, expHex;
        if (!std::getline(ss, valStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;
        int32_t value = (int32_t)std::stoll(valStr);
        size_t expLen = (size_t)std::stoull(lenStr);

        // write(): FriendlyByteBuf.writeVarInt(simulationDistance).
        PacketBuffer enc;
        enc.writeVarInt(value);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH value=" << value << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH value=" << value << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // read(): input.readVarInt() must recover the exact signed int.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotValue = dec.readVarInt();
        if (gotValue != value) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex << " got=" << gotValue
                      << " want=" << value << "\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING value=" << value << " remaining="
                      << dec.remaining() << "\n";
        }
    }

    std::cout << "PktSetSimulationDistanceParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

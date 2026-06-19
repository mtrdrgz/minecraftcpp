// Parity gate for ClientboundSetChunkCacheCenterPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktSetChunkCenterParity.java ground truth).
//
// The packet body is exactly (net.minecraft.network.protocol.game.
// ClientboundSetChunkCacheCenterPacket lines 20-28; Packet.codec -> no id prefix):
//   write : output.writeVarInt(x); output.writeVarInt(z);
//   read  : x = input.readVarInt(); z = input.readVarInt();
// writeVarInt is LEB128 over the int's unsigned 32-bit pattern (negatives -> 5
// bytes). This reuses the certified PacketBuffer (FriendlyByteBuf port): we write
// writeVarInt(x), writeVarInt(z) in that order and require the emitted bytes (hex)
// and readableBytes match the ground truth, then decode the expected bytes back
// via readVarInt() twice and require (x,z) round-trip exactly.
//
//   pkt_set_chunk_center_parity [--cases mcpp/build/pkt_set_chunk_center.tsv]
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
    std::string casesPath = "mcpp/build/pkt_set_chunk_center.tsv";
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

        // ENC <name> <x> <z> <readableBytes> <hexBytes>
        std::string name, xStr, zStr, rdStr, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, xStr, '\t')
            || !std::getline(ss, zStr, '\t') || !std::getline(ss, rdStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t x = (int32_t)std::stoll(xStr);
        int32_t z = (int32_t)std::stoll(zStr);
        size_t expReadable = (size_t)std::stoull(rdStr);

        // ENCODE: write(): writeVarInt(x), writeVarInt(z) in that exact order.
        PacketBuffer enc;
        enc.writeVarInt(x);
        enc.writeVarInt(z);
        std::string got = hex(enc.data());
        if (got != expHex || enc.size() != expReadable) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH x=" << x << " z=" << z
                      << "\n  got  " << got << " (" << enc.size() << "B)"
                      << "\n  want " << expHex << " (" << expReadable << "B)\n";
            continue;
        }

        // DECODE: read the expected bytes back through PacketBuffer (readVarInt
        // twice) and require the fields round-trip exactly.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotX = dec.readVarInt();
        int32_t gotZ = dec.readVarInt();
        if (gotX != x || gotZ != z || dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex
                      << " got=(" << gotX << "," << gotZ << ")"
                      << " want=(" << x << "," << z << ")"
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktSetChunkCenterParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

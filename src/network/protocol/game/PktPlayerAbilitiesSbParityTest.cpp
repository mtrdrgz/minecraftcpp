// Parity gate for net.minecraft.network.protocol.game.ServerboundPlayerAbilitiesPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktPlayerAbilitiesSbParity.java GT).
//
// Real 26.1.2 source (ServerboundPlayerAbilitiesPacket.java 13-32):
//   FLAG_FLYING = 2
//   write : byte bitfield = 0; if (isFlying) bitfield |= 2; output.writeByte(bitfield)
//   read  : byte bitfield = input.readByte(); isFlying = (bitfield & 2) != 0
// STREAM_CODEC = Packet.codec(write, new) -> StreamCodec.ofMember: NO packet-id prefix,
// just the body. The whole wire payload is exactly ONE byte:
//   isFlying==false -> 0x00,  isFlying==true -> 0x02.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
//   ENC: writeByte(isFlying ? 2 : 0) must equal the GT hex byte-for-byte (+ readableBytes).
//   DEC: readByte() then (b & 2) != 0 must equal the GT-recovered isFlying for EVERY
//        input byte 0..255.
//
//   pkt_player_abilities_sb_parity [--cases mcpp/build/pkt_player_abilities_sb.tsv]
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

// FLAG_FLYING, verbatim from ServerboundPlayerAbilitiesPacket.java line 13.
constexpr int FLAG_FLYING = 2;
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_player_abilities_sb.tsv";
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

        if (tag == "ENC") {
            // ENC <name-hex> <isFlying-dec> <readableBytes-dec> <hex>
            std::string nameHex, flyStr, readableStr, expHex;
            if (!std::getline(ss, nameHex, '\t') || !std::getline(ss, flyStr, '\t')
                || !std::getline(ss, readableStr, '\t') || !std::getline(ss, expHex))
                continue;
            ++cases;
            bool isFlying = std::stoi(flyStr) != 0;
            int expReadable = std::stoi(readableStr);

            // write(): byte bitfield = 0; if (isFlying) bitfield |= 2; writeByte(bitfield).
            PacketBuffer enc;
            uint8_t bitfield = 0;
            if (isFlying) bitfield = (uint8_t)(bitfield | FLAG_FLYING);
            enc.writeByte(bitfield);

            std::string got = hex(enc.data());
            int gotReadable = (int)enc.data().size();
            if (got != expHex || gotReadable != expReadable) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH flying=" << isFlying
                          << "\n  got  " << got << " (" << gotReadable << "B)"
                          << "\n  want " << expHex << " (" << expReadable << "B)\n";
            }

            // Round-trip: decode our own bytes back through PacketBuffer and check isFlying.
            PacketBuffer rt(enc.data());
            uint8_t rtByte = rt.readByte();
            bool rtFlying = (rtByte & FLAG_FLYING) != 0;
            if (rtFlying != isFlying) {
                ++mismatches;
                std::cerr << "ENC-ROUNDTRIP-MISMATCH flying=" << isFlying
                          << " decoded=" << rtFlying << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <inHex> <isFlying-out-dec>
            std::string inHex, flyOutStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, flyOutStr))
                continue;
            ++cases;
            bool expFlying = std::stoi(flyOutStr) != 0;

            // read(): byte bitfield = input.readByte(); isFlying = (bitfield & 2) != 0.
            std::vector<uint8_t> bytes = unhex(inHex);
            PacketBuffer dec(bytes);
            uint8_t bitfield = dec.readByte();
            bool gotFlying = (bitfield & FLAG_FLYING) != 0;
            if (gotFlying != expFlying) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH in=" << inHex << " got=" << gotFlying
                          << " want=" << expFlying << "\n";
            }
        }
    }

    std::cout << "PktPlayerAbilitiesSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

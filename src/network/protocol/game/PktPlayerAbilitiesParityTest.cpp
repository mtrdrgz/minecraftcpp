// Parity gate for net.minecraft.network.protocol.game.ClientboundPlayerAbilitiesPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktPlayerAbilitiesParity.java GT).
//
// The packet body is exactly (real 26.1.2 source, ClientboundPlayerAbilitiesPacket
// lines 13-22, 43-64):
//   FLAG_INVULNERABLE=1, FLAG_FLYING=2, FLAG_CAN_FLY=4, FLAG_INSTABUILD=8
//   write : bitfield = (invuln?1:0)|(flying?2:0)|(canFly?4:0)|(instabuild?8:0)
//           writeByte(bitfield)                 // single raw byte
//           writeFloat(flyingSpeed)             // raw int bits, big-endian
//           writeFloat(walkingSpeed)            // raw int bits, big-endian
//   read  : bitfield = readByte();
//           invuln=(bitfield&1)!=0; flying=(bitfield&2)!=0;
//           canFly=(bitfield&4)!=0; instabuild=(bitfield&8)!=0;
//           flyingSpeed=readFloat(); walkingSpeed=readFloat();
// STREAM_CODEC = Packet.codec(write, new) -> StreamCodec.ofMember: NO packet-id prefix,
// just the body. So the wire payload is: 1B bitfield + 4B float + 4B float = 9 bytes.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly: writeByte
// (single raw byte) and writeFloat (memcpy raw bits -> big-endian uint32) are byte-for-
// byte the same as the real codec. For each ENC row we:
//   (1) ENCODE: pack the four flag bits into a byte EXACTLY as the Java write() does,
//       writeByte(bitfield); writeFloat(flyingSpeed); writeFloat(walkingSpeed);
//       require bytes == Java hex AND byte count == readableBytes.
//   (2) DECODE: read the Java bytes back via readByte/readFloat/readFloat, unpack the
//       four flag bits, and require exact flags + bit-exact float recovery + no trailing.
//
//   pkt_player_abilities_parity [--cases mcpp/build/pkt_player_abilities.tsv]
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
    std::string casesPath = "build/pkt_player_abilities.tsv";
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

        // ENC <name-hex> <invuln> <flying> <canfly> <instabuild>
        //     <flyBits-08x> <walkBits-08x> <readableBytes-dec> <hex>
        std::string nameStr, invulnStr, flyingStr, canflyStr, instaStr,
                    flyBitsStr, walkBitsStr, lenStr, expHex;
        if (!std::getline(ss, nameStr, '\t') || !std::getline(ss, invulnStr, '\t')
            || !std::getline(ss, flyingStr, '\t') || !std::getline(ss, canflyStr, '\t')
            || !std::getline(ss, instaStr, '\t') || !std::getline(ss, flyBitsStr, '\t')
            || !std::getline(ss, walkBitsStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;

        bool invuln = (std::stoi(invulnStr) != 0);
        bool flying = (std::stoi(flyingStr) != 0);
        bool canFly = (std::stoi(canflyStr) != 0);
        bool insta  = (std::stoi(instaStr) != 0);
        uint32_t flyBits  = (uint32_t)std::stoul(flyBitsStr, nullptr, 16);
        uint32_t walkBits = (uint32_t)std::stoul(walkBitsStr, nullptr, 16);
        size_t   expLen   = (size_t)std::stoull(lenStr);

        float flySpeed  = bitsToFloat(flyBits);
        float walkSpeed = bitsToFloat(walkBits);

        // (1) ENCODE: pack flag bits EXACTLY as Java write(), then byte + 2 floats.
        uint8_t bitfield = 0;
        if (invuln) bitfield = (uint8_t)(bitfield | 1);
        if (flying) bitfield = (uint8_t)(bitfield | 2);
        if (canFly) bitfield = (uint8_t)(bitfield | 4);
        if (insta)  bitfield = (uint8_t)(bitfield | 8);

        PacketBuffer enc;
        enc.writeByte(bitfield);
        enc.writeFloat(flySpeed);
        enc.writeFloat(walkSpeed);

        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH name=" << nameStr << " bitfield=" << (int)bitfield
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH name=" << nameStr << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // (2) DECODE: readByte()/readFloat()/readFloat() recover the exact fields.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        uint8_t gotBitfield = dec.readByte();
        bool gotInvuln = (gotBitfield & 1) != 0;
        bool gotFlying = (gotBitfield & 2) != 0;
        bool gotCanFly = (gotBitfield & 4) != 0;
        bool gotInsta  = (gotBitfield & 8) != 0;
        float gotFly  = dec.readFloat();
        float gotWalk = dec.readFloat();

        if (gotInvuln != invuln || gotFlying != flying
            || gotCanFly != canFly || gotInsta != insta) {
            ++mismatches;
            std::cerr << "DEC-FLAG-MISMATCH hex=" << expHex
                      << " gotBitfield=" << (int)gotBitfield
                      << " want(" << invuln << "," << flying << "," << canFly << ","
                      << insta << ")\n";
        }
        if (std::bit_cast<uint32_t>(gotFly) != flyBits) {
            ++mismatches;
            std::cerr << "DEC-FLY-MISMATCH hex=" << expHex
                      << " got=" << std::bit_cast<uint32_t>(gotFly)
                      << " want=" << flyBits << "\n";
        }
        if (std::bit_cast<uint32_t>(gotWalk) != walkBits) {
            ++mismatches;
            std::cerr << "DEC-WALK-MISMATCH hex=" << expHex
                      << " got=" << std::bit_cast<uint32_t>(gotWalk)
                      << " want=" << walkBits << "\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING name=" << nameStr << " remaining="
                      << dec.remaining() << "\n";
        }
    }

    std::cout << "PktPlayerAbilitiesParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

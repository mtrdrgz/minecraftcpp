// Parity gate for net.minecraft.network.protocol.game.ClientboundHurtAnimationPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktHurtAnimationParity.java GT).
//
// The packet is a record(int id, float yaw) and its body is exactly
// (ClientboundHurtAnimationPacket.java:18-25):
//   write : FriendlyByteBuf.writeVarInt(this.id)     // entity id (VarInt / LEB128)
//           FriendlyByteBuf.writeFloat(this.yaw)     // big-endian IEEE-754 4 bytes
//   read  : this.id  = input.readVarInt();
//           this.yaw = input.readFloat();
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.
//
// Both fields are plain numbers, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeVarInt/readVarInt (LEB128) and writeFloat/
// readFloat (big-endian IEEE-754 bit-cast) are byte-for-byte / bit-for-bit identical
// to the real codec. yaw is carried through the TSV as its raw int bits (%08x) so
// NaN/Inf/-0.0 are exercised exactly without float-parse rounding.
//
//   pkt_hurt_animation_parity [--cases mcpp/build/pkt_hurt_animation.tsv]
#include "../../PacketBuffer.h"

#include <cstdint>
#include <cstring>
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

// Reinterpret raw 32-bit IEEE-754 bits as a float (matches Float.intBitsToFloat).
float bitsToFloat(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

// Reinterpret a float as its raw 32-bit IEEE-754 bits (matches Float.floatToRawIntBits).
uint32_t floatToBits(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    return bits;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_hurt_animation.tsv";
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
            // ENC <id> <yawRawBitsHex> <readableBytes> <hexBytes>
            std::string idStr, yawStr, nStr, expHex;
            if (!std::getline(ss, idStr, '\t') || !std::getline(ss, yawStr, '\t')
                || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t id = (int32_t)std::stoll(idStr);
            uint32_t yawBits = (uint32_t)std::stoul(yawStr, nullptr, 16);
            float yaw = bitsToFloat(yawBits);
            size_t expN = (size_t)std::stoull(nStr);

            // write(): writeVarInt(id) then writeFloat(yaw).
            PacketBuffer enc;
            enc.writeVarInt(id);
            enc.writeFloat(yaw);

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH id=" << id << " yawBits=" << yawStr
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <id_in> <yawRawBitsHex_in> <id_decoded> <yawRawBitsHex_decoded>
            std::string inHex, idStr, yawStr, decIdStr, decYawStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, idStr, '\t')
                || !std::getline(ss, yawStr, '\t') || !std::getline(ss, decIdStr, '\t')
                || !std::getline(ss, decYawStr)) continue;
            ++cases;
            int32_t expId = (int32_t)std::stoll(decIdStr);
            uint32_t expYawBits = (uint32_t)std::stoul(decYawStr, nullptr, 16);

            // read(): readVarInt() then readFloat().
            PacketBuffer dec(unhex(inHex));
            int32_t gotId = dec.readVarInt();
            float gotYaw = dec.readFloat();
            uint32_t gotYawBits = floatToBits(gotYaw);
            if (gotId != expId || gotYawBits != expYawBits) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex
                          << " got id=" << gotId << " yawBits=" << std::hex << gotYawBits
                          << " want id=" << std::dec << expId << " yawBits=" << yawStr
                          << "\n";
            }
        }
    }

    std::cout << "PktHurtAnimationParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

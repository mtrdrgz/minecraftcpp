// Parity gate for net.minecraft.network.protocol.game.ClientboundProjectilePowerPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktProjectilePowerParity.java GT).
//
// The packet holds (int id, double accelerationPower) and its STREAM_CODEC is
// Packet.codec(write, new); write(FriendlyByteBuf) is, in this exact wire order
// (ClientboundProjectilePowerPacket.java:25-28):
//   output.writeVarInt(this.id);                 -> VarInt (LEB128) id
//   output.writeDouble(this.accelerationPower);  -> big-endian IEEE-754 8-byte double
// and the decode ctor reads readVarInt() then readDouble() in that same order
// (ClientboundProjectilePowerPacket.java:20-23). Packet.codec -> no packet-id prefix,
// just the body.
//
// Both fields are plain primitives, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeVarInt/readVarInt (LEB128) and writeDouble/
// readDouble (big-endian IEEE-754 bit-cast) are byte-for-byte / bit-for-bit identical to
// the real codec. accelerationPower is carried through the TSV as its raw long bits
// (%016x) so NaN/Inf/-0.0 are exercised exactly without parse rounding.
//
//   pkt_projectile_power_parity [--cases mcpp/build/pkt_projectile_power.tsv]
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

// Reinterpret raw 64-bit IEEE-754 bits as a double (matches Double.longBitsToDouble).
double bitsToDouble(uint64_t bits) {
    double d;
    std::memcpy(&d, &bits, 8);
    return d;
}
uint64_t doubleToBits(double d) {
    uint64_t bits;
    std::memcpy(&bits, &d, 8);
    return bits;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_projectile_power.tsv";
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

        // ENC <id> <powBits> <readableBytes> <hexBytes>
        std::string idS, powS, nS, expHex;
        if (!std::getline(ss, idS, '\t') || !std::getline(ss, powS, '\t')
            || !std::getline(ss, nS, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;
        int32_t id = (int32_t)std::stoll(idS);
        uint64_t powBits = (uint64_t)std::stoull(powS, nullptr, 16);
        double power = bitsToDouble(powBits);
        size_t expN = (size_t)std::stoull(nS);

        // write(): the codec writes, in order, writeVarInt(id) then writeDouble(power).
        PacketBuffer enc;
        enc.writeVarInt(id);
        enc.writeDouble(power);

        std::string got = hex(enc.data());
        if (got != expHex || enc.size() != expN) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH id=" << idS << " pow=" << powS
                      << "\n  got  n=" << enc.size() << " " << got
                      << "\n  want n=" << expN << " " << expHex << "\n";
            continue;
        }

        // Round-trip: decode the expected bytes back through PacketBuffer in the same
        // order (readVarInt then readDouble) and require the fields survive exactly with
        // no trailing bytes.
        PacketBuffer dec(unhex(expHex));
        int32_t gid = dec.readVarInt();
        double gpow = dec.readDouble();
        uint64_t gPowBits = doubleToBits(gpow);
        if (gid != id || gPowBits != powBits || dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH id=" << idS << " pow=" << powS
                      << " got id=" << gid << " powBits=" << std::hex << gPowBits
                      << std::dec << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktProjectilePowerParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

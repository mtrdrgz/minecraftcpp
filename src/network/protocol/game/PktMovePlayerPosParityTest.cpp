// Parity gate for net.minecraft.network.protocol.game.ServerboundMovePlayerPacket.Pos's
// StreamCodec vs the REAL net.minecraft codec (tools/PktMovePlayerPosParity.java GT).
//
// ServerboundMovePlayerPacket.Pos (ServerboundMovePlayerPacket.java:108-142) carries:
//   double x, double y, double z, boolean onGround, boolean horizontalCollision.
// Its STREAM_CODEC is Packet.codec(Pos::write, Pos::read). write()
// (ServerboundMovePlayerPacket.java:131-136) emits, IN THIS EXACT ORDER:
//   writeDouble(x) ++ writeDouble(y) ++ writeDouble(z) ++ writeByte(flags)
// where flags = (onGround?1:0) | (horizontalCollision?2:0) -- packFlags
// (ServerboundMovePlayerPacket.java:22-33). On-wire body: 3 big-endian IEEE-754
// doubles (24 bytes) + 1 flags byte = 25 bytes, NO packet-id prefix.
//
// Every field is a plain primitive, so this exercises the certified PacketBuffer
// (the FriendlyByteBuf port) directly: writeDouble/readDouble (big-endian IEEE-754
// bit-cast) and writeByte/readByte are byte-for-byte / bit-for-bit identical to the
// real codec. x/y/z are carried through the TSV as their raw long bits (%016x) so
// NaN/Inf/-0.0 are exercised exactly without double-parse rounding.
//
//   pkt_move_player_pos_parity [--cases mcpp/build/pkt_move_player_pos.tsv]
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

// Reinterpret a double as its raw 64-bit IEEE-754 bits (matches Double.doubleToRawLongBits).
uint64_t doubleToBits(double d) {
    uint64_t bits;
    std::memcpy(&bits, &d, 8);
    return bits;
}

// packFlags: (onGround?1:0) | (horizontalCollision?2:0). See ServerboundMovePlayerPacket.java:22-33.
uint8_t packFlags(bool onGround, bool horizontalCollision) {
    uint8_t flags = 0;
    if (onGround) flags |= 1;
    if (horizontalCollision) flags |= 2;
    return flags;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_move_player_pos.tsv";
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
            // ENC <xBits> <yBits> <zBits> <onGround> <horizColl> <readableBytes> <hexBytes>
            std::string xStr, yStr, zStr, ogStr, hcStr, nStr, expHex;
            if (!std::getline(ss, xStr, '\t') || !std::getline(ss, yStr, '\t')
                || !std::getline(ss, zStr, '\t') || !std::getline(ss, ogStr, '\t')
                || !std::getline(ss, hcStr, '\t') || !std::getline(ss, nStr, '\t')
                || !std::getline(ss, expHex)) continue;
            ++cases;
            double x = bitsToDouble((uint64_t)std::stoull(xStr, nullptr, 16));
            double y = bitsToDouble((uint64_t)std::stoull(yStr, nullptr, 16));
            double z = bitsToDouble((uint64_t)std::stoull(zStr, nullptr, 16));
            bool onGround = std::stoi(ogStr) != 0;
            bool horizontalCollision = std::stoi(hcStr) != 0;
            size_t expN = (size_t)std::stoull(nStr);

            // write(): writeDouble(x) ++ writeDouble(y) ++ writeDouble(z)
            //          ++ writeByte(packFlags(onGround, horizontalCollision)) -- EXACT order.
            PacketBuffer enc;
            enc.writeDouble(x);
            enc.writeDouble(y);
            enc.writeDouble(z);
            enc.writeByte(packFlags(onGround, horizontalCollision));

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH xBits=" << xStr << " yBits=" << yStr
                          << " zBits=" << zStr << " onGround=" << ogStr << " hc=" << hcStr
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <xBits_in> <yBits_in> <zBits_in> <og_in> <hc_in>
            //     <xBits_dec> <yBits_dec> <zBits_dec> <og_dec> <hc_dec>
            std::string inHex, xIn, yIn, zIn, ogIn, hcIn, xDec, yDec, zDec, ogDec, hcDec;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, xIn, '\t')
                || !std::getline(ss, yIn, '\t') || !std::getline(ss, zIn, '\t')
                || !std::getline(ss, ogIn, '\t') || !std::getline(ss, hcIn, '\t')
                || !std::getline(ss, xDec, '\t') || !std::getline(ss, yDec, '\t')
                || !std::getline(ss, zDec, '\t') || !std::getline(ss, ogDec, '\t')
                || !std::getline(ss, hcDec)) continue;
            ++cases;
            uint64_t expXBits = (uint64_t)std::stoull(xDec, nullptr, 16);
            uint64_t expYBits = (uint64_t)std::stoull(yDec, nullptr, 16);
            uint64_t expZBits = (uint64_t)std::stoull(zDec, nullptr, 16);
            bool expOg = std::stoi(ogDec) != 0;
            bool expHc = std::stoi(hcDec) != 0;

            // read(): readDouble() ++ readDouble() ++ readDouble() ++ readUnsignedByte()
            //         then unpackOnGround(flags) = (flags&1)!=0,
            //              unpackHorizontalCollision(flags) = (flags&2)!=0.
            PacketBuffer dec(unhex(inHex));
            uint64_t gotXBits = doubleToBits(dec.readDouble());
            uint64_t gotYBits = doubleToBits(dec.readDouble());
            uint64_t gotZBits = doubleToBits(dec.readDouble());
            uint8_t flags = dec.readByte();
            bool gotOg = (flags & 1) != 0;
            bool gotHc = (flags & 2) != 0;
            if (gotXBits != expXBits || gotYBits != expYBits || gotZBits != expZBits
                || gotOg != expOg || gotHc != expHc) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex << std::hex
                          << " got xBits=" << gotXBits << " yBits=" << gotYBits
                          << " zBits=" << gotZBits << std::dec
                          << " og=" << gotOg << " hc=" << gotHc
                          << " want xBits=" << xDec << " yBits=" << yDec
                          << " zBits=" << zDec << " og=" << ogDec << " hc=" << hcDec << "\n";
            }
        }
    }

    std::cout << "PktMovePlayerPosParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

// Parity gate for net.minecraft.network.protocol.game.ClientboundSetBorderCenterPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktSetBorderCenterParity.java GT).
//
// ClientboundSetBorderCenterPacket (ClientboundSetBorderCenterPacket.java:9-47) carries:
//   private final double newCenterX;
//   private final double newCenterZ;
// Its STREAM_CODEC is Packet.codec(write, new) -- a plain body codec, no packet-id prefix.
// write() (ClientboundSetBorderCenterPacket.java:26-29) emits, IN THIS EXACT ORDER:
//   writeDouble(newCenterX) ++ writeDouble(newCenterZ)
// and the read ctor (ClientboundSetBorderCenterPacket.java:21-24) reads in the same order:
//   newCenterX = readDouble(); newCenterZ = readDouble();
// On-wire body: 2 big-endian IEEE-754 doubles = 16 bytes, NO packet-id prefix.
//
// Both fields are plain primitives, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeDouble/readDouble (big-endian IEEE-754 bit-cast)
// are byte-for-byte / bit-for-bit identical to the real codec. newCenterX / newCenterZ
// are carried through the TSV as their raw long bits (%016x) so NaN/Inf/-0.0 are
// exercised exactly without double-parse rounding.
//
//   pkt_set_border_center_parity [--cases mcpp/build/pkt_set_border_center.tsv]
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
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_set_border_center.tsv";
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
            // ENC <xBits> <zBits> <readableBytes> <hexBytes>
            std::string xStr, zStr, nStr, expHex;
            if (!std::getline(ss, xStr, '\t') || !std::getline(ss, zStr, '\t')
                || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            double x = bitsToDouble((uint64_t)std::stoull(xStr, nullptr, 16));
            double z = bitsToDouble((uint64_t)std::stoull(zStr, nullptr, 16));
            size_t expN = (size_t)std::stoull(nStr);

            // write(): writeDouble(newCenterX) ++ writeDouble(newCenterZ) -- EXACT order.
            PacketBuffer enc;
            enc.writeDouble(x);
            enc.writeDouble(z);

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH xBits=" << xStr << " zBits=" << zStr
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <xBits_in> <zBits_in> <xBits_dec> <zBits_dec>
            std::string inHex, xIn, zIn, xDec, zDec;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, xIn, '\t')
                || !std::getline(ss, zIn, '\t') || !std::getline(ss, xDec, '\t')
                || !std::getline(ss, zDec)) continue;
            ++cases;
            uint64_t expXBits = (uint64_t)std::stoull(xDec, nullptr, 16);
            uint64_t expZBits = (uint64_t)std::stoull(zDec, nullptr, 16);

            // read(): readDouble() ++ readDouble() -- EXACT order.
            PacketBuffer dec(unhex(inHex));
            uint64_t gotXBits = doubleToBits(dec.readDouble());
            uint64_t gotZBits = doubleToBits(dec.readDouble());
            if (gotXBits != expXBits || gotZBits != expZBits) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex << std::hex
                          << " got xBits=" << gotXBits << " zBits=" << gotZBits << std::dec
                          << " want xBits=" << xDec << " zBits=" << zDec << "\n";
            }
        }
    }

    std::cout << "PktSetBorderCenterParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

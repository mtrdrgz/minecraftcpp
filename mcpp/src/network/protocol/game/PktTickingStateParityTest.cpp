// Parity gate for net.minecraft.network.protocol.game.ClientboundTickingStatePacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktTickingStateParity.java GT).
//
// The packet is a record(float tickRate, boolean isFrozen) and its body is exactly
// (ClientboundTickingStatePacket.java:22-25):
//   write : FriendlyByteBuf.writeFloat(this.tickRate)    // big-endian IEEE-754 4 bytes
//           FriendlyByteBuf.writeBoolean(this.isFrozen)  // 1 byte: 0x00 / 0x01
//   read  : this.tickRate = input.readFloat();
//           this.isFrozen = input.readBoolean();
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.
//
// Both fields are plain numbers, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeFloat/readFloat (big-endian IEEE-754 bit-cast)
// and writeBool/readBool (single 0/1 byte) are byte-for-byte / bit-for-bit identical to
// the real codec. tickRate is carried through the TSV as its raw int bits (%08x) so
// NaN/Inf/-0.0 are exercised exactly without float-parse rounding.
//
//   pkt_ticking_state_parity [--cases mcpp/build/pkt_ticking_state.tsv]
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
    std::string casesPath = "mcpp/build/pkt_ticking_state.tsv";
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
            // ENC <tickRateRawBitsHex> <isFrozen0or1> <readableBytes> <hexBytes>
            std::string rateStr, frozenStr, nStr, expHex;
            if (!std::getline(ss, rateStr, '\t') || !std::getline(ss, frozenStr, '\t')
                || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            uint32_t rateBits = (uint32_t)std::stoul(rateStr, nullptr, 16);
            float tickRate = bitsToFloat(rateBits);
            bool isFrozen = (frozenStr == "1");
            size_t expN = (size_t)std::stoull(nStr);

            // write(): writeFloat(tickRate) then writeBoolean(isFrozen).
            PacketBuffer enc;
            enc.writeFloat(tickRate);
            enc.writeBool(isFrozen);

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH rateBits=" << rateStr << " frozen=" << frozenStr
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <tickRateRawBitsHex_in> <isFrozen_in> <tickRateRawBitsHex_decoded> <isFrozen_decoded>
            std::string inHex, rateStr, frozenStr, decRateStr, decFrozenStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, rateStr, '\t')
                || !std::getline(ss, frozenStr, '\t') || !std::getline(ss, decRateStr, '\t')
                || !std::getline(ss, decFrozenStr)) continue;
            ++cases;
            uint32_t expRateBits = (uint32_t)std::stoul(decRateStr, nullptr, 16);
            bool expFrozen = (decFrozenStr == "1");

            // read(): readFloat() then readBoolean().
            PacketBuffer dec(unhex(inHex));
            float gotRate = dec.readFloat();
            bool gotFrozen = dec.readBool();
            uint32_t gotRateBits = floatToBits(gotRate);
            if (gotRateBits != expRateBits || gotFrozen != expFrozen) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex
                          << " got rateBits=" << std::hex << gotRateBits << std::dec
                          << " frozen=" << (gotFrozen ? 1 : 0)
                          << " want rateBits=" << decRateStr
                          << " frozen=" << decFrozenStr << "\n";
            }
        }
    }

    std::cout << "PktTickingStateParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

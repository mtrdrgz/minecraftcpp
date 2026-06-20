// Parity gate for net.minecraft.network.protocol.game.ServerboundChunkBatchReceivedPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktChunkBatchReceivedSbParity.java GT).
//
// The packet is a record holding (float desiredChunksPerTick) and its STREAM_CODEC is
// Packet.codec(write, new); write(FriendlyByteBuf) is, in this exact wire order
// (ServerboundChunkBatchReceivedPacket.java:17-19):
//   output.writeFloat(this.desiredChunksPerTick); -> big-endian IEEE-754 4-byte float
// and the decode ctor reads readFloat() (ServerboundChunkBatchReceivedPacket.java:13-15).
// Packet.codec -> no packet-id prefix, just the body.
//
// The only field is a plain float, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeFloat/readFloat (big-endian IEEE-754 bit-cast) are
// byte-for-byte / bit-for-bit identical to the real codec. desiredChunksPerTick is carried
// through the TSV as its raw int bits (%08x) so NaN/Inf/-0.0 are exercised exactly without
// parse rounding.
//
//   pkt_chunk_batch_received_sb_parity [--cases mcpp/build/pkt_chunk_batch_received_sb.tsv]
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
uint32_t floatToBits(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    return bits;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_chunk_batch_received_sb.tsv";
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

        // ENC <chunksBits> <readableBytes> <hexBytes>
        std::string rateS, nS, expHex;
        if (!std::getline(ss, rateS, '\t') || !std::getline(ss, nS, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;
        uint32_t rateBits = (uint32_t)std::stoul(rateS, nullptr, 16);
        float rate = bitsToFloat(rateBits);
        size_t expN = (size_t)std::stoull(nS);

        // write(): the codec writes writeFloat(desiredChunksPerTick).
        PacketBuffer enc;
        enc.writeFloat(rate);

        std::string got = hex(enc.data());
        if (got != expHex || enc.size() != expN) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH rate=" << rateS
                      << "\n  got  n=" << enc.size() << " " << got
                      << "\n  want n=" << expN << " " << expHex << "\n";
            continue;
        }

        // Round-trip: decode the expected bytes back through PacketBuffer (readFloat) and
        // require the field survives exactly with no trailing bytes.
        PacketBuffer dec(unhex(expHex));
        float grate = dec.readFloat();
        uint32_t gRateBits = floatToBits(grate);
        if (gRateBits != rateBits || dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH rate=" << rateS
                      << " got rateBits=" << std::hex << gRateBits << std::dec
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktChunkBatchReceivedSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

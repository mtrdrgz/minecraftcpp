// Parity gate for net.minecraft.network.protocol.game.ClientboundForgetLevelChunkPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktForgetLevelChunkParity.java GT).
//
// The packet body is exactly one ChunkPos (ClientboundForgetLevelChunkPacket lines
// 9-20; Packet.codec -> StreamCodec.ofMember: body only, no packet-id prefix):
//   write : output.writeChunkPos(pos)  == writeLong(pos.pack())
//   read  : pos = input.readChunkPos() == ChunkPos.unpack(readLong())
//
// ChunkPos.pack(x,z) = (x & 0xFFFFFFFFL) | ((z & 0xFFFFFFFFL) << 32)  (ChunkPos.java
// 76-78): low 32 bits are x (unsigned), high 32 bits are z (unsigned). writeLong is
// big-endian, so the 8 wire bytes are z (4 bytes BE) then x (4 bytes BE).
// ChunkPos.unpack(key): x = (int)key, z = (int)(key >> 32) -- a plain narrowing cast,
// so the sign bits round-trip and negative coords are exact.
//
// This reuses the certified PacketBuffer (FriendlyByteBuf port): writeLong/readLong
// are byte-for-byte the net.minecraft big-endian long codec. We rebuild the packed
// key the same way the Java does and writeLong it, requiring the emitted bytes (hex)
// and readableBytes match the ground truth, then decode the bytes back via readLong
// + unpack and require (x,z) round-trip exactly.
//
//   pkt_forget_level_chunk_parity [--cases mcpp/build/pkt_forget_level_chunk.tsv]
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
    s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    out.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}

// ChunkPos.pack(x, z): (x & 0xFFFFFFFFL) | ((z & 0xFFFFFFFFL) << 32).
int64_t chunkPack(int32_t x, int32_t z) {
    return (int64_t)((uint64_t)(uint32_t)x | ((uint64_t)(uint32_t)z << 32));
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_forget_level_chunk.tsv";
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

        // ENC <x> <z> <readableBytes> <hexBytes>
        std::string xStr, zStr, rdStr, expHex;
        if (!std::getline(ss, xStr, '\t') || !std::getline(ss, zStr, '\t')
            || !std::getline(ss, rdStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t x = (int32_t)std::stoll(xStr);
        int32_t z = (int32_t)std::stoll(zStr);
        size_t expReadable = (size_t)std::stoull(rdStr);

        // ENCODE: writeChunkPos == writeLong(pack(x, z)).
        PacketBuffer enc;
        enc.writeLong(chunkPack(x, z));
        std::string got = hex(enc.data());
        if (got != expHex || enc.size() != expReadable) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH x=" << x << " z=" << z
                      << "\n  got  " << got << " (" << enc.size() << "B)"
                      << "\n  want " << expHex << " (" << expReadable << "B)\n";
            continue;
        }

        // DECODE: readChunkPos == unpack(readLong()): x=(int)key, z=(int)(key>>32).
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int64_t key = dec.readLong();
        int32_t gotX = (int32_t)key;
        int32_t gotZ = (int32_t)(key >> 32);
        if (gotX != x || gotZ != z || dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex
                      << " got=(" << gotX << "," << gotZ << ")"
                      << " want=(" << x << "," << z << ")"
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktForgetLevelChunkParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

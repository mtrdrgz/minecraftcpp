// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundChunksBiomesPacket
// vs the REAL ClientboundChunksBiomesPacket.STREAM_CODEC (tools/PktChunksBiomesParity.java).
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src):
//   record ClientboundChunksBiomesPacket(List<ChunkBiomeData> chunkBiomeData)
//   STREAM_CODEC = Packet.codec(write, ::new)  over a PLAIN FriendlyByteBuf
//        -> body only, NO packet-id prefix, NO registry state.
//   write (ClientboundChunksBiomesPacket.java:28-30):
//        output.writeCollection(this.chunkBiomeData, (o, c) -> c.write(o));
//   writeCollection == VarInt.write(size) + per element ChunkBiomeData.write(o).
//   ChunkBiomeData.write (lines 81-84):
//        output.writeChunkPos(this.pos);     == writeLong(pos.pack())    (8 BE bytes)
//        output.writeByteArray(this.buffer); == VarInt(len) + raw bytes
//   ChunkPos.pack(x,z) = (x & 0xFFFFFFFFL) | ((z & 0xFFFFFFFFL) << 32)  (ChunkPos.java:72-78)
//        -> x in the low 32 bits, z in the high 32 bits; carried here as the raw
//        signed 64-bit long, so the C++ side only needs writeLong(posLong).
//
//   So the body is exactly:
//       VarInt(size)
//       for each entry (list order):
//           long  pos.pack()                 (writeLong, 8 BE bytes)
//           VarInt(bufferLen)  bufferBytes    (raw)
//
//   pkt_chunks_biomes_cb_parity [--cases mcpp/build/pkt_chunks_biomes.tsv]
//
// Row: ENC <name> <size> [<posLong-dec> <bufHex-or-_>]*size <bytesN> <hexBytes>
//      posLong = pos.pack() (signed 64-bit decimal); bufHex is the raw buffer bytes
//      as lowercase hex, with the token "_" meaning a zero-length buffer.
#include "../../PacketBuffer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
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
    std::vector<uint8_t> v;
    v.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((uint8_t)std::stoul(s.substr(i, 2), nullptr, 16));
    return v;
}

// Decode a raw-hex buffer field, with "_" meaning the empty buffer.
std::vector<uint8_t> fieldToBytes(const std::string& tok) {
    if (tok == "_") return std::vector<uint8_t>();
    return unhex(tok);
}

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_chunks_biomes.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> col = splitTabs(line);
        // Layout: ENC name size [posLong buf]*size bytesN hexBytes
        //         0    1    2    3 .. 3+2*size-1       -2     -1
        if (col.size() < 5) continue;
        if (col[0] != "ENC") continue;

        const std::string& name = col[1];
        int size = std::stoi(col[2]);

        size_t need = 3 + (size_t)2 * (size_t)size + 2; // entries + bytesN + hexBytes
        if (col.size() != need) {
            ++mismatches;
            std::cerr << "FIELD-COUNT-MISMATCH " << name << " cols=" << col.size()
                      << " want=" << need << " (size=" << size << ")\n";
            continue;
        }
        ++cases;

        std::vector<std::pair<int64_t, std::vector<uint8_t>>> entries;
        entries.reserve((size_t)size);
        for (int e = 0; e < size; ++e) {
            const std::string& posTok = col[3 + 2 * (size_t)e];
            const std::string& bufTok = col[3 + 2 * (size_t)e + 1];
            int64_t posLong = (int64_t)std::stoll(posTok);
            entries.emplace_back(posLong, fieldToBytes(bufTok));
        }

        size_t expBytes = (size_t)std::stoul(col[col.size() - 2]);
        const std::string& expHex = col[col.size() - 1];

        // (1) ENCODE the body through PacketBuffer in EXACT field order:
        //     writeVarInt(size) then per entry writeLong(posLong) +
        //     writeByteArray(buffer) == writeVarInt(len) + raw bytes.
        PacketBuffer enc;
        enc.writeVarInt((int32_t)size);
        for (const auto& [posLong, buf] : entries) {
            enc.writeLong(posLong);
            enc.writeVarInt((int32_t)buf.size());
            enc.writeBytes(buf);
        }

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE the Java bytes back through PacketBuffer: VarInt(size) then size
        //     (readLong pos, readVarInt len + readBytes(len)) tuples must reproduce
        //     our entries with nothing left over.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t bSize = dec.readVarInt();
        bool ok = (bSize == size);
        if (ok) {
            for (int e = 0; e < size && ok; ++e) {
                int64_t bPos = dec.readLong();
                int32_t bLen = dec.readVarInt();
                std::vector<uint8_t> bBuf = dec.readBytes((size_t)bLen);
                ok = ok && (bPos == entries[(size_t)e].first)
                        && (bBuf == entries[(size_t)e].second);
            }
        }
        ok = ok && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " size(got=" << bSize << " want=" << size << ")"
                      << " remaining=" << dec.remaining() << "\n";
            continue;
        }
    }

    std::cout << "PktChunksBiomesParity checks=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

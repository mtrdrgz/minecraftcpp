// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundSectionBlocksUpdatePacket
// vs the REAL ClientboundSectionBlocksUpdatePacket.STREAM_CODEC (tools/PktSectionBlocksUpdateParity.java).
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src):
//   STREAM_CODEC = Packet.codec(write, ::new)  over a PLAIN FriendlyByteBuf
//        -> body only, NO packet-id prefix, NO registry state on the wire.
//   write (ClientboundSectionBlocksUpdatePacket.java:54-61):
//        SectionPos.STREAM_CODEC.encode(output, sectionPos);   == writeLong(sectionPos.asLong())  (8 BE bytes)
//        output.writeVarInt(positions.length);                 == VarInt(count)
//        for each i: output.writeVarLong((long)Block.getId(states[i]) << 12 | positions[i]);  == VarLong per change
//   SectionPos.STREAM_CODEC (SectionPos.java:34) = ByteBufCodecs.LONG.map(of, asLong)
//   ByteBufCodecs.LONG (ByteBufCodecs.java:114-122) = writeLong / readLong  (8 BE bytes)
//
//   So the body is exactly:
//       long  sectionPosAsLong                      (writeLong, 8 BE bytes)
//       VarInt(count)
//       VarLong(change)  x count                    ((blockStateId << 12) | posInSection)
//
//   pkt_section_blocks_update_cb_parity [--cases mcpp/build/pkt_section_blocks_update.tsv]
//
// Row: ENC <name> <sectionLong-dec> <count-dec> <change0-dec> ... <bytesN-dec> <hexBytes>
//      (variable width: 4 fixed cols + <count> change cols + 2 trailing cols).
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
    std::string casesPath = "build/pkt_section_blocks_update.tsv";
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
        // ENC name sectionLong count [change...] bytesN hexBytes
        // minimum columns (count=0): ENC name sec count bytesN hex -> 6.
        if (col.size() < 6) continue;
        if (col[0] != "ENC") continue;

        int64_t sectionLong = (int64_t)std::stoll(col[2]);
        int64_t count       = (int64_t)std::stoll(col[3]);
        if (count < 0) continue;
        // exact column count: 4 fixed + count changes + 2 trailing.
        if ((int64_t)col.size() != 4 + count + 2) continue;
        ++cases;

        std::vector<int64_t> changes;
        changes.reserve((size_t)count);
        for (int64_t k = 0; k < count; ++k)
            changes.push_back((int64_t)std::stoll(col[(size_t)(4 + k)]));

        size_t expBytes = (size_t)std::stoull(col[(size_t)(4 + count)]);
        const std::string& expHex = col[(size_t)(4 + count + 1)];

        // (1) ENCODE the body through PacketBuffer in EXACT field order:
        //     writeLong(sectionLong), writeVarInt(count), writeVarLong(change) x count.
        PacketBuffer enc;
        enc.writeLong(sectionLong);
        enc.writeVarInt((int32_t)count);
        for (int64_t c : changes) enc.writeVarLong(c);

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << col[1] << " sec=" << sectionLong
                      << " count=" << count
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE the Java bytes back through PacketBuffer and recover every field,
        //     with nothing left over.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int64_t bSec = dec.readLong();
        int32_t bCount = dec.readVarInt();
        bool ok = (bSec == sectionLong) && ((int64_t)bCount == count);
        for (int64_t k = 0; ok && k < count; ++k) {
            int64_t bc = dec.readVarLong();
            if (bc != changes[(size_t)k]) ok = false;
        }
        ok = ok && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << col[1] << " sec(got=" << bSec << " want=" << sectionLong << ")"
                      << " count(got=" << bCount << " want=" << count << ")"
                      << " remaining=" << dec.remaining() << "\n";
            continue;
        }
    }

    std::cout << "PktSectionBlocksUpdateParity checks=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

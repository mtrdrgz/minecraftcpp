// Parity gate for net.minecraft.network.protocol.game.ServerboundEntityTagQueryPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktEntityTagQuerySbParity.java GT).
//
// The packet holds two ints (transactionId, entityId) and its STREAM_CODEC is
// Packet.codec(write, new); write(FriendlyByteBuf) is, in this exact wire order
// (ServerboundEntityTagQueryPacket.java:25-28):
//   output.writeVarInt(this.transactionId);   -> LEB128 VarInt of signed transactionId
//   output.writeVarInt(this.entityId);        -> LEB128 VarInt of signed entityId
// and the decode ctor reads readVarInt() then readVarInt() (lines 20-23). Packet.codec ->
// no packet-id prefix, just the body. writeVarInt does NOT zig-zag, so negatives are 5 bytes.
//
// Both fields are plain VarInts, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeVarInt/readVarInt (LEB128) are byte-for-byte
// identical to the real codec. The fields are carried through the TSV as decimal signed
// ints.
//
//   pkt_entity_tag_query_sb_parity [--cases mcpp/build/pkt_entity_tag_query_sb.tsv]
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
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}

// Parse a signed 32-bit decimal (handles Integer.MIN_VALUE = -2147483648 exactly).
int32_t parseI32(const std::string& s) {
    return (int32_t)std::stoll(s);
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_entity_tag_query_sb.tsv";
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

        // ENC <transactionId> <entityId> <readableBytes> <hexBytes>
        std::string txS, eidS, nS, expHex;
        if (!std::getline(ss, txS, '\t') || !std::getline(ss, eidS, '\t')
            || !std::getline(ss, nS, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;
        int32_t transactionId = parseI32(txS);
        int32_t entityId = parseI32(eidS);
        size_t expN = (size_t)std::stoull(nS);

        // write(): the codec writes writeVarInt(transactionId) then writeVarInt(entityId).
        PacketBuffer enc;
        enc.writeVarInt(transactionId);
        enc.writeVarInt(entityId);

        std::string got = hex(enc.data());
        if (got != expHex || enc.size() != expN) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH tx=" << txS << " eid=" << eidS
                      << "\n  got  n=" << enc.size() << " " << got
                      << "\n  want n=" << expN << " " << expHex << "\n";
            continue;
        }

        // Round-trip: decode the expected bytes back through PacketBuffer (readVarInt twice)
        // and require both fields survive exactly with no trailing bytes.
        PacketBuffer dec(unhex(expHex));
        int32_t gTx = dec.readVarInt();
        int32_t gEid = dec.readVarInt();
        if (gTx != transactionId || gEid != entityId || dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH tx=" << txS << " eid=" << eidS
                      << " got tx=" << gTx << " eid=" << gEid
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktEntityTagQuerySbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

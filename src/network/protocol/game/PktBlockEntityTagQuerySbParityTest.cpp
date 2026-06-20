// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundBlockEntityTagQueryPacket
// vs the REAL ServerboundBlockEntityTagQueryPacket.STREAM_CODEC
// (tools/PktBlockEntityTagQuerySbParity.java).
//
// The packet body, VERBATIM from write(FriendlyByteBuf)
// (ServerboundBlockEntityTagQueryPacket.java:26-29), is:
//   writeVarInt(transactionId)   -> LEB128 VarInt of a signed int
//   writeBlockPos(pos)           -> writeLong(pos.asLong())  big-endian 8-byte long
//        (FriendlyByteBuf.java:398-400; BlockPos.asLong packs x26/z26/y12)
//
// No Holder / ResourceLocation / ItemStack / Component / NBT is on the wire: the
// body is a VarInt plus a packed BlockPos long, so the certified PacketBuffer (the
// FriendlyByteBuf port) rebuilds the body directly:
//   writeVarInt(transactionId) + writeLong(posLong)
//
// For each ENC row the C++ encode must reproduce the Java wire bytes byte-for-byte
// and the same readableBytes; decode(Java bytes) must then recover transactionId
// and posLong.
//
//   pkt_block_entity_tag_query_sb_parity [--cases mcpp/build/pkt_block_entity_tag_query_sb.tsv]
//
// Row: ENC <name> <transactionId-dec> <posLong-dec> <readableBytes> <hex>
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
    std::vector<uint8_t> v;
    v.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((uint8_t)std::stoul(s.substr(i, 2), nullptr, 16));
    return v;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_block_entity_tag_query_sb.tsv";
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
        std::string tag, name, txnStr, posStr, lenStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, txnStr, '\t') ||
            !std::getline(ss, posStr, '\t') || !std::getline(ss, lenStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        // transactionId is a signed 32-bit int (Integer.MIN_VALUE fits in long parse).
        int32_t transactionId = (int32_t)std::stoll(txnStr);
        int64_t posLong = (int64_t)std::stoll(posStr);
        size_t readableBytes = (size_t)std::stoul(lenStr);

        // (1) ENCODE: write the body in the same order via PacketBuffer and compare.
        PacketBuffer enc;
        enc.writeVarInt(transactionId);   // writeVarInt(transactionId)
        enc.writeLong(posLong);           // writeBlockPos == writeLong(asLong)

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != readableBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " txn=" << transactionId
                      << " pos=" << posLong
                      << "\n  got  (" << enc.data().size() << ") " << got
                      << "\n  want (" << readableBytes << ") " << expHex << "\n";
            continue;
        }

        // (2) DECODE: read the Java bytes back and verify every field.
        std::vector<uint8_t> raw = unhex(expHex);
        if (raw.size() != readableBytes) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH " << name << " hex=" << expHex
                      << " bytes=" << raw.size() << " want=" << readableBytes << "\n";
            continue;
        }
        PacketBuffer dec(raw);
        int32_t gotTxn = dec.readVarInt();
        int64_t gotPos = dec.readLong();

        bool ok = (gotTxn == transactionId) && (gotPos == posLong) &&
                  (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " txn=" << transactionId << " (got " << gotTxn << ")"
                      << " pos=" << posLong << " (got " << gotPos << ")"
                      << " rem=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktBlockEntityTagQuerySbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

// Byte-exact parity gate for net.minecraft.network.protocol.game.ClientboundBlockUpdatePacket
// vs the REAL ClientboundBlockUpdatePacket.STREAM_CODEC (tools/PktBlockUpdateParity.java).
//
// The packet body, VERBATIM from the STREAM_CODEC.composite
// (ClientboundBlockUpdatePacket.java:14-20), in codec order, is:
//   BlockPos.STREAM_CODEC.encode -> writeBlockPos(pos) -> writeLong(pos.asLong())
//        big-endian 8-byte long  (BlockPos.java:39-46, FriendlyByteBuf.java:398-400)
//   ByteBufCodecs.idMapper(Block.BLOCK_STATE_REGISTRY).encode
//        -> VarInt.write(out, BLOCK_STATE_REGISTRY.getIdOrThrow(state))
//        (ByteBufCodecs.java:542-558) -> a plain integer BlockState registry id.
//
// No Holder / ResourceLocation / ItemStack / Component / NBT is on the wire: the
// block state is just an integer registry id, so the certified PacketBuffer (the
// FriendlyByteBuf port) rebuilds the body directly:
//   writeLong(posLong) + writeVarInt(stateId)
//
// For each ENC row the C++ encode must reproduce the Java wire bytes byte-for-byte
// and the same readableBytes; decode(Java bytes) must then recover posLong/stateId.
//
//   pkt_block_update_parity [--cases mcpp/build/pkt_block_update.tsv]
//
// Row: ENC <name> <posLong-dec> <stateId-dec> <readableBytes> <hex>
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
    std::string casesPath = "build/pkt_block_update.tsv";
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
        std::string tag, name, posStr, idStr, lenStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, posStr, '\t') ||
            !std::getline(ss, idStr, '\t') || !std::getline(ss, lenStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        int64_t posLong = (int64_t)std::stoll(posStr);
        int32_t stateId = (int32_t)std::stol(idStr);
        size_t readableBytes = (size_t)std::stoul(lenStr);

        // (1) ENCODE: write the body in the same codec order via PacketBuffer.
        PacketBuffer enc;
        enc.writeLong(posLong);                       // writeBlockPos == writeLong(asLong)
        enc.writeVarInt(stateId);                     // idMapper VarInt(state registry id)

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != readableBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " pos=" << posLong
                      << " id=" << stateId
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
        int64_t gotPos = dec.readLong();
        int32_t gotId = dec.readVarInt();

        bool ok = (gotPos == posLong) && (gotId == stateId) && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name << " pos=" << posLong
                      << " (got " << gotPos << ") id=" << stateId << "/" << gotId
                      << " rem=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktBlockUpdateParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

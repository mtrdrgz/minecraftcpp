// Byte-exact parity gate for net.minecraft.network.protocol.game.ClientboundBlockEventPacket
// vs the REAL ClientboundBlockEventPacket.STREAM_CODEC (tools/PktBlockEventParity.java).
//
// The packet body, VERBATIM from write(RegistryFriendlyByteBuf)
// (ClientboundBlockEventPacket.java:35-40), is:
//   writeBlockPos(pos)  -> writeLong(pos.asLong())   big-endian 8-byte long
//   writeByte(b0)       -> one byte (low 8 bits)
//   writeByte(b1)       -> one byte (low 8 bits)
//   ByteBufCodecs.registry(BLOCK).encode(out, block) -> VarInt(registry id)
//        (ByteBufCodecs.java:560-577 -> VarInt.write(out, getIdOrThrow(value)))
//
// No Holder / ResourceLocation / ItemStack / Component / NBT is on the wire: the
// block is just an integer registry id, so the certified PacketBuffer (the
// FriendlyByteBuf port) rebuilds the body directly:
//   writeLong(posLong) + writeByte(b0) + writeByte(b1) + writeVarInt(blockId)
//
// For each ENC row the C++ encode must reproduce the Java wire bytes byte-for-byte
// and the same readableBytes; decode(Java bytes) must then recover posLong/b0/b1/id.
//
//   pkt_block_event_parity [--cases mcpp/build/pkt_block_event.tsv]
//
// Row: ENC <name> <posLong-dec> <b0-dec> <b1-dec> <blockId-dec> <readableBytes> <hex>
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
    std::string casesPath = "build/pkt_block_event.tsv";
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
        std::string tag, name, posStr, b0Str, b1Str, idStr, lenStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, posStr, '\t') ||
            !std::getline(ss, b0Str, '\t') || !std::getline(ss, b1Str, '\t') ||
            !std::getline(ss, idStr, '\t') || !std::getline(ss, lenStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        int64_t posLong = (int64_t)std::stoll(posStr);
        int32_t b0 = (int32_t)std::stol(b0Str);       // 0..255
        int32_t b1 = (int32_t)std::stol(b1Str);       // 0..255
        int32_t blockId = (int32_t)std::stol(idStr);
        size_t readableBytes = (size_t)std::stoul(lenStr);

        // (1) ENCODE: write the body in the same order via PacketBuffer and compare.
        PacketBuffer enc;
        enc.writeLong(posLong);                       // writeBlockPos == writeLong(asLong)
        enc.writeByte((uint8_t)(b0 & 0xff));          // writeByte: low 8 bits
        enc.writeByte((uint8_t)(b1 & 0xff));
        enc.writeVarInt(blockId);                     // registry id VarInt

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != readableBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " pos=" << posLong
                      << " b0=" << b0 << " b1=" << b1 << " id=" << blockId
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
        int32_t gotB0 = (int32_t)dec.readByte();      // readUnsignedByte: 0..255
        int32_t gotB1 = (int32_t)dec.readByte();
        int32_t gotId = dec.readVarInt();

        bool ok = (gotPos == posLong) && (gotB0 == (b0 & 0xff)) &&
                  (gotB1 == (b1 & 0xff)) && (gotId == blockId) &&
                  (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name << " pos=" << posLong
                      << " (got " << gotPos << ") b0=" << b0 << "/" << gotB0
                      << " b1=" << b1 << "/" << gotB1
                      << " id=" << blockId << "/" << gotId
                      << " rem=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktBlockEventParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

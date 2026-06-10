// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundJigsawGeneratePacket
// vs the REAL ServerboundJigsawGeneratePacket.STREAM_CODEC (tools/PktJigsawGenerateSbParity.java).
//
// The packet body, VERBATIM from write()/ctor exercised by
//   STREAM_CODEC = Packet.codec(::write, ::new)   (ServerboundJigsawGeneratePacket.java:10-12)
// in codec order (ServerboundJigsawGeneratePacket.java:23-33), is:
//   output.writeBlockPos(this.pos);       -> writeLong(pos.asLong()) big-endian 8-byte
//        long (FriendlyByteBuf.java:393-400; BlockPos.java:107-116).
//   output.writeVarInt(this.levels);      -> LEB128 VarInt of the int levels.
//   output.writeBoolean(this.keepJigsaws);-> 1 byte 0x00/0x01.
//
// No Holder / ResourceLocation / ItemStack / Component / NBT is on the wire: every field
// is a primitive, so the certified PacketBuffer (the FriendlyByteBuf port) rebuilds the
// body directly: writeLong(posLong) + writeVarInt(levels) + writeBoolean(keepJigsaws).
//
// For each ENC row the C++ encode must reproduce the Java wire bytes byte-for-byte and
// the same readableBytes; decode(Java bytes) must then recover posLong/levels/keepJigsaws.
//
//   pkt_jigsaw_generate_sb_parity [--cases mcpp/build/pkt_jigsaw_generate_sb.tsv]
//
// Row: ENC <name> <posLong-dec> <levels-dec> <keepJigsaws-0|1> <readableBytes> <hex>
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
    std::string casesPath = "mcpp/build/pkt_jigsaw_generate_sb.tsv";
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
        std::string tag, name, posStr, lvlStr, keepStr, lenStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, posStr, '\t') ||
            !std::getline(ss, lvlStr, '\t') || !std::getline(ss, keepStr, '\t') ||
            !std::getline(ss, lenStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        int64_t posLong = (int64_t)std::stoll(posStr);
        int32_t levels = (int32_t)std::stol(lvlStr);
        bool keepJigsaws = (std::stoi(keepStr) != 0);
        size_t readableBytes = (size_t)std::stoul(lenStr);

        // (1) ENCODE: write the body in the same codec order via PacketBuffer.
        PacketBuffer enc;
        enc.writeLong(posLong);          // writeBlockPos == writeLong(pos.asLong())
        enc.writeVarInt(levels);         // writeVarInt(levels)
        enc.writeBool(keepJigsaws);      // writeBoolean(keepJigsaws)

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != readableBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " pos=" << posLong
                      << " levels=" << levels << " keep=" << keepJigsaws
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
        int32_t gotLvl = dec.readVarInt();
        bool gotKeep = dec.readBool();

        bool ok = (gotPos == posLong) && (gotLvl == levels) &&
                  (gotKeep == keepJigsaws) && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name << " pos=" << posLong
                      << " (got " << gotPos << ") levels=" << levels << "/" << gotLvl
                      << " keep=" << keepJigsaws << "/" << gotKeep
                      << " rem=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktJigsawGenerateSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

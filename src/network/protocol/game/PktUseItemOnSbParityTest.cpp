// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundUseItemOnPacket
// vs the REAL ServerboundUseItemOnPacket.STREAM_CODEC (tools/PktUseItemOnSbParity.java).
//
// The packet body, VERBATIM from write(FriendlyByteBuf)
// (ServerboundUseItemOnPacket.java:30-34):
//   writeEnum(hand)              -> writeVarInt(hand.ordinal())       MAIN_HAND=0, OFF_HAND=1
//   writeBlockHitResult(blockHit)                                    (FriendlyByteBuf.java:636-646):
//       writeBlockPos(pos)       -> writeLong(pos.asLong())          big-endian 8-byte long
//       writeEnum(direction)     -> writeVarInt(direction.ordinal()) DOWN=0..EAST=5
//       writeFloat(loc.x - pos.x())                                  BE 4-byte float
//       writeFloat(loc.y - pos.y())                                  BE 4-byte float
//       writeFloat(loc.z - pos.z())                                  BE 4-byte float
//       writeBoolean(inside)     -> one byte 0/1
//       writeBoolean(worldBorder)-> one byte 0/1
//   writeVarInt(sequence)        -> VarInt
//
// No Holder / ResourceLocation / ItemStack / Component / NBT is on the wire: every
// field decomposes to primitives the certified PacketBuffer (the FriendlyByteBuf
// port) supports, so it rebuilds the body directly:
//   writeVarInt(handOrd) + writeLong(posLong) + writeVarInt(dirOrd)
//     + writeFloat(fx) + writeFloat(fy) + writeFloat(fz)
//     + writeByte(inside) + writeByte(worldBorder) + writeVarInt(sequence)
//
// The three floats are carried as their EXACT 32-bit IEEE-754 raw int bits in the
// TSV (so there is zero double->float rounding ambiguity); the C++ side bit-casts
// those raw bits back into a float and emits them via writeFloat.
//
// For each ENC row the C++ encode must reproduce the Java wire bytes byte-for-byte
// and the same readableBytes; decode(Java bytes) must then recover every field.
//
//   pkt_use_item_on_sb_parity [--cases mcpp/build/pkt_use_item_on_sb.tsv]
//
// Row: ENC <name> <handOrd> <posLong> <dirOrd> <fxBits8> <fyBits8> <fzBits8>
//          <inside> <worldBorder> <seq> <readableBytes> <hex>
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

// Bit-cast a 32-bit raw IEEE-754 pattern (parsed from hex) into a float.
float floatFromBits(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_use_item_on_sb.tsv";
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
        std::string tag, name, handStr, posStr, dirStr, fxStr, fyStr, fzStr,
                    insideStr, wbStr, seqStr, lenStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t')   || !std::getline(ss, handStr, '\t') ||
            !std::getline(ss, posStr, '\t') || !std::getline(ss, dirStr, '\t')  ||
            !std::getline(ss, fxStr, '\t')  || !std::getline(ss, fyStr, '\t')   ||
            !std::getline(ss, fzStr, '\t')  || !std::getline(ss, insideStr, '\t') ||
            !std::getline(ss, wbStr, '\t')  || !std::getline(ss, seqStr, '\t')  ||
            !std::getline(ss, lenStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t handOrd = (int32_t)std::stol(handStr);
        int64_t posLong = (int64_t)std::stoll(posStr);
        int32_t dirOrd  = (int32_t)std::stol(dirStr);
        uint32_t fxBits = (uint32_t)std::stoul(fxStr, nullptr, 16);
        uint32_t fyBits = (uint32_t)std::stoul(fyStr, nullptr, 16);
        uint32_t fzBits = (uint32_t)std::stoul(fzStr, nullptr, 16);
        int32_t inside  = (int32_t)std::stol(insideStr);
        int32_t wb      = (int32_t)std::stol(wbStr);
        int32_t seq     = (int32_t)std::stoll(seqStr);
        size_t readableBytes = (size_t)std::stoul(lenStr);

        float fx = floatFromBits(fxBits);
        float fy = floatFromBits(fyBits);
        float fz = floatFromBits(fzBits);

        // (1) ENCODE: write the body in the same codec order via PacketBuffer.
        PacketBuffer enc;
        enc.writeVarInt(handOrd);              // writeEnum(hand)  -> VarInt(ordinal)
        enc.writeLong(posLong);                // writeBlockPos    -> writeLong(asLong)
        enc.writeVarInt(dirOrd);               // writeEnum(dir)   -> VarInt(ordinal)
        enc.writeFloat(fx);                    // local clickX
        enc.writeFloat(fy);                    // local clickY
        enc.writeFloat(fz);                    // local clickZ
        enc.writeByte((uint8_t)(inside ? 1 : 0));
        enc.writeByte((uint8_t)(wb ? 1 : 0));
        enc.writeVarInt(seq);                  // sequence VarInt

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != readableBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " hand=" << handOrd
                      << " pos=" << posLong << " dir=" << dirOrd
                      << " seq=" << seq
                      << "\n  got  (" << enc.data().size() << ") " << got
                      << "\n  want (" << readableBytes << ") " << expHex << "\n";
            continue;
        }

        // (2) DECODE: read the Java bytes back and verify every field round-trips.
        std::vector<uint8_t> raw = unhex(expHex);
        if (raw.size() != readableBytes) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH " << name << " hex=" << expHex
                      << " bytes=" << raw.size() << " want=" << readableBytes << "\n";
            continue;
        }
        PacketBuffer dec(raw);
        int32_t gotHand = dec.readVarInt();
        int64_t gotPos  = dec.readLong();
        int32_t gotDir  = dec.readVarInt();
        float gotFx = dec.readFloat();
        float gotFy = dec.readFloat();
        float gotFz = dec.readFloat();
        int32_t gotInside = (int32_t)dec.readByte();
        int32_t gotWb     = (int32_t)dec.readByte();
        int32_t gotSeq    = dec.readVarInt();

        // Compare floats by raw bits so NaN / -0.0 are exact.
        uint32_t gFx, gFy, gFz;
        std::memcpy(&gFx, &gotFx, 4);
        std::memcpy(&gFy, &gotFy, 4);
        std::memcpy(&gFz, &gotFz, 4);

        bool ok = (gotHand == handOrd) && (gotPos == posLong) && (gotDir == dirOrd) &&
                  (gFx == fxBits) && (gFy == fyBits) && (gFz == fzBits) &&
                  (gotInside == (inside ? 1 : 0)) && (gotWb == (wb ? 1 : 0)) &&
                  (gotSeq == seq) && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " hand=" << handOrd << "/" << gotHand
                      << " pos=" << posLong << "/" << gotPos
                      << " dir=" << dirOrd << "/" << gotDir
                      << " inside=" << inside << "/" << gotInside
                      << " wb=" << wb << "/" << gotWb
                      << " seq=" << seq << "/" << gotSeq
                      << " rem=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktUseItemOnSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

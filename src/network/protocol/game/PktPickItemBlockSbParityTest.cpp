// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundPickItemFromBlockPacket
// vs the REAL ServerboundPickItemFromBlockPacket.STREAM_CODEC (tools/PktPickItemBlockSbParity.java).
//
// The packet is record(BlockPos pos, boolean includeData). The FULL wire body, in codec order, is
// (see GT tool header for the verbatim source map):
//   writeLong(pos.asLong())   -- big-endian 8-byte long  (BlockPos.STREAM_CODEC -> writeBlockPos)
//   writeBoolean(includeData) -- single byte, 0x01/0x00   (ByteBufCodecs.BOOL)
//
// No Holder / ItemStack / Component / NBT / registry id is on the wire -- both fields decompose to
// primitives the certified PacketBuffer (the FriendlyByteBuf port) supports -- so it rebuilds the
// body directly: writeLong(posLong) + writeBool(includeData).
//
// For each ENC row the C++ encode must reproduce the Java wire bytes byte-for-byte and the same
// readableBytes; decode(Java bytes) must then recover posLong/includeData exactly.
//
//   pkt_pick_item_block_sb_parity [--cases mcpp/build/pkt_pick_item_block_sb.tsv]
//
// Row: ENC <name> <posLong-dec> <includeData 0|1> <readableBytes> <hex>
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
    std::string casesPath = "build/pkt_pick_item_block_sb.tsv";
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
        std::string tag, name, posStr, incStr, lenStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, posStr, '\t') ||
            !std::getline(ss, incStr, '\t') || !std::getline(ss, lenStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        int64_t posLong = (int64_t)std::stoll(posStr);
        bool includeData = (std::stoi(incStr) != 0);
        size_t readableBytes = (size_t)std::stoul(lenStr);

        // (1) ENCODE: write the body in the same codec order via PacketBuffer and compare.
        PacketBuffer enc;
        enc.writeLong(posLong);        // writeBlockPos == writeLong(asLong), BE 8 bytes
        enc.writeBool(includeData);    // ByteBufCodecs.BOOL, single byte

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != readableBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " pos=" << posLong
                      << " includeData=" << (includeData ? 1 : 0)
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
        bool gotInc = dec.readBool();

        bool ok = (gotPos == posLong) && (gotInc == includeData) && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " pos=" << posLong << "/" << gotPos
                      << " includeData=" << (includeData ? 1 : 0) << "/" << (gotInc ? 1 : 0)
                      << " rem=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktPickItemBlockSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

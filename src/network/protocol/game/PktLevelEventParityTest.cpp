// Byte-exact parity gate for net.minecraft.network.protocol.game.ClientboundLevelEventPacket
// vs the REAL ClientboundLevelEventPacket.STREAM_CODEC (tools/PktLevelEventParity.java).
//
// The packet body, VERBATIM from write(FriendlyByteBuf)
// (ClientboundLevelEventPacket.java:32-37), is:
//   writeInt(type)        -> 4-byte big-endian int
//   writeBlockPos(pos)    -> writeLong(pos.asLong())   big-endian 8-byte long
//   writeInt(data)        -> 4-byte big-endian int
//   writeBoolean(global)  -> one byte 0/1
//
// No Holder / ResourceLocation / ItemStack / Component / NBT is on the wire: every
// field is a primitive, so the certified PacketBuffer (the FriendlyByteBuf port)
// rebuilds the body directly:
//   writeInt(type) + writeLong(posLong) + writeInt(data) + writeBool(global)
//
// For each ENC row the C++ encode must reproduce the Java wire bytes byte-for-byte
// and the same readableBytes; decode(Java bytes) must then recover all fields.
//
//   pkt_level_event_parity [--cases mcpp/build/pkt_level_event.tsv]
//
// Row: ENC <name> <type-dec> <posLong-dec> <data-dec> <global-dec> <readableBytes> <hex>
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
    std::string casesPath = "mcpp/build/pkt_level_event.tsv";
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
        std::string tag, name, typeStr, posStr, dataStr, globalStr, lenStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, typeStr, '\t') ||
            !std::getline(ss, posStr, '\t') || !std::getline(ss, dataStr, '\t') ||
            !std::getline(ss, globalStr, '\t') || !std::getline(ss, lenStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t type = (int32_t)std::stol(typeStr);
        int64_t posLong = (int64_t)std::stoll(posStr);
        int32_t data = (int32_t)std::stol(dataStr);
        int32_t global = (int32_t)std::stol(globalStr); // 0/1
        size_t readableBytes = (size_t)std::stoul(lenStr);

        // (1) ENCODE: write the body in the same order via PacketBuffer and compare.
        PacketBuffer enc;
        enc.writeInt(type);                  // writeInt: 4-byte big-endian
        enc.writeLong(posLong);              // writeBlockPos == writeLong(asLong)
        enc.writeInt(data);                  // writeInt: 4-byte big-endian
        enc.writeBool(global != 0);          // writeBoolean: one byte 0/1

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != readableBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " type=" << type
                      << " pos=" << posLong << " data=" << data << " global=" << global
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
        int32_t gotType = dec.readInt();
        int64_t gotPos = dec.readLong();
        int32_t gotData = dec.readInt();
        bool gotGlobal = dec.readBool();

        bool ok = (gotType == type) && (gotPos == posLong) && (gotData == data) &&
                  (gotGlobal == (global != 0)) && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " type=" << type << "/" << gotType
                      << " pos=" << posLong << "/" << gotPos
                      << " data=" << data << "/" << gotData
                      << " global=" << global << "/" << (gotGlobal ? 1 : 0)
                      << " rem=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktLevelEventParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundChangeDifficultyPacket
// vs the REAL ClientboundChangeDifficultyPacket.STREAM_CODEC (tools/PktChangeDifficultyParity.java).
//
// 26.1.2 wire format (verified against 26.1.2/src):
//   STREAM_CODEC = StreamCodec.composite(
//       Difficulty.STREAM_CODEC, ::difficulty,   -> VarInt(difficulty.getId())
//       ByteBufCodecs.BOOL,      ::locked,        -> single byte 0/1
//       ::new)
//   Difficulty.STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, Difficulty::getId); idMapper.encode
//   writes VarInt.write(output, getId()) (ByteBufCodecs lines 542-553) — a VarInt of getId(),
//   NOT writeEnum(ordinal) and NOT a raw fixed byte. getId(): PEACEFUL=0/EASY=1/NORMAL=2/HARD=3.
//   ByteBufCodecs.BOOL -> FriendlyByteBuf.writeBoolean = a single 0/1 byte.
//
// This test exercises mc::net::PacketBuffer ONLY (no per-packet C++ class/header): it replays the
// codec — writeVarInt(id) then writeBool(locked) — and requires the produced bytes (as hex) AND the
// byte count == the Java ground truth, then decodes the Java bytes back through PacketBuffer requiring
// (id, locked) round-trip exactly.
//
//   pkt_change_difficulty_parity [--cases mcpp/build/pkt_change_difficulty.tsv]
//
// Row: ENC <name> <id-dec> <locked-0|1> <readableBytes-dec> <hexBytes>
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
    std::string casesPath = "build/pkt_change_difficulty.tsv";
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
        std::string tag, name, idStr, lockedStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t')   || !std::getline(ss, idStr, '\t') ||
            !std::getline(ss, lockedStr, '\t') || !std::getline(ss, nStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t id      = (int32_t)std::stoll(idStr);
        bool    locked  = std::stoi(lockedStr) != 0;
        size_t  expBytes = (size_t)std::stoul(nStr);

        // (1) ENCODE: VarInt(getId()) + BOOL(locked) through PacketBuffer; compare bytes + count.
        PacketBuffer enc;
        enc.writeVarInt(id);
        enc.writeBool(locked);

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " id=" << id << " locked=" << locked
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE: pull (id, locked) back out of the Java bytes; require exact round-trip.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t backId   = dec.readVarInt();
        bool    backLock = dec.readBool();
        if (backId != id || backLock != locked) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " gotId=" << backId << " wantId=" << id
                      << " gotLocked=" << backLock << " wantLocked=" << locked << "\n";
            continue;
        }
        if (!dec.eof()) {
            ++mismatches;
            std::cerr << "DECODE-TRAILING " << name << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktChangeDifficultyParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

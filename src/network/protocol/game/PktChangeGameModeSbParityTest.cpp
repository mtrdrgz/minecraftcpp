// Parity gate for ServerboundChangeGameModePacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktChangeGameModeSbParity.java ground truth).
//
// net.minecraft.network.protocol.game.ServerboundChangeGameModePacket (26.1.2):
//   public record ServerboundChangeGameModePacket(GameType mode) ...
//   STREAM_CODEC = StreamCodec.composite(GameType.STREAM_CODEC, ::mode, ::new);
// GameType.STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, GameType::getId):
//   encode -> VarInt.write(out, value.getId())   (ByteBufCodecs:549-552)
//   decode -> BY_ID.apply(VarInt.read(in))        (ByteBufCodecs:544-546)
// So the packet body is a single VarInt of the GameType id. GameType ids
// (GameType.java 17-20): SURVIVAL=0, CREATIVE=1, ADVENTURE=2, SPECTATOR=3.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt(id) is LEB128 and readVarInt() the inverse, matching the real
// codec byte-for-byte.
//
//   pkt_change_gamemode_sb_parity [--cases mcpp/build/pkt_change_gamemode_sb.tsv]
#include "../../PacketBuffer.h"

#include <array>
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

// GameType constants in declaration order; index == id (GameType.java 17-20).
const std::array<const char*, 4> kModeNames = {
    "SURVIVAL",   // id 0
    "CREATIVE",   // id 1
    "ADVENTURE",  // id 2
    "SPECTATOR",  // id 3
};
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_change_gamemode_sb.tsv";
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

        if (tag == "ENUM") {
            // ENUM <id> <name>
            std::string idStr, name;
            if (!std::getline(ss, idStr, '\t') || !std::getline(ss, name)) continue;
            ++cases;
            int id = std::stoi(idStr);
            if (id < 0 || id >= (int)kModeNames.size() ||
                name != kModeNames[(size_t)id]) {
                ++mismatches;
                std::cerr << "ENUM-MISMATCH id=" << id << " got="
                          << (id >= 0 && id < (int)kModeNames.size()
                                  ? kModeNames[(size_t)id] : "<oob>")
                          << " want=" << name << "\n";
            }
        } else if (tag == "ENC") {
            // ENC <name> <id> <readableBytes> <hex>
            std::string name, idStr, rbStr, expHex;
            if (!std::getline(ss, name, '\t') || !std::getline(ss, idStr, '\t')
                || !std::getline(ss, rbStr, '\t') || !std::getline(ss, expHex))
                continue;
            ++cases;
            int32_t id = (int32_t)std::stoll(idStr);
            int expReadable = std::stoi(rbStr);

            // write(): STREAM_CODEC.encode -> VarInt.write(id).
            PacketBuffer enc;
            enc.writeVarInt(id);
            std::string got = hex(enc.data());
            if (got != expHex) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH name=" << name << " id=" << id
                          << "\n  got  " << got << "\n  want " << expHex << "\n";
            }
            if ((int)enc.data().size() != expReadable) {
                ++mismatches;
                std::cerr << "ENC-READABLE-MISMATCH name=" << name << " id=" << id
                          << " got=" << enc.data().size()
                          << " want=" << expReadable << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hex> <id_in> <id_out>
            std::string inHex, idInStr, idOutStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, idInStr, '\t')
                || !std::getline(ss, idOutStr)) continue;
            ++cases;
            int32_t expId = (int32_t)std::stoll(idOutStr);

            // read(): STREAM_CODEC.decode -> BY_ID.apply(VarInt.read()).
            // For the in-range ids emitted here BY_ID.apply is the identity, so
            // the decoded id equals the read VarInt.
            PacketBuffer dec(unhex(inHex));
            int32_t gotId = dec.readVarInt();
            if (gotId != expId) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex << " got=" << gotId
                          << " want=" << expId << "\n";
            }
        }
    }

    std::cout << "PktChangeGameModeSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

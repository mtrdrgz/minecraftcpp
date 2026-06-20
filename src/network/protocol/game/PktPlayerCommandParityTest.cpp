// Parity gate for ServerboundPlayerCommandPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktPlayerCommandParity.java ground truth).
//
// The packet body is exactly (ServerboundPlayerCommandPacket.java lines 27-37):
//   write : FriendlyByteBuf.writeVarInt(id)
//           FriendlyByteBuf.writeEnum(action) = writeVarInt(action.ordinal())
//           FriendlyByteBuf.writeVarInt(data)
//   read  : id     = readVarInt()
//           action = readEnum(Action.class) = getEnumConstants()[readVarInt()]
//           data   = readVarInt()
// (FriendlyByteBuf.writeEnum 471-473 / readEnum 467-469.) STREAM_CODEC =
// Packet.codec -> no packet-id prefix, just the body: VarInt(id), VarInt(ordinal),
// VarInt(data).
//
// Action declaration order (ServerboundPlayerCommandPacket.java 60-67):
//   STOP_SLEEPING=0, START_SPRINTING=1, STOP_SPRINTING=2, START_RIDING_JUMP=3,
//   STOP_RIDING_JUMP=4, OPEN_INVENTORY=5, START_FALL_FLYING=6
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt is LEB128 and readVarInt() its inverse, matching the real codec
// byte-for-byte.
//
//   pkt_player_command_parity [--cases mcpp/build/pkt_player_command.tsv]
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
    std::vector<uint8_t> b;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        b.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return b;
}

// ServerboundPlayerCommandPacket.Action constants, in declaration order
// (== ordinal). Verbatim from the Java enum body lines 60-67.
const std::array<const char*, 7> kActionNames = {
    "STOP_SLEEPING",      // ordinal 0
    "START_SPRINTING",    // ordinal 1
    "STOP_SPRINTING",     // ordinal 2
    "START_RIDING_JUMP",  // ordinal 3
    "STOP_RIDING_JUMP",   // ordinal 4
    "OPEN_INVENTORY",     // ordinal 5
    "START_FALL_FLYING",  // ordinal 6
};
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_player_command.tsv";
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
            // ENUM <ordinal> <name>
            std::string ordStr, name;
            if (!std::getline(ss, ordStr, '\t') || !std::getline(ss, name)) continue;
            ++cases;
            int ord = std::stoi(ordStr);
            if (ord < 0 || ord >= (int)kActionNames.size() ||
                name != kActionNames[(size_t)ord]) {
                ++mismatches;
                std::cerr << "ENUM-MISMATCH ord=" << ord << " got="
                          << (ord >= 0 && ord < (int)kActionNames.size()
                                  ? kActionNames[(size_t)ord] : "<oob>")
                          << " want=" << name << "\n";
            }
        } else if (tag == "ENC") {
            // ENC <id> <ordinal> <data> <readableBytes> <hex>
            std::string idStr, ordStr, dataStr, nStr, expHex;
            if (!std::getline(ss, idStr, '\t') || !std::getline(ss, ordStr, '\t')
                || !std::getline(ss, dataStr, '\t') || !std::getline(ss, nStr, '\t')
                || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t id   = (int32_t)std::stoll(idStr);
            int32_t ord  = (int32_t)std::stoll(ordStr);
            int32_t data = (int32_t)std::stoll(dataStr);
            size_t  n    = (size_t)std::stoull(nStr);

            // write(): writeVarInt(id), writeEnum(action)=writeVarInt(ordinal),
            // writeVarInt(data).
            PacketBuffer enc;
            enc.writeVarInt(id);
            enc.writeVarInt(ord);
            enc.writeVarInt(data);
            std::string got = hex(enc.data());
            if (got != expHex || enc.data().size() != n) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH id=" << id << " ord=" << ord
                          << " data=" << data
                          << "\n  got  " << got << " (len " << enc.data().size() << ")"
                          << "\n  want " << expHex << " (len " << n << ")\n";
            }
        } else if (tag == "DEC") {
            // DEC <hex> <id_in> <ord_in> <data_in> <id_out> <ord_out> <data_out>
            std::string inHex, idInStr, ordInStr, dataInStr,
                        idOutStr, ordOutStr, dataOutStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, idInStr, '\t')
                || !std::getline(ss, ordInStr, '\t') || !std::getline(ss, dataInStr, '\t')
                || !std::getline(ss, idOutStr, '\t') || !std::getline(ss, ordOutStr, '\t')
                || !std::getline(ss, dataOutStr)) continue;
            ++cases;
            int32_t expId   = (int32_t)std::stoll(idOutStr);
            int32_t expOrd  = (int32_t)std::stoll(ordOutStr);
            int32_t expData = (int32_t)std::stoll(dataOutStr);

            // read(): readVarInt() id, readEnum->readVarInt() ordinal, readVarInt() data.
            PacketBuffer dec(unhex(inHex));
            int32_t gotId   = dec.readVarInt();
            int32_t gotOrd  = dec.readVarInt();
            int32_t gotData = dec.readVarInt();
            if (gotId != expId || gotOrd != expOrd || gotData != expData) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex
                          << " got=(" << gotId << "," << gotOrd << "," << gotData << ")"
                          << " want=(" << expId << "," << expOrd << "," << expData << ")\n";
            }
        }
    }

    std::cout << "PktPlayerCommandParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

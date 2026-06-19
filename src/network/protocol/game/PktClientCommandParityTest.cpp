// Parity gate for ServerboundClientCommandPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktClientCommandParity.java ground truth).
//
// The packet body is exactly:
//   write : FriendlyByteBuf.writeEnum(action) = writeVarInt(action.ordinal())
//   read  : FriendlyByteBuf.readEnum(Action.class) = getEnumConstants()[readVarInt()]
// (net.minecraft.network.protocol.game.ServerboundClientCommandPacket lines 18-24;
//  FriendlyByteBuf.writeEnum 471-473 / readEnum 467-469. Packet.codec -> no packet-id
//  prefix, just the body: a single VarInt of the enum ordinal.)
//
// Action declaration order (ServerboundClientCommandPacket.java 39-43):
//   PERFORM_RESPAWN=0, REQUEST_STATS=1, REQUEST_GAMERULE_VALUES=2
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt(ordinal) is LEB128 and readVarInt() the inverse, matching the real
// codec byte-for-byte.
//
//   pkt_client_command_parity [--cases mcpp/build/pkt_client_command.tsv]
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

// ServerboundClientCommandPacket.Action constants, in declaration order
// (== ordinal). Verbatim from the Java enum body lines 39-43.
const std::array<const char*, 3> kActionNames = {
    "PERFORM_RESPAWN",          // ordinal 0
    "REQUEST_STATS",            // ordinal 1
    "REQUEST_GAMERULE_VALUES",  // ordinal 2
};
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_client_command.tsv";
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
            // ENC <ordinal> <hex>
            std::string ordStr, expHex;
            if (!std::getline(ss, ordStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t ord = (int32_t)std::stoll(ordStr);

            // write(): FriendlyByteBuf.writeEnum(action) -> writeVarInt(ordinal).
            PacketBuffer enc;
            enc.writeVarInt(ord);
            std::string got = hex(enc.data());
            if (got != expHex) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH ord=" << ord << "\n  got  " << got
                          << "\n  want " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hex> <ordinal_in> <ordinal_out>
            std::string inHex, ordInStr, ordOutStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, ordInStr, '\t')
                || !std::getline(ss, ordOutStr)) continue;
            ++cases;
            int32_t expOrd = (int32_t)std::stoll(ordOutStr);

            // read(): FriendlyByteBuf.readEnum -> getEnumConstants()[readVarInt()].
            std::vector<uint8_t> bytes;
            for (size_t i = 0; i + 1 < inHex.size(); i += 2)
                bytes.push_back((uint8_t)std::stoi(inHex.substr(i, 2), nullptr, 16));
            PacketBuffer dec(bytes);
            int32_t gotOrd = dec.readVarInt();
            if (gotOrd != expOrd) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex << " got=" << gotOrd
                          << " want=" << expOrd << "\n";
            }
        }
    }

    std::cout << "PktClientCommandParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

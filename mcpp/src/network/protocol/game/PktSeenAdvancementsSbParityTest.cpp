// Parity gate for net.minecraft.network.protocol.game.ServerboundSeenAdvancementsPacket
// vs the REAL net.minecraft STREAM_CODEC (tools/PktSeenAdvancementsSbParity.java GT).
//
// STREAM_CODEC = Packet.codec(write, new) -> no packet-id prefix, just the body.
//
// write(FriendlyByteBuf output) (ServerboundSeenAdvancementsPacket.java 40-45):
//   output.writeEnum(this.action);                                  // writeVarInt(ordinal)
//   if (this.action == Action.OPENED_TAB) output.writeIdentifier(this.tab);
//   writeIdentifier(id) = writeUtf(id.toString())                   (FriendlyByteBuf:585-588)
//   id.toString()       = namespace + ":" + path                    (Identifier:124-126)
//   writeUtf(s)         = VarInt(utf8 byte length) + utf8 bytes
//
// IMPORTANT: the tab is written ONLY when action == OPENED_TAB, with NO leading
// boolean present-flag. The Action enum (OPENED_TAB=0, CLOSED_SCREEN=1) is the
// discriminator. So:
//   CLOSED_SCREEN -> wire = VarInt(1)                (1 byte)
//   OPENED_TAB    -> wire = VarInt(0) + writeUtf(namespace:path)
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port): writeVarInt
// (LEB128) and writeString (== writeUtf: VarInt byte-length + UTF-8 bytes). The C++
// side rebuilds the wire field-by-field in codec order and must match byte-for-byte,
// then round-trips the GT bytes back through PacketBuffer (action ordinal + tab).
//
//   pkt_seen_advancements_sb_parity [--cases mcpp/build/pkt_seen_advancements_sb.tsv]
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

std::vector<uint8_t> unhex(const std::string& h) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back((uint8_t)std::stoi(h.substr(i, 2), nullptr, 16));
    return out;
}

std::string hexToAscii(const std::string& h) {
    std::string s;
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        s.push_back((char)(uint8_t)std::stoi(h.substr(i, 2), nullptr, 16));
    return s;
}

// ServerboundSeenAdvancementsPacket.Action constants, in declaration order
// (== ordinal). Verbatim from the Java enum body (lines 64-67).
const std::array<const char*, 2> kActionNames = {
    "OPENED_TAB",     // ordinal 0
    "CLOSED_SCREEN",  // ordinal 1
};
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_seen_advancements_sb.tsv";
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
            // ENC <name> <ordinal> <hasTab> <tabHex> <readableBytes> <hexBytes>
            std::string name, ordStr, hasTabStr, tabHex, readStr, expHex;
            if (!std::getline(ss, name, '\t') || !std::getline(ss, ordStr, '\t')
                || !std::getline(ss, hasTabStr, '\t') || !std::getline(ss, tabHex, '\t')
                || !std::getline(ss, readStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t ord = (int32_t)std::stoll(ordStr);
            bool hasTab = std::stoi(hasTabStr) != 0;
            int expReadable = std::stoi(readStr);
            std::string tab = hexToAscii(tabHex);

            // write(): writeEnum(action) = writeVarInt(ordinal);
            //          if OPENED_TAB: writeIdentifier(tab) = writeUtf(tab.toString()).
            PacketBuffer enc;
            enc.writeVarInt(ord);
            if (hasTab) enc.writeString(tab);

            std::string got = hex(enc.data());
            if (got != expHex) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH " << name << " tab=" << tab
                          << "\n  got  " << got << "\n  want " << expHex << "\n";
            }
            if ((int)enc.data().size() != expReadable) {
                ++mismatches;
                std::cerr << "LEN-MISMATCH " << name << " got=" << enc.data().size()
                          << " want=" << expReadable << "\n";
            }

            // Decode the GT bytes back through PacketBuffer: action ordinal first,
            // then the tab string iff OPENED_TAB (ordinal 0). Fields must round-trip.
            PacketBuffer dec(unhex(expHex));
            int32_t gotOrd = dec.readVarInt();
            if (gotOrd != ord) {
                ++mismatches;
                std::cerr << "DEC-ORD-MISMATCH " << name << " got=" << gotOrd
                          << " want=" << ord << "\n";
            }
            // OPENED_TAB has ordinal 0 -> tab present.
            if (gotOrd == 0) {
                std::string gotTab = dec.readString();
                if (gotTab != tab) {
                    ++mismatches;
                    std::cerr << "DEC-TAB-MISMATCH " << name << " got=" << gotTab
                              << " want=" << tab << "\n";
                }
            }
            if (dec.remaining() != 0) {
                ++mismatches;
                std::cerr << "DEC-TRAILING " << name << " remaining=" << dec.remaining()
                          << "\n";
            }
        }
    }

    std::cout << "PktSeenAdvancementsSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

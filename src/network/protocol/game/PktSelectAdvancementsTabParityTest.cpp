// Parity gate for net.minecraft.network.protocol.game.ClientboundSelectAdvancementsTabPacket
// vs the REAL net.minecraft STREAM_CODEC (tools/PktSelectAdvancementsTabParity.java GT).
//
// STREAM_CODEC = Packet.codec(write, new) -> no packet-id prefix, just the body.
//
// write(FriendlyByteBuf output) (ClientboundSelectAdvancementsTabPacket.java 24-26):
//   output.writeNullable(this.tab, FriendlyByteBuf::writeIdentifier);
//
// writeNullable(value, enc) (FriendlyByteBuf:267-274):
//   if value != null { writeBoolean(true);  writeIdentifier(value); }
//   else             { writeBoolean(false); }
//   writeIdentifier(id) = writeUtf(id.toString())     (FriendlyByteBuf:585-588)
//   id.toString()       = namespace + ":" + path      (Identifier:124-126)
//   writeUtf(s)         = VarInt(utf8 byte length) + utf8 bytes
//
// So the wire is:
//   tab == null -> 0x00                                (1 byte)
//   tab != null -> 0x01 + writeUtf(namespace:path)
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port): writeBool
// (single byte) and writeString (== writeUtf: VarInt byte-length + UTF-8 bytes). The
// C++ side rebuilds the wire field-by-field in codec order and must match
// byte-for-byte AND readableBytes, then round-trips the GT bytes back through
// PacketBuffer (present-flag + optional tab string).
//
//   pkt_select_advancements_tab_parity [--cases mcpp/build/pkt_select_advancements_tab.tsv]
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
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_select_advancements_tab.tsv";
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
        if (tag != "ENC") continue;

        // ENC <name> <present> <tabHex> <readableBytes> <hexBytes>
        std::string name, presentStr, tabHex, readStr, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, presentStr, '\t')
            || !std::getline(ss, tabHex, '\t') || !std::getline(ss, readStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;
        bool present = std::stoi(presentStr) != 0;
        int expReadable = std::stoi(readStr);
        std::string tab = hexToAscii(tabHex);

        // write(): writeNullable -> writeBool(present); if present writeUtf(tab).
        PacketBuffer enc;
        enc.writeBool(present);
        if (present) enc.writeString(tab);

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

        // Decode the GT bytes back through PacketBuffer: present-flag first, then
        // the tab string iff present. Fields must round-trip.
        PacketBuffer dec(unhex(expHex));
        bool gotPresent = dec.readBool();
        if (gotPresent != present) {
            ++mismatches;
            std::cerr << "DEC-PRESENT-MISMATCH " << name << " got=" << gotPresent
                      << " want=" << present << "\n";
        }
        if (gotPresent) {
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

    std::cout << "PktSelectAdvancementsTabParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

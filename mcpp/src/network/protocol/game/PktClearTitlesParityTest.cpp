// Parity gate for ClientboundClearTitlesPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktClearTitlesParity.java ground truth).
//
// The packet has a single field `boolean resetTimes`. Its body is exactly:
//   write : FriendlyByteBuf.writeBoolean(this.resetTimes)
//   read  : input.readBoolean()
// (net.minecraft.network.protocol.game.ClientboundClearTitlesPacket lines 9-24.)
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body, so the
// whole wire payload is a single byte: 0x01 if resetTimes else 0x00.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeBool(b) / readBool() are byte-for-byte / value-for-value the same as the
// real codec (writeBoolean -> single byte 1/0).
//
//   pkt_clear_titles_parity [--cases mcpp/build/pkt_clear_titles.tsv]
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

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_clear_titles.tsv";
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

        // ENC <resetTimes-dec(0/1)> <readableBytes-dec> <hex>
        std::string flagStr, lenStr, expHex;
        if (!std::getline(ss, flagStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;
        bool resetTimes = (std::stoi(flagStr) != 0);
        size_t expLen = (size_t)std::stoull(lenStr);

        // write(): FriendlyByteBuf.writeBoolean(resetTimes).
        PacketBuffer enc;
        enc.writeBool(resetTimes);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH resetTimes=" << resetTimes << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH resetTimes=" << resetTimes << " got="
                      << enc.data().size() << " want=" << expLen << "\n";
        }

        // read(): input.readBoolean() must recover the exact boolean.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        bool gotFlag = dec.readBool();
        if (gotFlag != resetTimes) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex << " got=" << gotFlag
                      << " want=" << resetTimes << "\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING resetTimes=" << resetTimes << " remaining="
                      << dec.remaining() << "\n";
        }
    }

    std::cout << "PktClearTitlesParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

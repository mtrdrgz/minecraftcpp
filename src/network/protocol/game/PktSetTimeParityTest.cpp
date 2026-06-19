// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundSetTimePacket
// vs the REAL ClientboundSetTimePacket.STREAM_CODEC (tools/PktSetTimeParity.java).
//
// For every PKT case the C++ ClientboundSetTimePacket::encode must reproduce the
// Java wire bytes byte-for-byte, AND decode(Java bytes) must recover the same
// gameTime + ordered clock-update entries.
//
//   pkt_set_time_parity [--cases mcpp/build/pkt_set_time.tsv]
//
// Row: PKT <gameTime-dec> <nEntries> <entries|-> <hex>
//   entries = space-separated <regId>:<totalTicks-dec>:<partialBits-08x>:<rateBits-08x>
#include "ClientboundSetTimePacket.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::net::ClientboundSetTimePacket;
using mc::net::ClockUpdateEntry;
using mc::net::ClockNetworkState;
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

float bitsToFloat(uint32_t bits) { return std::bit_cast<float>(bits); }

// Parse one "id:totalTicks:partialBits:rateBits" token.
ClockUpdateEntry parseEntry(const std::string& tok) {
    ClockUpdateEntry e;
    std::vector<std::string> parts;
    std::string cur;
    for (char c : tok) {
        if (c == ':') { parts.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    parts.push_back(cur);
    // id : totalTicks : partialBits(hex) : rateBits(hex)
    e.registryId       = (int32_t)std::stol(parts.at(0));
    e.state.totalTicks = (int64_t)std::stoll(parts.at(1));
    e.state.partialTick = bitsToFloat((uint32_t)std::stoul(parts.at(2), nullptr, 16));
    e.state.rate        = bitsToFloat((uint32_t)std::stoul(parts.at(3), nullptr, 16));
    return e;
}

std::vector<ClockUpdateEntry> parseEntries(const std::string& field) {
    std::vector<ClockUpdateEntry> out;
    if (field == "-" || field.empty()) return out;
    std::istringstream ss(field);
    std::string tok;
    while (ss >> tok) out.push_back(parseEntry(tok));
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_set_time.tsv";
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
        std::string tag, gtStr, nStr, entriesStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "PKT") continue;
        if (!std::getline(ss, gtStr, '\t') || !std::getline(ss, nStr, '\t') ||
            !std::getline(ss, entriesStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        int64_t gameTime = (int64_t)std::stoll(gtStr);
        size_t nEntries = (size_t)std::stoul(nStr);
        std::vector<ClockUpdateEntry> entries = parseEntries(entriesStr);

        if (entries.size() != nEntries) {
            ++mismatches;
            std::cerr << "ENTRY-COUNT mismatch gt=" << gameTime
                      << " declared=" << nEntries << " parsed=" << entries.size() << "\n";
            continue;
        }

        // (1) ENCODE: build packet, encode, compare to Java hex byte-for-byte.
        ClientboundSetTimePacket pkt;
        pkt.gameTime = gameTime;
        pkt.clockUpdates = entries;

        PacketBuffer enc;
        pkt.encode(enc);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH gt=" << gameTime << " n=" << nEntries
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE: decode Java bytes, verify fields bit-exact + wire order.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        ClientboundSetTimePacket back = ClientboundSetTimePacket::decode(dec);

        bool ok = (back.gameTime == gameTime) && (back.clockUpdates.size() == nEntries);
        for (size_t i = 0; ok && i < nEntries; ++i) {
            const ClockUpdateEntry& a = back.clockUpdates[i];
            const ClockUpdateEntry& b = entries[i];
            if (a.registryId != b.registryId) ok = false;
            if (a.state.totalTicks != b.state.totalTicks) ok = false;
            // float bit-exact comparison
            if (std::bit_cast<uint32_t>(a.state.partialTick) != std::bit_cast<uint32_t>(b.state.partialTick)) ok = false;
            if (std::bit_cast<uint32_t>(a.state.rate)        != std::bit_cast<uint32_t>(b.state.rate))        ok = false;
        }
        // ensure the buffer was fully consumed
        if (dec.remaining() != 0) ok = false;

        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH gt=" << gameTime << " n=" << nEntries << "\n";
        }
    }

    std::cout << "PktSetTimeParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

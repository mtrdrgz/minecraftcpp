// Parity test for the volume()/pitch() float pair of every public-static
//   net.minecraft.world.level.block.SoundType  constant  (Minecraft 26.1.2).
//
// Ground truth: mcpp/tools/SoundTypeParity.java (calls the REAL net.minecraft
// SoundType + getVolume()/getPitch()). This test reconstructs the same facts from
// the ported table in world/level/block/SoundType.h and compares them BIT-FOR-BIT:
// the constant count (int) and, per constant, the IEEE-754 bit patterns of volume
// and pitch (via std::bit_cast<uint32_t>(float) vs the GT %08x hex).
//
// The five SoundEvent fields are registry/asset-coupled and intentionally NOT
// ported/compared — see SoundType.h.
//
//   sound_type_parity --cases mcpp/build/sound_type.tsv

#include "SoundType.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using mc::block::SOUND_TYPE_COUNT;
using mc::block::SOUND_TYPES;
using mc::block::SoundTypeData;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

int64_t toI(const std::string& s) { return static_cast<int64_t>(std::stoll(s)); }

// Parse a lowercase 8-hex-digit string into a 32-bit IEEE-754 bit pattern.
uint32_t toU32Hex(const std::string& s) {
    return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
}

const SoundTypeData* findByName(const std::string& name) {
    for (const auto& st : SOUND_TYPES)
        if (name == st.name) return &st;
    return nullptr;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: sound_type_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& line, const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH(" << why << ") " << line << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF safety
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];

        if (tag == "CNT") {
            ++total;
            if (toI(p[1]) != static_cast<int64_t>(SOUND_TYPE_COUNT))
                fail(line, "CNT got=" + std::to_string(SOUND_TYPE_COUNT));
        } else if (tag == "ST") {
            // ST <name> <volume %08x> <pitch %08x>
            ++total;
            const std::string& name = p[1];
            const SoundTypeData* st = findByName(name);
            if (st == nullptr) { fail(line, "ST unknown name=" + name); continue; }
            uint32_t expVol = toU32Hex(p[2]);
            uint32_t expPit = toU32Hex(p[3]);
            uint32_t gotVol = std::bit_cast<uint32_t>(st->volume);
            uint32_t gotPit = std::bit_cast<uint32_t>(st->pitch);
            if (gotVol != expVol || gotPit != expPit)
                fail(line, "ST " + name + " vol=" + p[2] + "/got" +
                           [&] { char b[9]; std::snprintf(b, sizeof b, "%08x", gotVol); return std::string(b); }() +
                           " pit=" + p[3] + "/got" +
                           [&] { char b[9]; std::snprintf(b, sizeof b, "%08x", gotPit); return std::string(b); }());
        } else {
            ++total;
            fail(line, "UNKNOWN_TAG");
        }
    }

    std::cout << "SoundType cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

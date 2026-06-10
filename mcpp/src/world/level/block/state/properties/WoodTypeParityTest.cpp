// Parity test for the DATA surface of
//   net.minecraft.world.level.block.state.properties.WoodType  (Minecraft 26.1.2)
//
// Ground truth: mcpp/tools/WoodTypeParity.java (calls the REAL net.minecraft
// classes). This test reconstructs the same facts from the ported table in
// world/level/block/state/properties/WoodType.h and compares them bit/byte
// exactly: names + setType names (strings), the BlockSetType booleans +
// pressure-plate sensitivity ordinal (ints), the sound category (string), the
// fence-gate close/open locations (strings), the count, and CODEC name
// resolution.
//
//   wood_type_parity --cases mcpp/build/wood_type.tsv
//
// The SoundType/SoundEvent OBJECT graphs are intentionally NOT compared (they
// are registry/asset-coupled and not byte-comparable across Java<->C++); only
// their portable identity/location facts are gated. See WoodType.h.

#include "WoodType.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using mc::block::state::properties::byName;
using mc::block::state::properties::WoodSoundCategory;
using mc::block::state::properties::WOOD_TYPE_COUNT;
using mc::block::state::properties::WOOD_TYPES;
using mc::block::state::properties::WoodTypeData;

namespace {
// Tab-split that PRESERVES empty fields (including empty leading/middle ones),
// so CODEC probe inputs like "" / "oak " round-trip exactly.
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
int32_t toI(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

const char* categoryName(WoodSoundCategory c) {
    switch (c) {
        case WoodSoundCategory::DEFAULT_WOOD: return "DEFAULT_WOOD";
        case WoodSoundCategory::CHERRY_WOOD:  return "CHERRY_WOOD";
        case WoodSoundCategory::NETHER_WOOD:  return "NETHER_WOOD";
        case WoodSoundCategory::BAMBOO_WOOD:  return "BAMBOO_WOOD";
    }
    return "UNKNOWN";
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: wood_type_parity --cases <tsv>\n"; return 2; }
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
            if (toI(p[1]) != WOOD_TYPE_COUNT) fail(line, "CNT");
        } else if (tag == "WT") {
            ++total;
            // WT idx name setName hand wind arrows sens category closeLoc openLoc
            int idx = toI(p[1]);
            if (idx < 0 || idx >= WOOD_TYPE_COUNT) { fail(line, "WT idx out of range"); continue; }
            const WoodTypeData& w = WOOD_TYPES[static_cast<std::size_t>(idx)];

            if (std::string(w.name) != p[2]) { fail(line, "WT name=" + std::string(w.name)); continue; }
            if (std::string(w.setType.name) != p[3]) { fail(line, "WT setName=" + std::string(w.setType.name)); continue; }
            if ((w.setType.canOpenByHand ? 1 : 0) != toI(p[4])) { fail(line, "WT hand"); continue; }
            if ((w.setType.canOpenByWindCharge ? 1 : 0) != toI(p[5])) { fail(line, "WT wind"); continue; }
            if ((w.setType.canButtonBeActivatedByArrows ? 1 : 0) != toI(p[6])) { fail(line, "WT arrows"); continue; }
            if (static_cast<int32_t>(w.setType.pressurePlateSensitivity) != toI(p[7])) { fail(line, "WT sens"); continue; }
            if (std::string(categoryName(w.soundCategory)) != p[8]) { fail(line, "WT category=" + std::string(categoryName(w.soundCategory))); continue; }
            if (std::string(w.fenceGateCloseLocation) != p[9]) { fail(line, "WT closeLoc=" + std::string(w.fenceGateCloseLocation)); continue; }
            if (std::string(w.fenceGateOpenLocation) != p[10]) { fail(line, "WT openLoc=" + std::string(w.fenceGateOpenLocation)); continue; }
        } else if (tag == "CODEC") {
            ++total;
            // CODEC input present resolvedName
            const std::string& input = p[1];
            int present = toI(p[2]);
            const std::string& resolved = p[3];
            auto r = byName(input);
            int gotPresent = r.has_value() ? 1 : 0;
            std::string gotResolved = r.has_value() ? std::string(r->name) : "-";
            if (gotPresent != present || gotResolved != resolved)
                fail(line, "CODEC present=" + std::to_string(gotPresent) + " resolved=" + gotResolved);
        } else {
            ++total;
            fail(line, "UNKNOWN_TAG");
        }
    }

    std::cout << "WoodType cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

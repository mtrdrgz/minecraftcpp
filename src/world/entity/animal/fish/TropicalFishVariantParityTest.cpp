// Parity test for the pure static variant math of
// net.minecraft.world.entity.animal.fish.TropicalFish (MC 26.1.2).
// Ground truth: tools/TropicalFishVariantParity.java vs the REAL class/enums.
//
// Verifies, bit/decimal-exact:
//   Pattern.getPackedId()        (base.id | index << 8)
//   Pattern.byId(int)            (ByIdMap.sparse default KOB)
//   TropicalFish.packVariant(...) (& 65535 | << 16 | << 24 with 0xFF masks)
//   TropicalFish.getBaseColor/getPatternColor(int) (arithmetic >> then & 0xFF
//                                                    then DyeColor.byId)
//   TropicalFish.getPattern(int) (Pattern.byId(packed & 65535))
//
//   tropical_fish_variant_parity --cases mcpp/build/tropical_fish_variant.tsv

#include "world/entity/animal/fish/TropicalFishVariant.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fish = mc::world::entity::animal::fish;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
long long ll(const std::string& s) { return std::stoll(s); }

// Patterns by Java ordinal == declaration order in TropicalFishVariant.h.
fish::Pattern patternByOrdinal(std::int32_t ord) {
    return static_cast<fish::Pattern>(ord);
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: tropical_fish_variant_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != ll(exp)) fail(l + " got=" + std::to_string(got));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "PATTERN") {
            // PATTERN <ordinal> <packedId> <baseId>
            auto pat = patternByOrdinal(static_cast<std::int32_t>(ll(p[1])));
            eqI(fish::getPackedId(pat), p[2], line);
            eqI(fish::baseId(fish::patternBase(pat)), p[3], line);
        } else if (t == "PATTERNBYID") {
            // PATTERNBYID <queryPackedId> <resultOrdinal>
            auto q = static_cast<std::int32_t>(ll(p[1]));
            eqI(static_cast<std::int32_t>(fish::patternById(q)), p[2], line);
        } else if (t == "PACK") {
            // PACK <patternOrdinal> <baseColorId> <patternColorId> <packedVariant>
            auto pat = patternByOrdinal(static_cast<std::int32_t>(ll(p[1])));
            auto baseColorId = static_cast<std::int32_t>(ll(p[2]));
            auto patColorId = static_cast<std::int32_t>(ll(p[3]));
            eqI(fish::packVariant(pat, baseColorId, patColorId), p[4], line);
        } else if (t == "BASECOLOR") {
            // BASECOLOR <packedVariant> <baseColorId>
            auto packed = static_cast<std::int32_t>(ll(p[1]));
            eqI(fish::getBaseColorId(packed), p[2], line);
        } else if (t == "PATCOLOR") {
            // PATCOLOR <packedVariant> <patternColorId>
            auto packed = static_cast<std::int32_t>(ll(p[1]));
            eqI(fish::getPatternColorId(packed), p[2], line);
        } else if (t == "GETPAT") {
            // GETPAT <packedVariant> <resultPatternOrdinal>
            auto packed = static_cast<std::int32_t>(ll(p[1]));
            eqI(static_cast<std::int32_t>(fish::getPattern(packed)), p[2], line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "TropicalFishVariant checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

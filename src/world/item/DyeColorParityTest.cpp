// Parity test for net.minecraft.world.item.DyeColor (MC 26.1.2). Ground truth:
// tools/DyeColorParity.java vs the real enum. Verifies all 16 colors' data
// (id/name/textureDiffuseColor/mapColor id+col/fireworkColor/textColor) plus
// byId (ByIdMap.continuous ZERO) and byFireworkColor, bit-exact.
//
//   dye_color_parity --cases mcpp/build/dye_color.tsv

#include "DyeColor.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace dye = mc::world::item;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
long long ll(const std::string& s) { return std::stoll(s); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: dye_color_parity --cases <tsv>\n";
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
    auto eqS = [&](std::string_view got, const std::string& exp, const std::string& l) {
        if (got != exp) fail(l + " got=" + std::string(got));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "DYE") {
            // DYE <id> <name> <tdc> <mapColorId> <mapColorCol> <fireworkColor> <textColor>
            int id = static_cast<int>(ll(p[1]));
            const dye::DyeColorData& d = dye::DYE_COLORS[static_cast<std::size_t>(id)];
            eqI(dye::getId(d), p[1], line);
            eqS(dye::getName(d), p[2], line);
            eqI(dye::getTextureDiffuseColor(d), p[3], line);
            eqI(dye::getMapColor(d).id, p[4], line);
            eqI(dye::getMapColor(d).col, p[5], line);
            eqI(dye::getFireworkColor(d), p[6], line);
            eqI(dye::getTextColor(d), p[7], line);
        } else if (t == "BYID") {
            // BYID <queryId> <resultId>
            std::int32_t q = static_cast<std::int32_t>(ll(p[1]));
            eqI(dye::getId(dye::byId(q)), p[2], line);
        } else if (t == "BYFW") {
            // BYFW <queryColor> <resultIndex>   (-1 == no match)
            std::int32_t q = static_cast<std::int32_t>(ll(p[1]));
            eqI(dye::byFireworkColorIndex(q), p[2], line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "DyeColor cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

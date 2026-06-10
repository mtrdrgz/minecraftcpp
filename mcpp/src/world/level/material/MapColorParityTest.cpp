// Parity test for net.minecraft.world.level.material.MapColor. Ground truth:
// tools/MapColorParity.java vs the real class. Verifies the col/id table, the
// Brightness modifiers, and the pure color math (calculateARGBColor /
// getColorFromPackedId / getPackedId), bit-exact (all values are ints).
//
//   map_color_parity --cases mcpp/build/map_color.tsv

#include "MapColor.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mat = mc::material;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int i(const std::string& s) { return std::stoi(s); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: map_color_parity --cases <tsv>\n";
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
        if (got != std::stoll(exp)) fail(l + " got=" + std::to_string(got));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "TABLE") {
            // TABLE <queryId> <mc.id> <mc.col>
            mat::MapColor mc = mat::mapColorByIdUnsafe(i(p[1]));
            eqI(mc.id, p[2], line);
            eqI(mc.col, p[3], line);
        } else if (t == "BRIGHTNESS") {
            // BRIGHTNESS <bid> <b.id> <b.modifier>
            mat::MapColorBrightness b = mat::mapColorBrightnessByIdUnsafe(i(p[1]));
            eqI(b.id, p[2], line);
            eqI(b.modifier, p[3], line);
        } else if (t == "CALC") {
            // CALC <id> <bid> <argb>
            mat::MapColor mc = mat::mapColorByIdUnsafe(i(p[1]));
            mat::MapColorBrightness b = mat::mapColorBrightnessByIdUnsafe(i(p[2]));
            eqI(mat::calculateARGBColor(mc, b), p[3], line);
        } else if (t == "PACKED") {
            // PACKED <packedId> <argb>
            eqI(mat::getColorFromPackedId(i(p[1])), p[2], line);
        } else if (t == "GETPACKED") {
            // GETPACKED <id> <bid> <signedByte>
            mat::MapColor mc = mat::mapColorByIdUnsafe(i(p[1]));
            mat::MapColorBrightness b = mat::mapColorBrightnessByIdUnsafe(i(p[2]));
            eqI(static_cast<int>(mat::getPackedId(mc, b)), p[3], line);
        } else {
            fail(line + " [unknown tag]");
        }
    }

    std::cout << "MapColor cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

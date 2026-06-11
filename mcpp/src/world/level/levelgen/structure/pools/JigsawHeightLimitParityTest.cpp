// Parity test for the PURE world-height-limit check in
//   net.minecraft.world.level.levelgen.structure.pools.JigsawPlacement
//     static boolean isStartTooCloseToWorldHeightLimits(
//         LevelHeightAccessor, DimensionPadding, BoundingBox)   (26.1.2).
//
// Ground truth: tools/JigsawHeightLimitParity.java drives the REAL private static
// method via reflection (over LevelHeightAccessor.create(...), the real
// DimensionPadding record incl. its ZERO sentinel, and real BoundingBoxes) and
// emits a TSV. We recompute each row from JigsawHeightLimit.h and compare. The
// result is a pure boolean of integer comparisons, so the gate is exact.
//
//   jigsaw_height_limit_parity --cases mcpp/build/jigsaw_height_limit.tsv
//
// Unknown first-tab tags are SKIPPED (not counted), so any stray stdout from the
// real <clinit>/Bootstrap can't corrupt the count.

#include "JigsawHeightLimit.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure::pools;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return static_cast<int>(std::stoll(s)); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: jigsaw_height_limit_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    auto fail = [&](const std::string& detail) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << detail << "\n";
    };

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = splitTabs(line);
        if (p.empty() || p[0] != "CHK") continue;  // skip non-tag / log lines
        if (p.size() < 9) continue;

        // CHK worldMinY worldMaxY padBottom padTop isZeroSentinel bbMinY bbMaxY result
        int worldMinY = toi(p[1]);
        int worldMaxY = toi(p[2]);
        DimensionPadding pad{toi(p[3]), toi(p[4]), toi(p[5]) != 0};
        HeightSpanBox bb{toi(p[6]), toi(p[7])};
        bool expected = toi(p[8]) != 0;

        ++total;
        bool got = isStartTooCloseToWorldHeightLimits(worldMinY, worldMaxY, pad, bb);
        if (got != expected)
            fail(line + " got=" + (got ? "1" : "0"));
    }

    std::cout << "JigsawHeightLimit checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

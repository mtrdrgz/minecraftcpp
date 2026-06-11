// Parity test for the PURE static bounding-box probe
//   net.minecraft.world.level.levelgen.structure.structures.MineshaftPieces
//     .MineShaftStairs.findStairs(...)
// ported in MineshaftStairsBox.h. Ground truth is produced by
// tools/MineshaftStairsBoxParity.java driving the REAL 26.1.2 method through an
// inline StructurePieceAccessor.
//
// Each TSV row carries the (collisionPresent, foot, direction) input and the
// expected (box-or-null) output; we recompute via findStairs() and compare the six
// int fields exactly. Box ints are plain decimal (no float), so the gate is exact.
//
//   mineshaft_stairs_box_parity --cases mcpp/build/mineshaft_stairs_box.tsv

#include "MineshaftStairsBox.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;
using mc::levelgen::structure::structures::findStairs;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int32_t toi(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: mineshaft_stairs_box_parity --cases <tsv>\n"; return 2; }

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
        // Skip any stray stdout lines whose first field isn't our tag.
        if (p.empty() || p[0] != "FSB") continue;
        // FSB collisionPresent footX footY footZ dirOrd hasResult r6(6) -> 13 fields.
        if (p.size() < 13) { fail("short row: " + line); continue; }
        ++total;

        bool collisionPresent = toi(p[1]) != 0;
        int32_t footX = toi(p[2]);
        int32_t footY = toi(p[3]);
        int32_t footZ = toi(p[4]);
        Direction dir = static_cast<Direction>(toi(p[5]));

        int hasResult = toi(p[6]);

        std::optional<BoundingBox> got = findStairs(collisionPresent, footX, footY, footZ, dir);

        if (hasResult == 0) {
            if (got.has_value())
                fail(line + " | expected null, got box "
                     + std::to_string(got->minX) + "," + std::to_string(got->minY) + "," + std::to_string(got->minZ)
                     + ".." + std::to_string(got->maxX) + "," + std::to_string(got->maxY) + "," + std::to_string(got->maxZ));
        } else {
            BoundingBox exp(toi(p[7]), toi(p[8]), toi(p[9]),
                            toi(p[10]), toi(p[11]), toi(p[12]));
            if (!got.has_value()) {
                fail(line + " | expected box, got null");
            } else if (!(*got == exp)) {
                fail(line + " | got "
                     + std::to_string(got->minX) + "," + std::to_string(got->minY) + "," + std::to_string(got->minZ)
                     + ".." + std::to_string(got->maxX) + "," + std::to_string(got->maxY) + "," + std::to_string(got->maxZ));
            }
        }
    }

    std::cout << "MineshaftStairsBox checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

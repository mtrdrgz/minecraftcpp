// Parity test for the PURE static bounding-box probe
//   net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces
//     .FillerCorridor.findPieceBox(...)
// ported in StrongholdPieceBox.h. Ground truth is produced by
// tools/StrongholdPieceBoxParity.java driving the REAL 26.1.2 method through an
// inline StructurePieceAccessor.
//
// Each TSV row carries the (collision-box-or-none, foot, direction) input and the
// expected (box-or-null) output; we recompute via findPieceBox() and compare the
// six int fields exactly. Box ints are plain decimal (no float), so the gate is
// exact.
//
//   stronghold_piece_box_parity --cases mcpp/build/stronghold_piece_box.tsv

#include "StrongholdPieceBox.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;
using mc::levelgen::structure::structures::findPieceBox;

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
    if (casesPath.empty()) { std::cerr << "usage: stronghold_piece_box_parity --cases <tsv>\n"; return 2; }

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
        if (p.empty() || p[0] != "FPB") continue;
        // FPB hasColl c6(6) footX footY footZ dirOrd hasResult r6(6)  -> 19 fields.
        if (p.size() < 19) { fail("short row: " + line); continue; }
        ++total;

        int hasColl = toi(p[1]);
        std::optional<BoundingBox> collision;
        if (hasColl) {
            collision = BoundingBox(toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]), toi(p[7]));
        }
        int32_t footX = toi(p[8]);
        int32_t footY = toi(p[9]);
        int32_t footZ = toi(p[10]);
        Direction dir = static_cast<Direction>(toi(p[11]));

        int hasResult = toi(p[12]);

        std::optional<BoundingBox> got = findPieceBox(collision, footX, footY, footZ, dir);

        if (hasResult == 0) {
            if (got.has_value())
                fail(line + " | expected null, got box "
                     + std::to_string(got->minX) + "," + std::to_string(got->minY) + "," + std::to_string(got->minZ)
                     + ".." + std::to_string(got->maxX) + "," + std::to_string(got->maxY) + "," + std::to_string(got->maxZ));
        } else {
            BoundingBox exp(toi(p[13]), toi(p[14]), toi(p[15]),
                            toi(p[16]), toi(p[17]), toi(p[18]));
            if (!got.has_value()) {
                fail(line + " | expected box, got null");
            } else if (!(*got == exp)) {
                fail(line + " | got "
                     + std::to_string(got->minX) + "," + std::to_string(got->minY) + "," + std::to_string(got->minZ)
                     + ".." + std::to_string(got->maxX) + "," + std::to_string(got->maxY) + "," + std::to_string(got->maxZ));
            }
        }
    }

    std::cout << "StrongholdPieceBox checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

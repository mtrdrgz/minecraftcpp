// Parity test for the PURE template-placement-position math nested in
//   net.minecraft.world.level.levelgen.structure.structures.IglooPieces
//     .IglooPiece.makePosition(Identifier, BlockPos, int)   (26.1.2).
//
// Ground truth: tools/IglooPiecePositionParity.java drives the REAL private
// static makePosition via reflection (over the REAL hard-coded OFFSETS map) and
// emits a TSV. We recompute each row from IglooPiecePosition.h and compare. The
// result is a pure-integer BlockPos, so the gate is exact (decimal compare).
//
//   igloo_piece_position_parity --cases mcpp/build/igloo_piece_position.tsv
//
// The Java SharedConstants/Bootstrap may log unrelated lines to stdout; any row
// whose first tab-field is not the known tag (POS) is SKIPPED (not counted, not a
// mismatch), so stray stdout can't corrupt the count.

#include "IglooPiecePosition.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure::igloo;

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
        std::cerr << "usage: igloo_piece_position_parity --cases <tsv>\n";
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
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "POS") {
            // templateKey px py pz depth | ox oy oz
            if (p.size() != 9) continue;  // malformed/partial line: skip, don't count
            BlockPos got = makePosition(p[1], BlockPos(toi(p[2]), toi(p[3]), toi(p[4])), toi(p[5]));
            int ex = toi(p[6]), ey = toi(p[7]), ez = toi(p[8]);
            ++total;
            if (got.x != ex || got.y != ey || got.z != ez)
                fail(line + " got=" + std::to_string(got.x) + "," + std::to_string(got.y) +
                            "," + std::to_string(got.z));
        }
        // Unknown tags (Bootstrap chatter) are silently skipped.
    }

    std::cout << "IglooPiecePosition checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

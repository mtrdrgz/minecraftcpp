// Parity test for the pure sculk-charge decay-penalty arithmetic of
// net.minecraft.world.level.block.SculkBlock.getDecayPenalty (26.1.2).
// Ground truth: tools/SculkDecayPenaltyParity.java (invokes the REAL private
// static method reflectively).
//
//   sculk_decay_penalty_parity --cases <sculk_decay_penalty.tsv>
//
// Each row carries the spreader noGrowthRadius, the (dx,dy,dz) of the charge
// position relative to a FIXED catalyst origin, the charge, and the expected
// decay penalty as a decimal int. The C++ side reconstructs the two BlockPos
// from origin + delta and recomputes getDecayPenalty, comparing the EXACT
// integer (no tolerance). The origin constants MUST match the Java driver.
//
// Row tag:
//   DECAY  <noGrowthRadius>  <dx>  <dy>  <dz>  <charge>  <penalty>

#include "world/level/block/SculkDecayPenalty.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
// Fixed catalyst origin — MUST equal ORIGIN_X/Y/Z in SculkDecayPenaltyParity.java.
constexpr int ORIGIN_X = 100;
constexpr int ORIGIN_Y = -13;
constexpr int ORIGIN_Z = -250;

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: sculk_decay_penalty_parity --cases <tsv>\n";
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

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "DECAY") {
            // DECAY <noGrowthRadius> <dx> <dy> <dz> <charge> <penalty>
            if (p.size() != 7) { fail("BADROW " + line); continue; }
            int noGrowthRadius = std::stoi(p[1]);
            int dx = std::stoi(p[2]);
            int dy = std::stoi(p[3]);
            int dz = std::stoi(p[4]);
            int charge = std::stoi(p[5]);
            int want = std::stoi(p[6]);
            int got = mc::block_sculk::getDecayPenalty(
                noGrowthRadius,
                ORIGIN_X + dx, ORIGIN_Y + dy, ORIGIN_Z + dz,
                ORIGIN_X, ORIGIN_Y, ORIGIN_Z,
                charge);
            if (got != want) {
                std::ostringstream os;
                os << line << "  (got=" << got << ")";
                fail(os.str());
            }
        } else {
            fail("UNKNOWN_TAG " + tag);
        }
    }

    std::cout << "SculkDecayPenalty checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

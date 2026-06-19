// Parity test for the PURE 2D-grid helper SimpleGrid nested in
//   net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces (26.1.2).
//
// Ground truth: tools/WoodlandMansionGridParity.java drives the REAL private
// static SimpleGrid (via reflection) through a deterministic op script and emits
// every get()/edgesTo() probe to a TSV. We replay the SAME ops against our own
// SimpleGrid (WoodlandMansionGrid.h) and compare each probe. All values are
// decimal ints / 0|1 booleans, so the gate is byte-exact.
//
//   woodland_mansion_grid_parity --cases mcpp/build/woodland_mansion_grid.tsv
//
// TSV ops (replayed in order; GET/EDGE rows are checked against the real class):
//   GRID  gid w h outside
//   SET   gid x y value
//   RECT  gid x0 y0 x1 y1 value
//   SETIF gid x y ifValue value
//   GET   gid x y result
//   EDGE  gid x y ifValue result(0/1)

#include "WoodlandMansionGrid.h"

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::structure::structures::SimpleGrid;

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
    if (casesPath.empty()) { std::cerr << "usage: woodland_mansion_grid_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    std::map<int, std::unique_ptr<SimpleGrid>> grids;

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    auto fail = [&](const std::string& tag, const std::string& detail) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << tag << " " << detail << "\n";
    };

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = splitTabs(line);
        const std::string& tag = p[0];
        int gid = toi(p[1]);

        if (tag == "GRID") {
            // gid w h outside
            grids[gid] = std::make_unique<SimpleGrid>(toi(p[2]), toi(p[3]), toi(p[4]));
        } else if (tag == "SET") {
            // gid x y value
            grids[gid]->set(toi(p[2]), toi(p[3]), toi(p[4]));
        } else if (tag == "RECT") {
            // gid x0 y0 x1 y1 value
            grids[gid]->set(toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]));
        } else if (tag == "SETIF") {
            // gid x y ifValue value
            grids[gid]->setif(toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]));
        } else if (tag == "GET") {
            // gid x y | result
            ++total;
            int got = grids[gid]->get(toi(p[2]), toi(p[3]));
            if (got != toi(p[4]))
                fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "EDGE") {
            // gid x y ifValue | result(0/1)
            ++total;
            bool got = grids[gid]->edgesTo(toi(p[2]), toi(p[3]), toi(p[4]));
            if ((got ? 1 : 0) != toi(p[5]))
                fail(tag, line + " got=" + std::to_string(got ? 1 : 0));
        } else {
            fail("UNKNOWN_TAG", tag);
        }
    }

    std::cout << "WoodlandMansionGrid checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

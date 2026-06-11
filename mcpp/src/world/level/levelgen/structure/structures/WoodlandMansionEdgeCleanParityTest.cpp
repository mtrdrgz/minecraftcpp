// Parity test for the PURE grid-cleanup pass nested in
//   net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces (26.1.2):
//     MansionGrid.isHouse(SimpleGrid, int, int)   (static)
//     MansionGrid.cleanEdges(SimpleGrid)          (instance, but reads only its grid arg)
//
// Ground truth: tools/WoodlandMansionEdgeCleanParity.java drives the REAL private
// nested classes (via reflection; cleanEdges invoked on an Unsafe-allocated
// MansionGrid) through a deterministic op script and emits, after each cleanEdges
// pass, the full grid via get() plus isHouse() over the OOB-extended ring, and the
// pass's returned `touched`. We replay the SAME script against our own
// SimpleGrid + mansionGridCleanEdges/mansionGridIsHouse (WoodlandMansionEdgeClean.h)
// and compare every probe. All values are decimal ints / 0|1 booleans, so the
// gate is byte-exact.
//
//   woodland_mansion_edge_clean_parity --cases mcpp/build/woodland_mansion_edge_clean.tsv
//
// TSV ops (replayed in order; HOUSE/CLEAN/GET rows are checked against the REAL class):
//   GRID  gid w h outside
//   SET   gid x y value
//   RECT  gid x0 y0 x1 y1 value
//   HOUSE gid x y result(0/1)
//   CLEAN gid touched(0/1)        -- run ONE in-place cleanEdges pass; result = return
//   GET   gid x y result

#include "WoodlandMansionEdgeClean.h"

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::structure::structures::SimpleGrid;
using mc::levelgen::structure::structures::mansionGridCleanEdges;
using mc::levelgen::structure::structures::mansionGridIsHouse;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return static_cast<int>(std::stoll(s)); }

// Known leading tags; rows that don't start with one are skipped (in case the
// REAL ctor/bootstrap logs to stdout — defensive, per the gate contract).
bool knownTag(const std::string& t) {
    return t == "GRID" || t == "SET" || t == "RECT" || t == "HOUSE" || t == "CLEAN" || t == "GET";
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: woodland_mansion_edge_clean_parity --cases <tsv>\n";
        return 2;
    }

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
        if (p.empty() || !knownTag(p[0])) continue;  // skip stray log lines
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
        } else if (tag == "HOUSE") {
            // gid x y | result(0/1)
            ++total;
            bool got = mansionGridIsHouse(*grids[gid], toi(p[2]), toi(p[3]));
            if ((got ? 1 : 0) != toi(p[4]))
                fail(tag, line + " got=" + std::to_string(got ? 1 : 0));
        } else if (tag == "CLEAN") {
            // gid | touched(0/1)  -- mutate the grid in place, exactly like Java
            ++total;
            bool got = mansionGridCleanEdges(*grids[gid]);
            if ((got ? 1 : 0) != toi(p[2]))
                fail(tag, line + " got=" + std::to_string(got ? 1 : 0));
        } else if (tag == "GET") {
            // gid x y | result
            ++total;
            int got = grids[gid]->get(toi(p[2]), toi(p[3]));
            if (got != toi(p[4]))
                fail(tag, line + " got=" + std::to_string(got));
        }
    }

    std::cout << "WoodlandMansionEdgeClean checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

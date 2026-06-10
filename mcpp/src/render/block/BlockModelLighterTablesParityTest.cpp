// Parity gate for the BlockModelLighter AO enum tables (render/block/BlockModelLighterTables.h) vs
// the reflection-dumped real AdjacencyInfo / AmbientVertexRemap / SizeInfo. Pure integer compare.
//
//   block_model_lighter_tables_parity --cases mcpp/build/bml_tables.tsv

#include "BlockModelLighterTables.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ao = mc::render::block::aolight;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// SizeInfo name -> expected C++ index.
int sizeIdxByName(const std::string& n) {
    if (n == "DOWN") return ao::S_DOWN;
    if (n == "UP") return ao::S_UP;
    if (n == "NORTH") return ao::S_NORTH;
    if (n == "SOUTH") return ao::S_SOUTH;
    if (n == "WEST") return ao::S_WEST;
    if (n == "EAST") return ao::S_EAST;
    if (n == "FLIP_DOWN") return ao::S_FLIP_DOWN;
    if (n == "FLIP_UP") return ao::S_FLIP_UP;
    if (n == "FLIP_NORTH") return ao::S_FLIP_NORTH;
    if (n == "FLIP_SOUTH") return ao::S_FLIP_SOUTH;
    if (n == "FLIP_WEST") return ao::S_FLIP_WEST;
    if (n == "FLIP_EAST") return ao::S_FLIP_EAST;
    return -999;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: block_model_lighter_tables_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long checks = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << why << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        if (t == "SIZE") {
            ++checks;
            if (sizeIdxByName(p[1]) != std::stoi(p[2])) fail("SizeInfo " + p[1]);
        } else if (t == "ADJ") {
            int dir = std::stoi(p[1]);
            const ao::AdjacencyInfo& a = ao::ADJACENCY[dir];
            for (int c = 0; c < 4; ++c) {
                ++checks;
                if (a.corners[c] != std::stoi(p[2 + c])) fail("ADJ d" + std::to_string(dir) + " corner" + std::to_string(c));
            }
            ++checks;
            if ((a.doNonCubicWeight ? 1 : 0) != std::stoi(p[6])) fail("ADJ d" + std::to_string(dir) + " doNonCubic");
            int base = 7;  // first vert weight column
            for (int v = 0; v < 4; ++v) {
                for (int k = 0; k < 8; ++k) {
                    ++checks;
                    if (a.vertWeights[v][k] != std::stoi(p[base + v * 8 + k]))
                        fail("ADJ d" + std::to_string(dir) + " v" + std::to_string(v) + "[" + std::to_string(k) + "]");
                }
            }
        } else if (t == "REMAP") {
            int dir = std::stoi(p[1]);
            for (int v = 0; v < 4; ++v) {
                ++checks;
                if (ao::AMBIENT_VERTEX_REMAP[dir][v] != std::stoi(p[2 + v]))
                    fail("REMAP d" + std::to_string(dir) + " vert" + std::to_string(v));
            }
        }
    }

    std::cout << "BlockModelLighterTables checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

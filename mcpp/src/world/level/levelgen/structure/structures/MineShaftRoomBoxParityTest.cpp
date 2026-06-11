// Bit-exact parity gate for the RNG-driven MineShaftRoom constructor geometry,
// ported in
//   world/level/levelgen/structure/structures/MineShaftRoomBox.h
// which reuses the certified BoundingBox.h (ctor) and the certified
// RandomSource.h/.cpp (seeded LegacyRandomSource via RandomSource::create).
//
// Ground truth (mcpp/tools/MineShaftRoomBoxParity.java) constructs the REAL
//   net.minecraft...structures.MineshaftPieces.MineShaftRoom(
//       int genDepth, RandomSource random, int west, int north,
//       MineshaftStructure.Type type)
// with RandomSource.create(seed), then reads back getBoundingBox(). We replay the
// same seeded RandomSource here, recompute the box, and compare bit-for-bit
// (all ints).
//
//   mine_shaft_room_box_parity --cases mcpp/build/mine_shaft_room_box.tsv

#include "world/level/levelgen/structure/structures/MineShaftRoomBox.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ms = mc::levelgen::structure::structures;
using mc::levelgen::structure::BoundingBox;

// INT_MIN/INT_MAX-safe decimal parse.
static int i32(const std::string& s) { return static_cast<int>(std::stoll(s)); }
static int64_t i64(const std::string& s) { return std::stoll(s); }

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: mine_shaft_room_box_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long checks = 0, mism = 0;
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
        if (p.empty()) continue;
        const std::string& tag = p[0];
        if (tag != "ROOM") { fail("UNKNOWN_TAG " + tag); continue; }
        ++checks;

        // ROOM <seed> <west> <north> <minX> <minY> <minZ> <maxX> <maxY> <maxZ>
        int64_t seed = i64(p[1]);
        int west = i32(p[2]);
        int north = i32(p[3]);
        int eMinX = i32(p[4]), eMinY = i32(p[5]), eMinZ = i32(p[6]);
        int eMaxX = i32(p[7]), eMaxY = i32(p[8]), eMaxZ = i32(p[9]);

        // Replay the SAME seeded RandomSource the Java ctor used.
        auto random = mc::levelgen::RandomSource::create(seed);
        BoundingBox bb = ms::makeRoomBox(*random, west, north);

        if (bb.minX != eMinX || bb.minY != eMinY || bb.minZ != eMinZ ||
            bb.maxX != eMaxX || bb.maxY != eMaxY || bb.maxZ != eMaxZ) {
            fail(line + " got bb=[" +
                 std::to_string(bb.minX) + "," + std::to_string(bb.minY) + "," +
                 std::to_string(bb.minZ) + " .. " +
                 std::to_string(bb.maxX) + "," + std::to_string(bb.maxY) + "," +
                 std::to_string(bb.maxZ) + "]");
        }
    }

    std::cout << "MineShaftRoom checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

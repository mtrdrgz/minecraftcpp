// Bit-exact parity gate for the RNG-driven MineShaftCorridor.findCorridorSize
// geometry, ported in
//   world/level/levelgen/structure/structures/MineShaftCorridorParity.h
// which reuses the certified BoundingBox.h (ctor + move) and the certified
// RandomSource.h/.cpp (seeded LegacyRandomSource via RandomSource::create).
//
// Ground truth (mcpp/tools/MineShaftCorridorParity.java) calls the REAL
//   net.minecraft...structures.MineshaftPieces.MineShaftCorridor.findCorridorSize(
//       StructurePieceAccessor, RandomSource, footX, footY, footZ, Direction)
// with RandomSource.create(seed) and a no-collision StructurePieceAccessor, then
// reads back the returned BoundingBox (or null). We replay the same seeded
// RandomSource here with a never-collides predicate and recompute, comparing
// bit-for-bit (all ints).
//
//   mine_shaft_corridor_parity --cases mcpp/build/mine_shaft_corridor.tsv

#include "world/level/levelgen/structure/structures/MineShaftCorridorParity.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace ms = mc::levelgen::structure::structures;
using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;

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
        std::cerr << "usage: mine_shaft_corridor_parity --cases <tsv>\n";
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

    // The parity accessor never reports a collision (matches the Java
    // no-collision StructurePieceAccessor: findCollisionPiece == null).
    const std::function<bool(const BoundingBox&)> neverCollides =
        [](const BoundingBox&) { return false; };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        if (tag != "CORR") { fail("UNKNOWN_TAG " + tag); continue; }
        ++checks;

        // CORR <seed> <footX> <footY> <footZ> <dirOrd> <found> <minX> <minY> <minZ> <maxX> <maxY> <maxZ>
        int64_t seed = i64(p[1]);
        int footX = i32(p[2]);
        int footY = i32(p[3]);
        int footZ = i32(p[4]);
        int dirOrd = i32(p[5]);
        int eFound = i32(p[6]);
        int eMinX = i32(p[7]), eMinY = i32(p[8]), eMinZ = i32(p[9]);
        int eMaxX = i32(p[10]), eMaxY = i32(p[11]), eMaxZ = i32(p[12]);

        Direction dir = static_cast<Direction>(dirOrd);

        // Replay the SAME seeded RandomSource the Java call used.
        auto random = mc::levelgen::RandomSource::create(seed);
        std::optional<BoundingBox> got =
            ms::findCorridorSize(*random, footX, footY, footZ, dir, neverCollides);

        int gFound = got.has_value() ? 1 : 0;
        if (gFound != eFound) {
            fail(line + " got found=" + std::to_string(gFound));
            continue;
        }
        if (gFound == 0) {
            // null on both sides: nothing more to compare.
            continue;
        }

        const BoundingBox& bb = *got;
        if (bb.minX != eMinX || bb.minY != eMinY || bb.minZ != eMinZ ||
            bb.maxX != eMaxX || bb.maxY != eMaxY || bb.maxZ != eMaxZ) {
            fail(line + " got bb=[" +
                 std::to_string(bb.minX) + "," + std::to_string(bb.minY) + "," +
                 std::to_string(bb.minZ) + " .. " +
                 std::to_string(bb.maxX) + "," + std::to_string(bb.maxY) + "," +
                 std::to_string(bb.maxZ) + "]");
        }
    }

    std::cout << "MineShaftCorridor checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

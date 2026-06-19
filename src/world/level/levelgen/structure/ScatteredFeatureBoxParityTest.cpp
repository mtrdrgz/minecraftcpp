// Bit-exact parity gate for the RNG-driven ScatteredFeaturePiece constructor
// geometry (world bounding box + orientation/rotation/mirror), ported in
//   world/level/levelgen/structure/ScatteredFeaturePieceBox.h
// which reuses the already-certified StructurePieceMath.h (makeBoundingBox /
// setOrientation) and the certified RandomSource.h/.cpp (seeded LegacyRandomSource).
//
// Ground truth (mcpp/tools/ScatteredFeatureBoxParity.java) constructs the REAL
//   net.minecraft...structures.SwampHutPiece(random, west, north)
//   net.minecraft...structures.DesertPyramidPiece(random, west, north)
// with RandomSource.create(seed) and reads back getBoundingBox()/orientation/
// getRotation()/getMirror(). We replay the same seeded RandomSource here and
// recompute, comparing bit-for-bit (all ints).
//
//   scattered_feature_box_parity --cases mcpp/build/scattered_feature_box.tsv

#include "world/level/levelgen/structure/ScatteredFeaturePieceBox.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace sfp = mc::levelgen::structure::piece;

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
        std::cerr << "usage: scattered_feature_box_parity --cases <tsv>\n";
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
        if (tag != "SWAMP" && tag != "DESERT") { fail("UNKNOWN_TAG " + tag); continue; }
        ++checks;

        // <tag> <seed> <west> <north> <dirOrd> <rotOrd> <mirOrd> <minX> <minY> <minZ> <maxX> <maxY> <maxZ>
        int64_t seed = i64(p[1]);
        int west = i32(p[2]);
        int north = i32(p[3]);
        int eDir = i32(p[4]);
        int eRot = i32(p[5]);
        int eMir = i32(p[6]);
        int eMinX = i32(p[7]), eMinY = i32(p[8]), eMinZ = i32(p[9]);
        int eMaxX = i32(p[10]), eMaxY = i32(p[11]), eMaxZ = i32(p[12]);

        // Replay the SAME seeded RandomSource the Java ctor used.
        auto random = mc::levelgen::RandomSource::create(seed);
        sfp::ScatteredFeaturePieceCtor r =
            (tag == "SWAMP")
                ? sfp::makeSwampHutPiece(*random, west, north)
                : sfp::makeDesertPyramidPiece(*random, west, north);

        int gDir = static_cast<int>(r.orientation);
        int gRot = static_cast<int>(r.rotation);
        int gMir = static_cast<int>(r.mirror);

        if (gDir != eDir || gRot != eRot || gMir != eMir ||
            r.boundingBox.minX != eMinX || r.boundingBox.minY != eMinY || r.boundingBox.minZ != eMinZ ||
            r.boundingBox.maxX != eMaxX || r.boundingBox.maxY != eMaxY || r.boundingBox.maxZ != eMaxZ) {
            fail(line + " got dir=" + std::to_string(gDir) + " rot=" + std::to_string(gRot) +
                 " mir=" + std::to_string(gMir) + " bb=[" +
                 std::to_string(r.boundingBox.minX) + "," + std::to_string(r.boundingBox.minY) + "," +
                 std::to_string(r.boundingBox.minZ) + " .. " +
                 std::to_string(r.boundingBox.maxX) + "," + std::to_string(r.boundingBox.maxY) + "," +
                 std::to_string(r.boundingBox.maxZ) + "]");
        }
    }

    std::cout << "ScatteredFeatureBox checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

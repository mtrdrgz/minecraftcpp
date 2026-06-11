// Parity test for the PURE cluster-ruin placement geometry of the REAL 26.1.2
//   net.minecraft.world.level.levelgen.structure.structures.OceanRuinPieces
// covered by OceanRuinClusterGeometry.h.
//
// Ground truth: tools/OceanRuinClusterParity.java drives the REAL methods
// (allPositions via reflection + a recording RandomSource; StructureTemplate
// .transform / Vec3i.offset / BoundingBox.fromCorners / .intersects) and emits a
// TSV. We recompute each row from the header and compare bit-for-bit. All values
// are pure ints / booleans, so the gate is an exact decimal compare.
//
//   ocean_ruin_cluster_parity --cases mcpp/build/ocean_ruin_cluster.tsv
//
// Rows with an unknown leading tag (Bootstrap stdout chatter) are skipped, never
// counted — stray output can't corrupt the result.

#include "OceanRuinClusterGeometry.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure::oceanruin;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int32_t toi(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

Rotation rotOf(int32_t ord) {
    switch (ord) {
        case 0: return Rotation::NONE;
        case 1: return Rotation::CLOCKWISE_90;
        case 2: return Rotation::CLOCKWISE_180;
        default: return Rotation::COUNTERCLOCKWISE_90;
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: ocean_ruin_cluster_parity --cases <tsv>\n";
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

        if (tag == "ALLPOS") {
            // ALLPOS ox oy oz seed r0..r15 | (b.x b.y b.z)*8  -> 5 + 16 + 24 = 45
            if (p.size() != 45) continue;
            BlockPos origin{toi(p[1]), toi(p[2]), toi(p[3])};
            ClusterCandidateDraws d{};
            for (int i = 0; i < 16; ++i) d.raw[i] = toi(p[5 + i]);
            auto got = allPositions(origin, d);
            ++total;
            bool ok = true;
            for (int i = 0; i < 8; ++i) {
                int32_t ex = toi(p[21 + 3 * i]);
                int32_t ey = toi(p[21 + 3 * i + 1]);
                int32_t ez = toi(p[21 + 3 * i + 2]);
                if (got[i].x != ex || got[i].y != ey || got[i].z != ez) ok = false;
            }
            if (!ok) fail(line);
        } else if (tag == "PARENT") {
            // PARENT px pz rot | cX cY cZ minX minY minZ maxX maxY maxZ blX blY blZ -> 4 + 12 = 16
            if (p.size() != 16) continue;
            auto r = parentBox(BlockPos{toi(p[1]), 0, toi(p[2])}, rotOf(toi(p[3])));
            ++total;
            bool ok =
                r.parentCorner.x == toi(p[4]) && r.parentCorner.y == toi(p[5]) && r.parentCorner.z == toi(p[6]) &&
                r.parentBB.minX == toi(p[7]) && r.parentBB.minY == toi(p[8]) && r.parentBB.minZ == toi(p[9]) &&
                r.parentBB.maxX == toi(p[10]) && r.parentBB.maxY == toi(p[11]) && r.parentBB.maxZ == toi(p[12]) &&
                r.parentBottomLeft.x == toi(p[13]) && r.parentBottomLeft.y == toi(p[14]) && r.parentBottomLeft.z == toi(p[15]);
            if (!ok) fail(line);
        } else if (tag == "FIT") {
            // FIT posX posY posZ nextRot pMinX pMinY pMinZ pMaxX pMaxY pMaxZ
            //     | ncX ncY ncZ nMinX nMinY nMinZ nMaxX nMaxY nMaxZ fits  -> 11 + 10 = 21
            if (p.size() != 21) continue;
            BlockPos pos{toi(p[1]), toi(p[2]), toi(p[3])};
            Rotation nextRot = rotOf(toi(p[4]));
            BoundingBox parent{toi(p[5]), toi(p[6]), toi(p[7]), toi(p[8]), toi(p[9]), toi(p[10])};
            auto r = candidateFit(pos, nextRot, parent);
            ++total;
            bool ok =
                r.nextCorner.x == toi(p[11]) && r.nextCorner.y == toi(p[12]) && r.nextCorner.z == toi(p[13]) &&
                r.nextBB.minX == toi(p[14]) && r.nextBB.minY == toi(p[15]) && r.nextBB.minZ == toi(p[16]) &&
                r.nextBB.maxX == toi(p[17]) && r.nextBB.maxY == toi(p[18]) && r.nextBB.maxZ == toi(p[19]) &&
                (r.fits ? 1 : 0) == toi(p[20]);
            if (!ok) fail(line);
        }
        // Unknown tags (Bootstrap chatter) are silently skipped.
    }

    std::cout << "OceanRuinCluster checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

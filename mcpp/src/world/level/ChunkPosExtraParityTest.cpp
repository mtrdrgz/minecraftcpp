// Bit-exact parity test for the ChunkPos coordinate helpers.
// Reads the ground-truth TSV produced by ChunkPosExtraParity.java, recomputes
// each value via mcpp/src/world/level/ChunkPosExtra.h, and compares as ints.
//
//   POS  <x> <z> <minX> <minZ> <maxX> <maxZ> <midX> <midZ> <regX> <regZ> <rlX> <rlZ>
//   BLK  <x> <z> <off> <getBlockX> <getBlockZ>
//   DIST <thisX> <thisZ> <argX> <argZ> <chessboard> <distSq>
//
// All fields are decimal int32 (Java int -> int32_t). We compare exact bits by
// comparing the int32_t values directly.

#include "world/level/ChunkPosExtra.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cp = mc::chunkpos;

// Parse a decimal int that may be as large as Integer.MIN/MAX. stoll then
// narrow to int32_t reproduces Java's signed 32-bit value exactly.
static int32_t pi(const std::string& s) {
    return static_cast<int32_t>(std::stoll(s));
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: " << argv[0] << " --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long cases = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> p;
        std::string field;
        std::stringstream ss(line);
        while (std::getline(ss, field, '\t')) p.push_back(field);
        if (p.empty()) continue;

        const std::string& tag = p[0];

        if (tag == "POS") {
            // p: POS x z minX minZ maxX maxZ midX midZ regX regZ rlX rlZ
            if (p.size() != 13) { ++mism; continue; }
            int32_t x = pi(p[1]), z = pi(p[2]);
            int32_t eMinX = pi(p[3]), eMinZ = pi(p[4]);
            int32_t eMaxX = pi(p[5]), eMaxZ = pi(p[6]);
            int32_t eMidX = pi(p[7]), eMidZ = pi(p[8]);
            int32_t eRegX = pi(p[9]), eRegZ = pi(p[10]);
            int32_t eRlX = pi(p[11]), eRlZ = pi(p[12]);

            bool ok =
                cp::getMinBlockX(x) == eMinX &&
                cp::getMinBlockZ(z) == eMinZ &&
                cp::getMaxBlockX(x) == eMaxX &&
                cp::getMaxBlockZ(z) == eMaxZ &&
                cp::getMiddleBlockX(x) == eMidX &&
                cp::getMiddleBlockZ(z) == eMidZ &&
                cp::getRegionX(x) == eRegX &&
                cp::getRegionZ(z) == eRegZ &&
                cp::getRegionLocalX(x) == eRlX &&
                cp::getRegionLocalZ(z) == eRlZ;
            ++cases;
            if (!ok) {
                ++mism;
                if (mism <= 20)
                    std::cerr << "POS mismatch x=" << x << " z=" << z << "\n";
            }
        } else if (tag == "BLK") {
            // p: BLK x z off getBlockX getBlockZ
            if (p.size() != 6) { ++mism; continue; }
            int32_t x = pi(p[1]), z = pi(p[2]), off = pi(p[3]);
            int32_t eBX = pi(p[4]), eBZ = pi(p[5]);
            bool ok =
                cp::getBlockX(x, off) == eBX &&
                cp::getBlockZ(z, off) == eBZ;
            ++cases;
            if (!ok) {
                ++mism;
                if (mism <= 20)
                    std::cerr << "BLK mismatch x=" << x << " z=" << z
                              << " off=" << off << "\n";
            }
        } else if (tag == "DIST") {
            // p: DIST thisX thisZ argX argZ chessboard distSq
            if (p.size() != 7) { ++mism; continue; }
            int32_t tx = pi(p[1]), tz = pi(p[2]);
            int32_t ax = pi(p[3]), az = pi(p[4]);
            int32_t eCd = pi(p[5]), eDsq = pi(p[6]);
            bool ok =
                cp::getChessboardDistance(tx, tz, ax, az) == eCd &&
                cp::distanceSquared(tx, tz, ax, az) == eDsq;
            ++cases;
            if (!ok) {
                ++mism;
                if (mism <= 20)
                    std::cerr << "DIST mismatch this=(" << tx << "," << tz
                              << ") arg=(" << ax << "," << az << ")\n";
            }
        } else {
            // Unknown tag -> count as mismatch so silent drift can't hide.
            ++cases;
            ++mism;
            if (mism <= 20)
                std::cerr << "unknown tag: " << tag << "\n";
        }
    }

    std::cout << "ChunkPosExtra cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

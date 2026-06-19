// Bit-exact parity gate for net.minecraft.client.renderer.culling.Frustum and the
// org.joml.FrustumIntersection plane-extraction / AABB-intersection it drives.
//
// Two TAGs from FrustumParity.java ground truth:
//   PLANES <16 combinedMatBits> <24 planeBits>
//        -> run FrustumIntersection.set(combinedMatrix) and compare all 24
//           extracted+normalized plane floats BIT-FOR-BIT (proves set() matches).
//   TEST   <16 combinedMatBits> <camX..camZ d> <minX..maxZ d> <cube int> <vis 0/1>
//        -> set the frustum from the matrix, prepare(cam), and compare
//           cubeInFrustum (exact int) and isVisible (0/1).
#include "render/Frustum.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

using mc::render::frustum::Frustum;
using mc::render::frustum::FrustumIntersection;

static double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
static float  bf(const std::string& s) { return std::bit_cast<float>((uint32_t)std::stoul(s, nullptr, 16)); }
static uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }

// Build the 16 combined-matrix floats (column-major m00..m33) from p[i..i+15].
static void readMat(const std::vector<std::string>& p, size_t i, float m[16]) {
    for (int k = 0; k < 16; ++k) m[k] = bf(p[i + k]);
}

// Apply FrustumIntersection.set from the 16 column-major floats at m[].
static void setFromMat(FrustumIntersection& fi, const float m[16]) {
    fi.set(m[0],  m[1],  m[2],  m[3],
           m[4],  m[5],  m[6],  m[7],
           m[8],  m[9],  m[10], m[11],
           m[12], m[13], m[14], m[15], true);
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) { std::cerr << "usage: --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long n = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> p;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, '\t')) p.push_back(tok);
        if (p.empty()) continue;

        const std::string& tag = p[0];

        if (tag == "PLANES") {
            // p[1..16] = combined matrix, p[17..40] = 24 expected plane floats
            ++n;
            float m[16];
            readMat(p, 1, m);
            FrustumIntersection fi;
            setFromMat(fi, m);
            const float got[24] = {
                fi.nxX, fi.nxY, fi.nxZ, fi.nxW,
                fi.pxX, fi.pxY, fi.pxZ, fi.pxW,
                fi.nyX, fi.nyY, fi.nyZ, fi.nyW,
                fi.pyX, fi.pyY, fi.pyZ, fi.pyW,
                fi.nzX, fi.nzY, fi.nzZ, fi.nzW,
                fi.pzX, fi.pzY, fi.pzZ, fi.pzW,
            };
            bool ok = true;
            for (int k = 0; k < 24; ++k) {
                uint32_t exp = (uint32_t)std::stoul(p[17 + k], nullptr, 16);
                if (fb(got[k]) != exp) { ok = false; break; }
            }
            if (!ok) {
                ++mism;
                if (mism <= 20) std::cerr << "PLANES mismatch at line: " << line << "\n";
            }
        } else if (tag == "TEST") {
            // p[1..16] = combined matrix
            // p[17..19] = camX,camY,camZ (double bits)
            // p[20..25] = minX,minY,minZ,maxX,maxY,maxZ (double bits)
            // p[26] = expected cubeInFrustum (int), p[27] = isVisible (0/1)
            ++n;
            float m[16];
            readMat(p, 1, m);
            double camX = bd(p[17]), camY = bd(p[18]), camZ = bd(p[19]);
            double minX = bd(p[20]), minY = bd(p[21]), minZ = bd(p[22]);
            double maxX = bd(p[23]), maxY = bd(p[24]), maxZ = bd(p[25]);
            int expCube = std::stoi(p[26]);
            int expVis  = std::stoi(p[27]);

            Frustum fr;
            setFromMat(fr.intersection, m);
            fr.prepare(camX, camY, camZ);
            int gotCube = fr.cubeInFrustum(minX, minY, minZ, maxX, maxY, maxZ);
            int gotVis  = fr.isVisible(minX, minY, minZ, maxX, maxY, maxZ) ? 1 : 0;

            if (gotCube != expCube || gotVis != expVis) {
                ++mism;
                if (mism <= 20) {
                    std::cerr << "TEST mismatch: expCube=" << expCube << " gotCube=" << gotCube
                              << " expVis=" << expVis << " gotVis=" << gotVis
                              << " line: " << line << "\n";
                }
            }
        }
        // unknown tags ignored
    }

    std::cout << "Frustum cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

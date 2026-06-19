// Parity test for net.minecraft.world.entity.PositionMoveRotation (26.1.2).
// Ground truth: tools/PositionMoveRotationParity.java.
//
// Bit-exact: doubles/floats compared as raw IEEE-754 bits via std::bit_cast.
//   position_move_rotation_parity --cases mcpp/build/position_move_rotation.tsv
//
// TAGs:
//   PACK <mask> <repacked>
//   ABS  <mask> <sPx sPy sPz sDx sDy sDz>(hex64) <sYRot sXRot>(hex32)
//        <cPx cPy cPz cDx cDy cDz>(hex64) <cYRot cXRot>(hex32)
//        <oPx oPy oPz oDx oDy oDz>(hex64) <oYRot oXRot>(hex32)
//   WROT <Px Py Pz Dx Dy Dz>(hex64) <yRot xRot newYRot newXRot>(hex32)
//        <oPx oPy oPz oDx oDy oDz>(hex64) <oYRot oXRot>(hex32)

#include "PositionMoveRotation.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::PositionMoveRotation;
using mc::Relative;
using mc::Vec3;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
float  bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: position_move_rotation_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n"; };
    auto eD = [&](double got, const std::string& exp, const std::string& l) {
        if (db(got) != std::stoull(exp, nullptr, 16)) fail(l);
    };
    auto eF = [&](float got, const std::string& exp, const std::string& l) {
        if (fb(got) != static_cast<uint32_t>(std::stoul(exp, nullptr, 16))) fail(l);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "PACK") {
            int mask = std::stoi(p[1]);
            int repacked = mc::relativePack(mc::relativeUnpack(mask));
            if (repacked != std::stoi(p[2])) fail(line);
        } else if (t == "ABS") {
            int mask = std::stoi(p[1]);
            std::set<Relative> rel = mc::relativeUnpack(mask);
            int i = 2;
            Vec3 sPos(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            Vec3 sDelta(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            float sYRot = bf(p[i]); ++i;
            float sXRot = bf(p[i]); ++i;
            Vec3 cPos(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            Vec3 cDelta(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            float cYRot = bf(p[i]); ++i;
            float cXRot = bf(p[i]); ++i;

            PositionMoveRotation source(sPos, sDelta, sYRot, sXRot);
            PositionMoveRotation change(cPos, cDelta, cYRot, cXRot);
            PositionMoveRotation out = PositionMoveRotation::calculateAbsolute(source, change, rel);

            eD(out.position.x, p[i], line);     eD(out.position.y, p[i + 1], line);     eD(out.position.z, p[i + 2], line);     i += 3;
            eD(out.deltaMovement.x, p[i], line); eD(out.deltaMovement.y, p[i + 1], line); eD(out.deltaMovement.z, p[i + 2], line); i += 3;
            eF(out.yRot, p[i], line); ++i;
            eF(out.xRot, p[i], line); ++i;
        } else if (t == "WROT") {
            int i = 1;
            Vec3 pos(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            Vec3 delta(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            float yRot = bf(p[i]); ++i;
            float xRot = bf(p[i]); ++i;
            float nYRot = bf(p[i]); ++i;
            float nXRot = bf(p[i]); ++i;

            PositionMoveRotation pmr(pos, delta, yRot, xRot);
            PositionMoveRotation out = pmr.withRotation(nYRot, nXRot);

            eD(out.position.x, p[i], line);     eD(out.position.y, p[i + 1], line);     eD(out.position.z, p[i + 2], line);     i += 3;
            eD(out.deltaMovement.x, p[i], line); eD(out.deltaMovement.y, p[i + 1], line); eD(out.deltaMovement.z, p[i + 2], line); i += 3;
            eF(out.yRot, p[i], line); ++i;
            eF(out.xRot, p[i], line); ++i;
        } else {
            std::cerr << "unknown TAG " << t << "\n";
            return 2;
        }
    }

    std::cout << "PositionMoveRotation cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

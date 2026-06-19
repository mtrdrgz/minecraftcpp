// Parity test for net.minecraft...cuboid.CuboidRotation (element rotation transform
// + rescale). Ground truth: tools/CuboidRotationParity.java. Bit-exact (floats as
// raw IEEE-754 bits). Also exercises Joml.h rotationZYX (Euler) and rotation(angle,
// axis) + scale + transformDirection on the certified base.
//
//   cuboid_rotation_parity --cases mcpp/build/cuboid_rotation.tsv

#include "CuboidRotation.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cr = mc::render::model::cuboid;
namespace j = mc::render::model::joml;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: cuboid_rotation_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto m4 = [&](const cr::Matrix4f& m, const std::vector<std::string>& p, int o, const std::string& l) {
        const float got[16] = { m.m00,m.m01,m.m02,m.m03, m.m10,m.m11,m.m12,m.m13, m.m20,m.m21,m.m22,m.m23, m.m30,m.m31,m.m32,m.m33 };
        for (int k = 0; k < 16; ++k) if (fb(got[k]) != static_cast<uint32_t>(std::stoul(p[o + k], nullptr, 16))) { fail(l + " idx=" + std::to_string(k)); return; }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;
        if (t == "SAR") {
            cr::Matrix4f m = cr::computeTransform(cr::singleAxisTransformation(std::stoi(p[1]), bf(p[2])), std::stoi(p[3]) != 0);
            m4(m, p, 4, line);
        } else if (t == "EUL") {
            cr::Matrix4f m = cr::computeTransform(cr::eulerTransformation(bf(p[1]), bf(p[2]), bf(p[3])), std::stoi(p[4]) != 0);
            m4(m, p, 5, line);
        } else fail("UNKNOWN_TAG " + t);
    }

    std::cout << "CuboidRotation cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

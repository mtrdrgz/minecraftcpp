// Parity test for the pure org.joml.Quaternionf operations mirrored by
// render/model/QuaternionMath.h. Ground truth: tools/QuaternionMathParity.java
// (the REAL org.joml.Quaternionf from joml-1.10.8.jar, JOML default options).
//
// The deterministic ops (mul/conjugate/normalize/dot/lengthSquared/invert) use only
// exact-float inputs and plain a*b+c fma, so they are bit-exact. rotationXYZ uses the
// libm sin path; it matches the certified (float)std::sin((double)x) convention.
//
//   quaternionf_math_parity --cases mcpp/build/quaternionf_math.tsv

#include "QuaternionMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace q = mc::render::model::quat;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }

// Exact-float quaternion components — identical to QuaternionMathParity.java QS.
const float QS[15][4] = {
    {0.f, 0.f, 0.f, 1.f},
    {0.5f, 0.5f, 0.5f, 0.5f},
    {-0.5f, 0.5f, -0.5f, 0.5f},
    {0.5f, 0.f, 0.f, 0.5f},
    {0.f, 0.25f, 0.f, 0.75f},
    {0.25f, -0.25f, 0.5f, 1.f},
    {1.f, 0.f, 0.f, 0.f},
    {0.f, 1.f, 0.f, 0.f},
    {0.f, 0.f, 1.f, 0.f},
    {0.125f, 0.375f, -0.625f, 0.75f},
    {-0.5f, -0.5f, 0.5f, 0.5f},
    {2.f, -3.f, 4.f, -5.f},
    {0.0625f, 0.0625f, 0.0625f, 0.0625f},
    {-1.5f, 0.5f, 2.25f, -0.75f},
    {0.f, 0.f, 0.f, 2.f},
};
q::Quaternionf qof(int i) { return q::Quaternionf{QS[i][0], QS[i][1], QS[i][2], QS[i][3]}; }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: quaternionf_math_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n"; };

    // Compare the 4 quaternion floats (x,y,z,w) starting at field offset o in p.
    auto cmp4 = [&](const q::Quaternionf& g, const std::vector<std::string>& p, int o, const std::string& l) {
        const float got[4] = { g.x, g.y, g.z, g.w };
        for (int k = 0; k < 4; ++k)
            if (fb(got[k]) != static_cast<uint32_t>(std::stoul(p[o + k], nullptr, 16))) { fail(l + " idx=" + std::to_string(k)); return; }
    };
    auto eq1 = [&](float got, const std::string& exp, const std::string& l) {
        if (fb(got) != static_cast<uint32_t>(std::stoul(exp, nullptr, 16))) fail(l);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "MUL") {
            auto a = qof(std::stoi(p[1])); a.mul(qof(std::stoi(p[2]))); cmp4(a, p, 3, line);
        } else if (t == "CONJ") {
            auto a = qof(std::stoi(p[1])); a.conjugate(); cmp4(a, p, 2, line);
        } else if (t == "NORM") {
            auto a = qof(std::stoi(p[1])); a.normalize(); cmp4(a, p, 2, line);
        } else if (t == "INV") {
            auto a = qof(std::stoi(p[1])); a.invert(); cmp4(a, p, 2, line);
        } else if (t == "LENSQ") {
            eq1(qof(std::stoi(p[1])).lengthSquared(), p[2], line);
        } else if (t == "DOT") {
            eq1(qof(std::stoi(p[1])).dot(qof(std::stoi(p[2]))), p[3], line);
        } else if (t == "ROTXYZ") {
            q::Quaternionf a; a.rotationXYZ(bf(p[1]), bf(p[2]), bf(p[3])); cmp4(a, p, 4, line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "QuaternionMath cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

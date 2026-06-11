// Parity test for the pure com.mojang.math.GivensParameters record mirrored by
// render/model/GivensParameters.h. Ground truth: tools/GivensParametersParity.java
// (the REAL com.mojang.math.GivensParameters from client.jar, JOML default options).
//
// Certifies the THREE Matrix3f aroundX/Y/Z(Matrix3f) overloads (ungated elsewhere —
// MatrixUtil.h deliberately omits them) plus re-covers the scalar/Quaternionf helpers
// as a self-contained class. All math is plain float (cos()/sin() are fmul/fsub/fadd,
// fromUnnormalized uses joml::jinvsqrt, fromPositiveAngle uses joml::jsin/cosFromSin),
// so every row is bit-exact under -ffp-contract=off.
//
//   givens_parameters_parity --cases mcpp/build/givens_parameters.tsv

#include "GivensParameters.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace g = mc::render::model;
namespace j = mc::render::model::joml;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }

// Same seed Matrix3f the Java GT uses (column-major m00,m01,m02, m10,m11,m12, m20,m21,m22).
j::Matrix3f seed() {
    j::Matrix3f m;
    m.m00 = 1.5f; m.m01 = 2.5f; m.m02 = 3.5f;
    m.m10 = 4.5f; m.m11 = 5.5f; m.m12 = 6.5f;
    m.m20 = 7.5f; m.m21 = 8.5f; m.m22 = 9.5f;
    return m;
}
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: givens_parameters_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    // Compare N floats (got[]) against TSV fields p[o..o+N).
    auto cmpN = [&](const float* got, int n, const std::vector<std::string>& p, int o, const std::string& l) {
        for (int k = 0; k < n; ++k)
            if (fb(got[k]) != static_cast<uint32_t>(std::stoul(p[o + k], nullptr, 16))) {
                fail(l + " idx=" + std::to_string(k)); return;
            }
    };
    // giv 4-tuple: sinHalf, cosHalf, cos(), sin()
    auto cmpGiv = [&](const g::GivensParameters& gp, const std::vector<std::string>& p, int o, const std::string& l) {
        const float got[4] = { gp.sinHalf, gp.cosHalf, gp.cos(), gp.sin() };
        cmpN(got, 4, p, o, l);
    };
    auto matFloats = [](const j::Matrix3f& m, float out[9]) {
        out[0]=m.m00; out[1]=m.m01; out[2]=m.m02;
        out[3]=m.m10; out[4]=m.m11; out[5]=m.m12;
        out[6]=m.m20; out[7]=m.m21; out[8]=m.m22;
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "FROMUNNORM") {
            auto gp = g::GivensParameters::fromUnnormalized(bf(p[1]), bf(p[2]));
            cmpGiv(gp, p, 3, line);
        } else if (t == "INV") {
            auto gp = g::GivensParameters::fromUnnormalized(bf(p[1]), bf(p[2])).inverse();
            cmpGiv(gp, p, 3, line);
        } else if (t == "FROMANGLE") {
            auto gp = g::GivensParameters::fromPositiveAngle(bf(p[1]));
            cmpGiv(gp, p, 2, line);
        } else if (t == "AROUND_Q") {
            auto gp = g::GivensParameters::fromUnnormalized(bf(p[1]), bf(p[2]));
            j::Quaternionf qx{7,7,7,7}, qy{7,7,7,7}, qz{7,7,7,7};
            gp.aroundX(qx); gp.aroundY(qy); gp.aroundZ(qz);
            const float got[12] = { qx.x,qx.y,qx.z,qx.w, qy.x,qy.y,qy.z,qy.w, qz.x,qz.y,qz.z,qz.w };
            cmpN(got, 12, p, 3, line);
        } else if (t == "AROUND_MX" || t == "AROUND_MY" || t == "AROUND_MZ") {
            auto gp = g::GivensParameters::fromUnnormalized(bf(p[1]), bf(p[2]));
            j::Matrix3f m = seed();
            if (t == "AROUND_MX") gp.aroundX(m);
            else if (t == "AROUND_MY") gp.aroundY(m);
            else gp.aroundZ(m);
            float got[9]; matFloats(m, got);
            cmpN(got, 9, p, 3, line);
        } else if (t == "ANGLE_MX" || t == "ANGLE_MY" || t == "ANGLE_MZ") {
            auto gp = g::GivensParameters::fromPositiveAngle(bf(p[1]));
            j::Matrix3f m = seed();
            if (t == "ANGLE_MX") gp.aroundX(m);
            else if (t == "ANGLE_MY") gp.aroundY(m);
            else gp.aroundZ(m);
            float got[9]; matFloats(m, got);
            cmpN(got, 9, p, 2, line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "GivensParameters checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

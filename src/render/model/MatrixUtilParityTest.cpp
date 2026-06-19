// Parity test for the PURE static helpers of com.mojang.math.MatrixUtil (26.1.2),
// mirrored in render/model/MatrixUtil.h (which reuses the certified Joml.h).
// Ground truth: tools/MatrixUtilParity.java. Floats compared as raw IEEE-754 bits.
//
//   matrix_util_parity --cases mcpp/build/matrix_util.tsv

#include "MatrixUtil.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace j = mc::render::model::joml;
namespace mu = mc::render::model::matrixutil;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
uint32_t ux(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }
int di(const std::string& s) { return std::stoi(s); }

// Reconstruct a JOML Matrix4f from 17 fields starting at offset o:
//   16 column-major float bits (m00,m01,m02,m03,m10,...) then the properties() int.
j::Matrix4f loadM(const std::vector<std::string>& p, int o) {
    j::Matrix4f m;
    m.m00 = bf(p[o+0]);  m.m01 = bf(p[o+1]);  m.m02 = bf(p[o+2]);  m.m03 = bf(p[o+3]);
    m.m10 = bf(p[o+4]);  m.m11 = bf(p[o+5]);  m.m12 = bf(p[o+6]);  m.m13 = bf(p[o+7]);
    m.m20 = bf(p[o+8]);  m.m21 = bf(p[o+9]);  m.m22 = bf(p[o+10]); m.m23 = bf(p[o+11]);
    m.m30 = bf(p[o+12]); m.m31 = bf(p[o+13]); m.m32 = bf(p[o+14]); m.m33 = bf(p[o+15]);
    m.properties = di(p[o+16]);
    return m;
}
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: matrix_util_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    // compare a Matrix4f (16 floats + properties int) against payload at offset o.
    auto cmpM4p = [&](const j::Matrix4f& m, const std::vector<std::string>& p, int o, const std::string& l) {
        const float got[16] = { m.m00,m.m01,m.m02,m.m03, m.m10,m.m11,m.m12,m.m13,
                                m.m20,m.m21,m.m22,m.m23, m.m30,m.m31,m.m32,m.m33 };
        for (int k = 0; k < 16; ++k)
            if (fb(got[k]) != ux(p[o + k])) { fail(l + " mIdx=" + std::to_string(k)); return; }
        if (m.properties != di(p[o + 16])) fail(l + " props got=" + std::to_string(m.properties) + " exp=" + p[o + 16]);
    };
    auto eqB = [&](float got, const std::string& exp, const std::string& l) {
        if (fb(got) != ux(exp)) fail(l);
    };
    auto eqI = [&](int got, const std::string& exp, const std::string& l) {
        if (got != di(exp)) fail(l + " got=" + std::to_string(got) + " exp=" + exp);
    };
    // GivensParameters 4-tuple: sinHalf, cosHalf, cos(), sin() at offset o
    auto cmpGiv = [&](const mu::GivensParameters& g, const std::vector<std::string>& p, int o, const std::string& l) {
        if (fb(g.sinHalf) != ux(p[o]))     fail(l + " sinHalf");
        if (fb(g.cosHalf) != ux(p[o + 1])) fail(l + " cosHalf");
        if (fb(g.cos())   != ux(p[o + 2])) fail(l + " cos");
        if (fb(g.sin())   != ux(p[o + 3])) fail(l + " sin");
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];
        ++total;

        if (tag.rfind("MULCW_", 0) == 0) {
            // <tag> <factor> <input m4p[17]@o=2> <output m4p[17]@o=19>
            float f = bf(p[1]);
            j::Matrix4f m = loadM(p, 2);
            mu::mulComponentWise(m, f);
            cmpM4p(m, p, 19, line);
        } else if (tag.rfind("PROPS_", 0) == 0) {
            // <tag> <input m4p[17]@o=1> raw1 raw2 raw4 raw8 raw16 isPureTr isIdent
            int base = 1;
            const int bits[5] = {1, 2, 4, 8, 16};
            for (int k = 0; k < 5; ++k) {
                j::Matrix4f m = loadM(p, base);
                eqI(mu::checkPropertyRaw(m, bits[k]) ? 1 : 0, p[base + 17 + k], line + " rawBit=" + std::to_string(bits[k]));
            }
            {
                j::Matrix4f m = loadM(p, base);
                eqI(mu::isPureTranslation(m) ? 1 : 0, p[base + 17 + 5], line + " isPureTranslation");
            }
            {
                j::Matrix4f m = loadM(p, base);
                eqI(mu::isIdentity(m) ? 1 : 0, p[base + 17 + 6], line + " isIdentity");
            }
        } else if (tag == "GIV_FROMUNNORM") {
            mu::GivensParameters g = mu::GivensParameters::fromUnnormalized(bf(p[1]), bf(p[2]));
            cmpGiv(g, p, 3, line);
        } else if (tag == "GIV_INV") {
            mu::GivensParameters g = mu::GivensParameters::fromUnnormalized(bf(p[1]), bf(p[2])).inverse();
            cmpGiv(g, p, 3, line);
        } else if (tag == "GIV_AROUND") {
            mu::GivensParameters g = mu::GivensParameters::fromUnnormalized(bf(p[1]), bf(p[2]));
            j::Quaternionf qx; g.aroundX(qx);
            j::Quaternionf qy; g.aroundY(qy);
            j::Quaternionf qz; g.aroundZ(qz);
            eqB(qx.x, p[3], line + " qx.x"); eqB(qx.y, p[4], line + " qx.y"); eqB(qx.z, p[5], line + " qx.z"); eqB(qx.w, p[6], line + " qx.w");
            eqB(qy.x, p[7], line + " qy.x"); eqB(qy.y, p[8], line + " qy.y"); eqB(qy.z, p[9], line + " qy.z"); eqB(qy.w, p[10], line + " qy.w");
            eqB(qz.x, p[11], line + " qz.x"); eqB(qz.y, p[12], line + " qz.y"); eqB(qz.z, p[13], line + " qz.z"); eqB(qz.w, p[14], line + " qz.w");
        } else if (tag == "GIV_FROMANGLE") {
            mu::GivensParameters g = mu::GivensParameters::fromPositiveAngle(bf(p[1]));
            cmpGiv(g, p, 2, line);
        } else if (tag == "APPROX") {
            mu::GivensParameters g = mu::approxGivensQuat(bf(p[1]), bf(p[2]), bf(p[3]));
            cmpGiv(g, p, 4, line);
        } else if (tag == "QR") {
            mu::GivensParameters g = mu::qrGivensQuat(bf(p[1]), bf(p[2]));
            cmpGiv(g, p, 3, line);
        } else {
            --total; // unknown tag (shouldn't happen)
            std::cerr << "UNKNOWN TAG " << tag << "\n";
        }
    }

    std::cout << "MatrixUtil cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

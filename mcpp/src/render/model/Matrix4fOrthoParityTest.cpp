// Parity test for the PURE orthographic-projection setters of org.joml.Matrix4f
// (JOML 1.10.8), mirrored in render/model/Matrix4fOrtho.h (which reuses the
// certified Joml.h Matrix4f). Ground truth: tools/Matrix4fOrthoParity.java.
// Floats compared as raw IEEE-754 bits; properties() int compared exactly.
//
//   matrix4f_ortho_parity --cases mcpp/build/matrix4f_ortho.tsv

#include "Matrix4fOrtho.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace j = mc::render::model::joml;
namespace mo = mc::render::matrix4f_ortho;

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

// Fresh identity matrix (default Matrix4f: properties carries PROPERTY_IDENTITY).
j::Matrix4f ident() { return j::Matrix4f(); }
// Pre-dirtied matrix: a translation clears PROPERTY_IDENTITY (matches the Java
// `new Matrix4f().translation(3,-5,7)` driver), forcing the setter to re-identity.
j::Matrix4f dirty() { j::Matrix4f m; m.translation(3.0f, -5.0f, 7.0f); return m; }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: matrix4f_ortho_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    // compare a Matrix4f (16 floats column-major field order + properties int) at offset o.
    auto cmpM4p = [&](const j::Matrix4f& m, const std::vector<std::string>& p, int o, const std::string& l) {
        const float got[16] = { m.m00,m.m01,m.m02,m.m03, m.m10,m.m11,m.m12,m.m13,
                                m.m20,m.m21,m.m22,m.m23, m.m30,m.m31,m.m32,m.m33 };
        for (int k = 0; k < 16; ++k)
            if (fb(got[k]) != ux(p[o + k])) { fail(l + " mIdx=" + std::to_string(k)); return; }
        if (m.properties != di(p[o + 16]))
            fail(l + " props got=" + std::to_string(m.properties) + " exp=" + p[o + 16]);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];
        ++total;

        if (tag.rfind("ORTHO_", 0) == 0 || tag.rfind("ORTHOLH_", 0) == 0) {
            // <tag> l r b t zn zf (6 floats @1..6) z01 (@7) result m4p[17] @8
            float l = bf(p[1]), r = bf(p[2]), b = bf(p[3]), t = bf(p[4]), zn = bf(p[5]), zf = bf(p[6]);
            bool z01 = di(p[7]) != 0;
            bool dirtied = tag.find("_DIRTY") != std::string::npos;
            j::Matrix4f m = dirtied ? dirty() : ident();
            if (tag.rfind("ORTHOLH_", 0) == 0) mo::setOrthoLH(m, l, r, b, t, zn, zf, z01);
            else                                mo::setOrtho(m, l, r, b, t, zn, zf, z01);
            cmpM4p(m, p, 8, line);
        } else if (tag.rfind("ORTHOSYM_", 0) == 0 || tag.rfind("ORTHOSYMLH_", 0) == 0) {
            // <tag> w h zn zf (4 floats @1..4) z01 (@5) result m4p[17] @6
            float w = bf(p[1]), h = bf(p[2]), zn = bf(p[3]), zf = bf(p[4]);
            bool z01 = di(p[5]) != 0;
            bool dirtied = tag.find("_DIRTY") != std::string::npos;
            j::Matrix4f m = dirtied ? dirty() : ident();
            if (tag.rfind("ORTHOSYMLH_", 0) == 0) mo::setOrthoSymmetricLH(m, w, h, zn, zf, z01);
            else                                   mo::setOrthoSymmetric(m, w, h, zn, zf, z01);
            cmpM4p(m, p, 6, line);
        } else {
            --total;
            std::cerr << "UNKNOWN TAG " << tag << "\n";
        }
    }

    std::cout << "Matrix4fOrtho checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

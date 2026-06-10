// Parity test for the org.joml.Matrix4f projection builders ported in
// render/model/Matrix4fProjection.h (setOrtho / setPerspective / perspective).
// Ground truth: tools/Matrix4fProjParity.java, calling the real org.joml.Matrix4f.
// All 16 matrix floats are compared bit-for-bit (raw IEEE-754).
//
// setPerspective routes through org.joml.Math.tan = (float)Math.tan((double)x), a
// libm transcendental; the C++ port uses (float)std::tan((double)x). This gate
// certifies they agree bit-for-bit on the emitted inputs (a divergence would surface
// here as a mismatch on the SETPERSP/PERSP rows' m00/m11 columns).
//
//   matrix4f_proj_parity --cases mcpp/build/matrix4f_proj.tsv

#include "Matrix4fProjection.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace m4 = mc::render::model::matrix4f_proj;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16)));
}
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: matrix4f_proj_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n";
    };

    // Compare the 16 matrix floats of m against tokens p[o..o+16).
    auto cmp16 = [&](const m4::Matrix4f& m, const std::vector<std::string>& p, int o,
                     const std::string& l) {
        const float got[16] = {
            m.m00, m.m01, m.m02, m.m03,
            m.m10, m.m11, m.m12, m.m13,
            m.m20, m.m21, m.m22, m.m23,
            m.m30, m.m31, m.m32, m.m33};
        for (int k = 0; k < 16; ++k) {
            if (fb(got[k]) != static_cast<uint32_t>(std::stoul(p[o + k], nullptr, 16))) {
                fail(l + " idx=" + std::to_string(k));
                return;
            }
        }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "ORTHO") {
            // left right bottom top zNear zFar zZeroToOne | 16 floats
            m4::Matrix4f m;
            m.setOrtho(bf(p[1]), bf(p[2]), bf(p[3]), bf(p[4]), bf(p[5]), bf(p[6]),
                       p[7] == "1");
            cmp16(m, p, 8, line);
        } else if (t == "SETPERSP") {
            // fovy aspect zNear zFar zZeroToOne | 16 floats
            m4::Matrix4f m;
            m.setPerspective(bf(p[1]), bf(p[2]), bf(p[3]), bf(p[4]), p[5] == "1");
            cmp16(m, p, 6, line);
        } else if (t == "PERSP") {
            // fovy aspect zNear zFar zZeroToOne | 16 floats (fresh identity matrix)
            m4::Matrix4f m;
            m.perspective(bf(p[1]), bf(p[2]), bf(p[3]), bf(p[4]), p[5] == "1");
            cmp16(m, p, 6, line);
        } else {
            std::cerr << "unknown tag: " << t << "\n";
            return 2;
        }
    }

    std::cout << "Matrix4fProj cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

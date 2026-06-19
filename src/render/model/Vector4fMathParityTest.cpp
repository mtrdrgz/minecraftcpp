// Parity test for the PURE arithmetic of org.joml.Vector4f (JOML 1.10.8), mirrored
// in render/model/Vector4fMath.h (which reuses the certified Joml.h jfma / Matrix4f).
// Ground truth: tools/Vector4fMathParity.java. Floats compared as raw IEEE-754 bits;
// the matrix dispatch uses the matrix's stored properties() int (carried in the TSV).
//
//   vector4f_math_parity --cases mcpp/build/vector4f_math.tsv
//
// MUST be built with -ffp-contract=off so jfma's `a*b + c` is not fused (a HW FMA
// would be a 1-ULP divergence from JOML's default two-rounding Math.fma).

#include "Vector4fMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace j = mc::render::model::joml;
namespace vm = mc::render::vector4f_math;

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

// read a Vector4f (4 raw float fields) starting at p[o]
vm::Vector4f readV4(const std::vector<std::string>& p, int o) {
    return vm::Vector4f(bf(p[o]), bf(p[o + 1]), bf(p[o + 2]), bf(p[o + 3]));
}
// read a Matrix4f (16 raw float fields column-major + properties int) starting at p[o]
j::Matrix4f readM4p(const std::vector<std::string>& p, int o) {
    j::Matrix4f m;
    m.m00 = bf(p[o + 0]);  m.m01 = bf(p[o + 1]);  m.m02 = bf(p[o + 2]);  m.m03 = bf(p[o + 3]);
    m.m10 = bf(p[o + 4]);  m.m11 = bf(p[o + 5]);  m.m12 = bf(p[o + 6]);  m.m13 = bf(p[o + 7]);
    m.m20 = bf(p[o + 8]);  m.m21 = bf(p[o + 9]);  m.m22 = bf(p[o + 10]); m.m23 = bf(p[o + 11]);
    m.m30 = bf(p[o + 12]); m.m31 = bf(p[o + 13]); m.m32 = bf(p[o + 14]); m.m33 = bf(p[o + 15]);
    m.properties = di(p[o + 16]);
    return m;
}
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: vector4f_math_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    // compare a Vector4f (4 floats) against expected raw bits at p[o]
    auto cmpV4 = [&](const vm::Vector4f& v, const std::vector<std::string>& p, int o, const std::string& l) {
        const uint32_t got[4] = { fb(v.x), fb(v.y), fb(v.z), fb(v.w) };
        for (int k = 0; k < 4; ++k)
            if (got[k] != ux(p[o + k])) { fail(l + " vIdx=" + std::to_string(k)); return; }
    };
    // compare a single float (raw bits) at p[o]
    auto cmpF = [&](float f, const std::vector<std::string>& p, int o, const std::string& l) {
        if (fb(f) != ux(p[o])) fail(l + " scalar");
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "MUL") {
            // MUL \t mi \t vi \t srcV4[4]@3 \t m4p[17]@7 \t destV4[4]@24
            vm::Vector4f src = readV4(p, 3);
            j::Matrix4f m = readM4p(p, 7);
            vm::Vector4f dest;
            src.mul(m, dest);
            cmpV4(dest, p, 24, line);
        } else if (tag == "ADD" || tag == "SUB" || tag == "MULV" || tag == "DIV") {
            // <tag> \t aV4[4]@1 \t bV4[4]@5 \t resV4[4]@9
            vm::Vector4f a = readV4(p, 1), b = readV4(p, 5), dest;
            if (tag == "ADD") a.add(b, dest);
            else if (tag == "SUB") a.sub(b, dest);
            else if (tag == "MULV") a.mul(b, dest);
            else a.div(b, dest);
            cmpV4(dest, p, 9, line);
        } else if (tag == "DOT") {
            // DOT \t aV4@1 \t bV4@5 \t resF@9
            vm::Vector4f a = readV4(p, 1), b = readV4(p, 5);
            cmpF(a.dot(b), p, 9, line);
        } else if (tag == "DIST2") {
            vm::Vector4f a = readV4(p, 1), b = readV4(p, 5);
            cmpF(a.distanceSquared(b), p, 9, line);
        } else if (tag == "NEG") {
            // NEG \t aV4@1 \t resV4@5
            vm::Vector4f a = readV4(p, 1), dest;
            a.negate(dest);
            cmpV4(dest, p, 5, line);
        } else if (tag == "LEN2") {
            vm::Vector4f a = readV4(p, 1);
            cmpF(a.lengthSquared(), p, 5, line);
        } else if (tag == "LERP") {
            // LERP \t aV4@1 \t bV4@5 \t tF@9 \t resV4@10
            vm::Vector4f a = readV4(p, 1), b = readV4(p, 5), dest;
            float t = bf(p[9]);
            a.lerp(b, t, dest);
            cmpV4(dest, p, 10, line);
        } else if (tag == "FMAVV") {
            // FMAVV \t aV4@1 \t bV4@5 \t cV4@9 \t resV4@13
            vm::Vector4f a = readV4(p, 1), b = readV4(p, 5), c = readV4(p, 9), dest;
            a.fma(b, c, dest);
            cmpV4(dest, p, 13, line);
        } else if (tag == "FMASV") {
            // FMASV \t aV4@1 \t tF@5 \t bV4@6 \t resV4@10
            vm::Vector4f a = readV4(p, 1), b = readV4(p, 6), dest;
            float t = bf(p[5]);
            a.fma(t, b, dest);
            cmpV4(dest, p, 10, line);
        } else {
            --total;
            std::cerr << "UNKNOWN TAG " << tag << "\n";
        }
    }

    std::cout << "Vector4fMath checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

// Parity test for the self-contained com.mojang.math.SymmetricGroup3 port in
// render/model/SymmetricGroup3.h. Ground truth: tools/SymmetricGroup3Parity.java
// (the REAL enum from client.jar, JOML default options).
//
// Certifies the surfaces octahedral_parity does NOT cover:
//   * transformation()        — 9 floats per group, bit-exact (floatToRawIntBits)
//   * inverse().ordinal()      — direct INVERSE_TABLE entry
//   * permuteVector(Vector3f)  — float vector permutation (in place)
//   * permuteVector(Vector3i)  — int   vector permutation
//   * permuteAxis(int)         — axis-ordinal permutation
// plus re-covers permute(int)/compose(ordinal) so this gate stands alone.
//
//   symmetric_group3_parity --cases mcpp/build/symmetric_group3.tsv

#include "SymmetricGroup3.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mm = mc::render::model;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int i(const std::string& s) { return std::stoi(s); }
// hex IEEE-754 bits -> float
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
uint32_t hx(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: symmetric_group3_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "TRANSFORM") {
            // p[1]=ordinal, p[2..10]=m00,m01,m02, m10,m11,m12, m20,m21,m22 (hex bits)
            const mm::Matrix3f& m = mm::SymmetricGroup3::value(i(p[1])).transformation();
            const float got[9] = { m.m00, m.m01, m.m02, m.m10, m.m11, m.m12, m.m20, m.m21, m.m22 };
            for (int k = 0; k < 9; ++k)
                if (fb(got[k]) != hx(p[2 + k])) { fail(line + " idx=" + std::to_string(k)); break; }
        } else if (t == "INVERSE") {
            int got = mm::SymmetricGroup3::value(i(p[1])).inverse().ordinal();
            if (got != i(p[2])) fail(line);
        } else if (t == "PERMUTE") {
            int got = mm::SymmetricGroup3::value(i(p[1])).permute(i(p[2]));
            if (got != i(p[3])) fail(line);
        } else if (t == "PERMUTEAXIS") {
            int got = mm::SymmetricGroup3::value(i(p[1])).permuteAxis(i(p[2]));
            if (got != i(p[3])) fail(line);
        } else if (t == "PVECF") {
            // p[1]=ord, p[2..4]=input x,y,z bits, p[5..7]=expected out x,y,z bits
            mm::Vector3f v(bf(p[2]), bf(p[3]), bf(p[4]));
            mm::SymmetricGroup3::value(i(p[1])).permuteVector(v);
            if (fb(v.x) != hx(p[5]) || fb(v.y) != hx(p[6]) || fb(v.z) != hx(p[7])) fail(line);
        } else if (t == "PVECI") {
            // p[1]=ord, p[2..4]=input x,y,z (decimal), p[5..7]=expected out
            mm::Vector3i v(i(p[2]), i(p[3]), i(p[4]));
            mm::SymmetricGroup3::value(i(p[1])).permuteVector(v);
            if (v.x != i(p[5]) || v.y != i(p[6]) || v.z != i(p[7])) fail(line);
        } else if (t == "COMPOSE") {
            int got = mm::SymmetricGroup3::value(i(p[1])).compose(mm::SymmetricGroup3::value(i(p[2]))).ordinal();
            if (got != i(p[3])) fail(line);
        } else if (t == "PRIV") {
            // p[1]=ord, p[2..4]=private p0,p1,p2 == permute(0..2)
            const auto& g = mm::SymmetricGroup3::value(i(p[1]));
            if (g.permute(0) != i(p[2]) || g.permute(1) != i(p[3]) || g.permute(2) != i(p[4])) fail(line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "SymmetricGroup3 checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

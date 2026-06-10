// Parity test for the deterministic org.joml subset mirrored by render/model/Joml.h.
// Ground truth: tools/JomlMathParity.java. All matrices built from rotation(quaternion)
// with exact-float components (no sin/cos), so the gate is bit-exact (floats compared
// as raw IEEE-754 bits). Certifies the existing Joml.h render-math groundwork.
//
//   joml_math_parity --cases mcpp/build/joml_math.tsv

#include "Joml.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace j = mc::render::model::joml;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }

// Same exact-float quaternions as the GT tool.
const float QS[10][4] = {
    {0,0,0,1}, {0.5f,0.5f,0.5f,0.5f}, {0.5f,0,0,0.5f}, {0,0.25f,0,0.75f},
    {-0.5f,0.5f,-0.5f,0.5f}, {0.25f,-0.25f,0.5f,1.f}, {1,0,0,0}, {0,1,0,0},
    {0.125f,0.375f,-0.625f,0.75f}, {-0.5f,-0.5f,0.5f,0.5f}
};
j::Matrix4f rot(int i) {
    j::Matrix4f m; m.rotation(j::Quaternionf{QS[i][0], QS[i][1], QS[i][2], QS[i][3]}); return m;
}
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: joml_math_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n"; };

    // Compare 16 matrix floats starting at field offset `o` in p.
    auto m4 = [&](const j::Matrix4f& m, const std::vector<std::string>& p, int o, const std::string& l) {
        const float got[16] = { m.m00,m.m01,m.m02,m.m03, m.m10,m.m11,m.m12,m.m13, m.m20,m.m21,m.m22,m.m23, m.m30,m.m31,m.m32,m.m33 };
        for (int k = 0; k < 16; ++k) if (fb(got[k]) != static_cast<uint32_t>(std::stoul(p[o + k], nullptr, 16))) { fail(l + " idx=" + std::to_string(k)); return; }
    };
    auto eq1 = [&](float got, const std::string& exp, const std::string& l) {
        if (fb(got) != static_cast<uint32_t>(std::stoul(exp, nullptr, 16))) fail(l);
    };
    auto v3 = [&](float x, float y, float z, const std::vector<std::string>& p, int o, const std::string& l) {
        if (fb(x) != static_cast<uint32_t>(std::stoul(p[o], nullptr, 16)) ||
            fb(y) != static_cast<uint32_t>(std::stoul(p[o+1], nullptr, 16)) ||
            fb(z) != static_cast<uint32_t>(std::stoul(p[o+2], nullptr, 16))) fail(l);
    };
    auto m3 = [&](const j::Matrix3f& m, const std::vector<std::string>& p, int o, const std::string& l) {
        const float got[9] = { m.m00,m.m01,m.m02, m.m10,m.m11,m.m12, m.m20,m.m21,m.m22 };
        for (int k = 0; k < 9; ++k) if (fb(got[k]) != static_cast<uint32_t>(std::stoul(p[o + k], nullptr, 16))) { fail(l + " idx=" + std::to_string(k)); return; }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "ROT")            m4(rot(std::stoi(p[1])), p, 2, line);
        else if (t == "TRANSLATE") { auto m = rot(std::stoi(p[1])); m.translate(bf(p[2]), bf(p[3]), bf(p[4])); m4(m, p, 5, line); }
        else if (t == "SCALE")     { auto m = rot(std::stoi(p[1])); m.scale(bf(p[2]), bf(p[3]), bf(p[4])); m4(m, p, 5, line); }
        else if (t == "INVAFF")    { auto m = rot(std::stoi(p[1])).invertAffine(); m4(m, p, 2, line); }
        else if (t == "MUL")       { auto m = rot(std::stoi(p[1])); m.mul(rot(std::stoi(p[2]))); m4(m, p, 3, line); }
        else if (t == "TFPOS")     { j::Vector3f v{bf(p[2]),bf(p[3]),bf(p[4])}; rot(std::stoi(p[1])).transformPosition(v); v3(v.x,v.y,v.z,p,5,line); }
        else if (t == "TFDIR")     { j::Vector3f v{bf(p[2]),bf(p[3]),bf(p[4])}; rot(std::stoi(p[1])).transformDirection(v); v3(v.x,v.y,v.z,p,5,line); }
        else if (t == "VADD")      { j::Vector3f a{bf(p[1]),bf(p[2]),bf(p[3])}, b{bf(p[4]),bf(p[5]),bf(p[6])}; a.add(b); v3(a.x,a.y,a.z,p,7,line); }
        else if (t == "VSUB")      { j::Vector3f a{bf(p[1]),bf(p[2]),bf(p[3])}, b{bf(p[4]),bf(p[5]),bf(p[6])}; a.sub(b); v3(a.x,a.y,a.z,p,7,line); }
        else if (t == "VMUL")      { j::Vector3f a{bf(p[1]),bf(p[2]),bf(p[3])}, b{bf(p[4]),bf(p[5]),bf(p[6])}; a.mul(b); v3(a.x,a.y,a.z,p,7,line); }
        else if (t == "VDOT")      { j::Vector3f a{bf(p[1]),bf(p[2]),bf(p[3])}, b{bf(p[4]),bf(p[5]),bf(p[6])}; eq1(a.dot(b), p[7], line); }
        else if (t == "VDIV")      { j::Vector3f a{bf(p[1]),bf(p[2]),bf(p[3])}; a.div(bf(p[4])); v3(a.x,a.y,a.z,p,5,line); }
        else if (t == "VNORM")     { j::Vector3f a{bf(p[1]),bf(p[2]),bf(p[3])}; a.normalize(); v3(a.x,a.y,a.z,p,4,line); }
        else if (t == "TRINORM")   { j::Vector3f v0{bf(p[1]),bf(p[2]),bf(p[3])}, v1{bf(p[4]),bf(p[5]),bf(p[6])}, v2{bf(p[7]),bf(p[8]),bf(p[9])}, dest; j::geometryNormal(v0,v1,v2,dest); v3(dest.x,dest.y,dest.z,p,10,line); }
        else if (t == "M3SCALE")   { j::Matrix3f m; m.scaling(bf(p[1]),bf(p[2]),bf(p[3])); m3(m, p, 4, line); }
        else if (t == "M3MUL")     { j::Matrix3f m; m.scaling(bf(p[1]),bf(p[2]),bf(p[3])); j::Matrix3f r; r.scaling(bf(p[4]),bf(p[5]),bf(p[6])); m.mul(r); m3(m, p, 7, line); }
        else fail("UNKNOWN_TAG " + t);
    }

    std::cout << "JomlMath cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

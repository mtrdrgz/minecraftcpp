// Bit-exact parity gate for com.mojang.math.Axis (Minecraft 26.1.2).
// Reads the TSV emitted by tools/MathAxisParity.java and recomputes every
// rotation()/rotationDegrees() via render/model/MathAxis.h, comparing the four
// Quaternionf components (x,y,z,w) by RAW IEEE-754 bits only.
//
// TSV rows:
//   ROT     <AXIS>  <angleBits>  <xBits> <yBits> <zBits> <wBits>
//   ROTDEG  <AXIS>  <degBits>    <xBits> <yBits> <zBits> <wBits>
// where <AXIS> is one of XN/XP/YN/YP/ZN/ZP.
//
//   mcpp/build/math_axis_parity.exe --cases mcpp/build/math_axis.tsv

#include "render/model/MathAxis.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::render::model::joml::Quaternionf;
namespace ax = mc::render::model::math_axis;

static float    bf(const std::string& s) { return std::bit_cast<float>((uint32_t)std::stoul(s, nullptr, 16)); }
static uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }

// Dispatch to the right Axis provider's rotation(radians).
static Quaternionf rotationFor(const std::string& axis, float angle) {
    if (axis == "XN") return ax::XN_rotation(angle);
    if (axis == "XP") return ax::XP_rotation(angle);
    if (axis == "YN") return ax::YN_rotation(angle);
    if (axis == "YP") return ax::YP_rotation(angle);
    if (axis == "ZN") return ax::ZN_rotation(angle);
    if (axis == "ZP") return ax::ZP_rotation(angle);
    return Quaternionf{}; // unreachable for well-formed TSV
}

static Quaternionf rotationDegreesFor(const std::string& axis, float deg) {
    if (axis == "XN") return ax::XN_rotationDegrees(deg);
    if (axis == "XP") return ax::XP_rotationDegrees(deg);
    if (axis == "YN") return ax::YN_rotationDegrees(deg);
    if (axis == "YP") return ax::YP_rotationDegrees(deg);
    if (axis == "ZN") return ax::ZN_rotationDegrees(deg);
    if (axis == "ZP") return ax::ZP_rotationDegrees(deg);
    return Quaternionf{};
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: math_axis_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, '\t')) p.push_back(tok);
        if (p.empty()) continue;

        const std::string& tag = p[0];

        if (tag == "ROT" || tag == "ROTDEG") {
            // p[1]=axis  p[2]=angle/deg bits  p[3..6]=x,y,z,w bits
            if (p.size() < 7) continue;
            const std::string& axis = p[1];
            float in_a = bf(p[2]);
            uint32_t ex = (uint32_t)std::stoul(p[3], nullptr, 16);
            uint32_t ey = (uint32_t)std::stoul(p[4], nullptr, 16);
            uint32_t ez = (uint32_t)std::stoul(p[5], nullptr, 16);
            uint32_t ew = (uint32_t)std::stoul(p[6], nullptr, 16);

            Quaternionf got = (tag == "ROT") ? rotationFor(axis, in_a)
                                             : rotationDegreesFor(axis, in_a);
            ++cases;
            if (fb(got.x) != ex || fb(got.y) != ey || fb(got.z) != ez || fb(got.w) != ew) {
                ++mism;
                if (mism <= 20) {
                    std::cerr << "MISMATCH " << tag << " " << axis
                              << " in=" << p[2]
                              << " exp=" << p[3] << "," << p[4] << "," << p[5] << "," << p[6]
                              << " got=" << std::hex
                              << fb(got.x) << "," << fb(got.y) << ","
                              << fb(got.z) << "," << fb(got.w) << std::dec << "\n";
                }
            }
        }
        // Unknown tags are ignored (forward-compatible).
    }

    std::cout << "MathAxis cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

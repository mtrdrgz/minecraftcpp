// Bit-exact parity gate for com.mojang.math.Axis.of(Vector3f) (Minecraft 26.1.2).
//
// Reads the TSV emitted by tools/MathAxisArbitraryParity.java and recomputes
// every Axis.of(vector).rotation()/rotationDegrees() via
// render/model/MathAxisArbitrary.h (which wraps org.joml.Quaternionf.rotationAxis),
// comparing the four Quaternionf components (x,y,z,w) by RAW IEEE-754 bits only.
//
// TSV rows (9 columns each):
//   ROT     <axBits> <ayBits> <azBits> <angleBits>  <xBits> <yBits> <zBits> <wBits>
//   ROTDEG  <axBits> <ayBits> <azBits> <degBits>    <xBits> <yBits> <zBits> <wBits>
//
//   mcpp/build/math_axis_arbitrary_parity.exe --cases mcpp/build/math_axis_arbitrary.tsv

#include "render/model/MathAxisArbitrary.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::render::model::joml::Quaternionf;
using mc::render::model::joml::Vector3f;
namespace ax = mc::render::model::math_axis_arbitrary;

static float    bf(const std::string& s) { return std::bit_cast<float>((uint32_t)std::stoul(s, nullptr, 16)); }
static uint32_t hx(const std::string& s) { return (uint32_t)std::stoul(s, nullptr, 16); }
static uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: math_axis_arbitrary_parity --cases <tsv>\n";
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
        if (tag != "ROT" && tag != "ROTDEG") continue; // forward-compatible

        // p[1..3]=axis x,y,z bits  p[4]=angle/deg bits  p[5..8]=x,y,z,w bits
        if (p.size() < 9) continue;
        Vector3f axis{ bf(p[1]), bf(p[2]), bf(p[3]) };
        float in_a = bf(p[4]);
        uint32_t ex = hx(p[5]);
        uint32_t ey = hx(p[6]);
        uint32_t ez = hx(p[7]);
        uint32_t ew = hx(p[8]);

        Quaternionf got = (tag == "ROT") ? ax::of_rotation(axis, in_a)
                                         : ax::of_rotationDegrees(axis, in_a);
        ++cases;
        if (fb(got.x) != ex || fb(got.y) != ey || fb(got.z) != ez || fb(got.w) != ew) {
            ++mism;
            if (mism <= 20) {
                std::cerr << "MISMATCH " << tag
                          << " axis=" << p[1] << "," << p[2] << "," << p[3]
                          << " in=" << p[4]
                          << " exp=" << p[5] << "," << p[6] << "," << p[7] << "," << p[8]
                          << " got=" << std::hex
                          << fb(got.x) << "," << fb(got.y) << ","
                          << fb(got.z) << "," << fb(got.w) << std::dec << "\n";
            }
        }
    }

    std::cout << "MathAxisArbitrary cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

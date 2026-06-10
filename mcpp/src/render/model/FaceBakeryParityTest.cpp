// Parity test for the FaceBakery geometry helpers. Ground truth:
// tools/FaceBakeryParity.java vs the real (private, reflection-invoked) methods.
// Bit-exact (floats as raw IEEE-754 bits; direction ordinals as ints).
//
//   face_bakery_parity --cases mcpp/build/face_bakery.tsv

#include "FaceBakery.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fb = mc::render::model::fb;
namespace j = mc::render::model::joml;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fbits(float v) { return std::bit_cast<uint32_t>(v); }

const float QS[5][4] = {
    {0,0,0,1}, {0.5f,0.5f,0.5f,0.5f}, {0.5f,0,0,0.5f}, {-0.5f,0.5f,-0.5f,0.5f}, {0.125f,0.375f,-0.625f,0.75f}
};
j::Matrix4f rotM(int i) { j::Matrix4f m; m.rotation(j::Quaternionf{QS[i][0],QS[i][1],QS[i][2],QS[i][3]}); return m; }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: face_bakery_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto v3 = [&](const j::Vector3f& v, const std::vector<std::string>& p, int o, const std::string& l) {
        if (fbits(v.x) != static_cast<uint32_t>(std::stoul(p[o], nullptr, 16)) ||
            fbits(v.y) != static_cast<uint32_t>(std::stoul(p[o+1], nullptr, 16)) ||
            fbits(v.z) != static_cast<uint32_t>(std::stoul(p[o+2], nullptr, 16))) fail(l);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "C2C") { if (fbits(fb::cornerToCenter(bf(p[1]))) != static_cast<uint32_t>(std::stoul(p[2], nullptr, 16))) fail(line); }
        else if (t == "CEN2C") { if (fbits(fb::centerToCorner(bf(p[1]))) != static_cast<uint32_t>(std::stoul(p[2], nullptr, 16))) fail(line); }
        else if (t == "ROTV") {
            j::Vector3f vertex{bf(p[2]), bf(p[3]), bf(p[4])};
            j::Vector3f origin{bf(p[5]), bf(p[6]), bf(p[7])};
            fb::rotateVertexBy(vertex, origin, rotM(std::stoi(p[1])));
            v3(vertex, p, 8, line);
        }
        else if (t == "FCD") {
            int got = fb::findClosestDirection(j::Vector3f{bf(p[1]), bf(p[2]), bf(p[3])});
            if (got != std::stoi(p[4])) fail(line + " got=" + std::to_string(got));
        }
        else if (t == "FACE") {
            int got = fb::calculateFacing(j::Vector3f{bf(p[1]),bf(p[2]),bf(p[3])}, j::Vector3f{bf(p[4]),bf(p[5]),bf(p[6])}, j::Vector3f{bf(p[7]),bf(p[8]),bf(p[9])});
            if (got != std::stoi(p[10])) fail(line + " got=" + std::to_string(got));
        }
        else fail("UNKNOWN_TAG " + t);
    }

    std::cout << "FaceBakery cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

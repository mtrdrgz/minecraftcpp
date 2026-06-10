// Parity test for the FaceBakery geometry helpers. Ground truth:
// tools/FaceBakeryParity.java vs the real (private, reflection-invoked) methods.
// Bit-exact (floats as raw IEEE-754 bits; direction ordinals as ints).
//
//   face_bakery_parity --cases mcpp/build/face_bakery.tsv

#include "FaceBakery.h"
#include "CuboidRotation.h"

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
        else if (t == "BVP") {
            namespace cr = mc::render::model::cuboid;
            int facing = std::stoi(p[1]), index = std::stoi(p[2]);
            j::Vector3f from{bf(p[3]),bf(p[4]),bf(p[5])}, to{bf(p[6]),bf(p[7]),bf(p[8])};
            bool hasElement = std::stoi(p[9]) != 0;
            int axis = std::stoi(p[10]); float angle = bf(p[11]); bool rescale = std::stoi(p[12]) != 0;
            j::Vector3f origin{bf(p[13]),bf(p[14]),bf(p[15])};
            int mdl = std::stoi(p[16]);
            j::Matrix4f elemT = hasElement ? cr::computeTransform(cr::singleAxisTransformation(axis, angle), rescale) : j::Matrix4f{};
            j::Matrix4f modelM = mdl >= 0 ? rotM(mdl) : j::Matrix4f{};
            j::Vector3f got = fb::bakeVertexPosition(facing, index, from, to, hasElement, origin, elemT, mdl >= 0, modelM);
            v3(got, p, 17, line);
        }
        else if (t == "BVUV") {
            fb::UVs uvs{bf(p[1]), bf(p[2]), bf(p[3]), bf(p[4])};
            int q = std::stoi(p[5]), index = std::stoi(p[6]), mdl = std::stoi(p[7]);
            j::Matrix4f uvm = mdl >= 0 ? rotM(mdl) : j::Matrix4f{};
            float u, v;
            fb::bakeVertexUV(uvs, q, index, mdl >= 0, uvm, u, v);
            if (fbits(u) != static_cast<uint32_t>(std::stoul(p[8], nullptr, 16)) ||
                fbits(v) != static_cast<uint32_t>(std::stoul(p[9], nullptr, 16))) fail(line);
        }
        else fail("UNKNOWN_TAG " + t);
    }

    std::cout << "FaceBakery cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

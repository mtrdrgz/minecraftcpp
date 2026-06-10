// Parity gate for the two bounded helpers the element-level bake composes on the certified
// FaceBakery.bakeQuad: fb::defaultFaceUV vs FaceBakery.defaultFaceUV, and dir::rotate vs
// Direction.rotate(Matrix4fc, Direction). Bit-exact (UV floats as raw bits; ordinals as ints).
//
//   model_element_helpers_parity --cases mcpp/build/model_element_helpers.tsv

#include "CuboidRotation.h"
#include "DirectionRotate.h"
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
namespace cr = mc::render::model::cuboid;
namespace dr = mc::render::model::dir;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fbits(float v) { return std::bit_cast<uint32_t>(v); }

const float QS[5][4] = {
    {0, 0, 0, 1}, {0.5f, 0.5f, 0.5f, 0.5f}, {0.5f, 0, 0, 0.5f},
    {-0.5f, 0.5f, -0.5f, 0.5f}, {0.125f, 0.375f, -0.625f, 0.75f}};
j::Matrix4f rotM(int i) {
    j::Matrix4f m;
    m.rotation(j::Quaternionf{QS[i][0], QS[i][1], QS[i][2], QS[i][3]});
    return m;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: model_element_helpers_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l, const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH(" << why << ") " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& t = p[0];

        if (t == "DFUV") {
            ++total;
            int facing = std::stoi(p[1]);
            j::Vector3f from{bf(p[2]), bf(p[3]), bf(p[4])};
            j::Vector3f to{bf(p[5]), bf(p[6]), bf(p[7])};
            fb::UVs uv = fb::defaultFaceUV(from, to, facing);
            if (fbits(uv.minU) != static_cast<uint32_t>(std::stoul(p[8], nullptr, 16)) ||
                fbits(uv.minV) != static_cast<uint32_t>(std::stoul(p[9], nullptr, 16)) ||
                fbits(uv.maxU) != static_cast<uint32_t>(std::stoul(p[10], nullptr, 16)) ||
                fbits(uv.maxV) != static_cast<uint32_t>(std::stoul(p[11], nullptr, 16)))
                fail(line, "uv");
        } else if (t == "DROT") {
            ++total;
            int facing = std::stoi(p[1]);
            int matKind = std::stoi(p[2]);
            int qIdx = std::stoi(p[3]);
            int axis = std::stoi(p[4]);
            float angle = bf(p[5]);
            int expOrd = std::stoi(p[6]);
            j::Matrix4f m;
            if (matKind == 1) m = rotM(qIdx);
            else if (matKind == 2) m = cr::singleAxisTransformation(axis, angle);
            // matKind 0 -> default identity Matrix4f
            int got = dr::rotate(m, facing);
            if (got != expOrd) fail(line, "rot got=" + std::to_string(got));
        }
    }

    std::cout << "ModelElementHelpers cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

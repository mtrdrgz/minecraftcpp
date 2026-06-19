// Parity gate for fb::bakeQuad (the full FaceBakery.bakeQuad assembly) vs the real
// net.minecraft.client.resources.model.cuboid.FaceBakery.bakeQuad. Bit-exact: the 4
// baked positions (raw IEEE-754 bits), 4 packed UVs (u64), and the final Direction.
//
//   face_bakery_bakequad_parity --cases mcpp/build/face_bakery_bakequad.tsv
//
// Matrices are rebuilt from the certified generators (rotM(i) / SingleAxisRotation),
// matching tools/FaceBakeryBakeQuadParity.java — the gate carries the generator indices
// + the hasModel/hasUvTransform/hasElement booleans, never raw matrix floats.

#include "CuboidRotation.h"
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
uint64_t lb(const std::string& s) { return static_cast<uint64_t>(std::stoull(s, nullptr, 16)); }

// Must match FaceBakeryBakeQuadParity.java QS[] / rotM exactly.
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
    if (casesPath.empty()) { std::cerr << "usage: face_bakery_bakequad_parity --cases <tsv>\n"; return 2; }
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
        if (p.empty() || p[0] != "BQ") continue;
        ++total;

        int facing = std::stoi(p[1]);
        j::Vector3f from{bf(p[2]), bf(p[3]), bf(p[4])};
        j::Vector3f to{bf(p[5]), bf(p[6]), bf(p[7])};
        fb::UVs uvs{bf(p[8]), bf(p[9]), bf(p[10]), bf(p[11])};
        int uvRot = std::stoi(p[12]);
        fb::SpriteUV sprite{bf(p[13]), bf(p[14]), bf(p[15]), bf(p[16])};

        bool hasModel = std::stoi(p[17]) != 0;
        int modelIdx = std::stoi(p[18]);
        bool hasUvT = std::stoi(p[19]) != 0;
        int uvIdx = std::stoi(p[20]);
        bool hasElem = std::stoi(p[21]) != 0;
        int elemAxis = std::stoi(p[22]);
        float elemAngle = bf(p[23]);
        bool elemRescale = std::stoi(p[24]) != 0;
        j::Vector3f elemOrigin{bf(p[25]), bf(p[26]), bf(p[27])};

        j::Matrix4f modelM = hasModel ? rotM(modelIdx) : j::Matrix4f{};
        j::Matrix4f uvM = hasUvT ? rotM(uvIdx) : j::Matrix4f{};
        j::Matrix4f elemT = hasElem
            ? cr::computeTransform(cr::singleAxisTransformation(elemAxis, elemAngle), elemRescale)
            : j::Matrix4f{};

        fb::BakedQuadGeom q = fb::bakeQuad(from, to, uvs, uvRot, facing, sprite,
                                           hasModel, modelM, hasUvT, uvM,
                                           hasElem, elemOrigin, elemT);

        int expDir = std::stoi(p[28]);
        if (q.direction != expDir) { fail(line, "dir got=" + std::to_string(q.direction)); continue; }

        bool ok = true;
        for (int i = 0; i < 4 && ok; ++i) {
            int base = 29 + i * 3;
            if (fbits(q.pos[i].x) != static_cast<uint32_t>(std::stoul(p[base], nullptr, 16)) ||
                fbits(q.pos[i].y) != static_cast<uint32_t>(std::stoul(p[base + 1], nullptr, 16)) ||
                fbits(q.pos[i].z) != static_cast<uint32_t>(std::stoul(p[base + 2], nullptr, 16)))
                ok = false;
        }
        if (!ok) { fail(line, "pos"); continue; }
        for (int i = 0; i < 4 && ok; ++i)
            if (q.packedUV[i] != lb(p[41 + i])) ok = false;
        if (!ok) { fail(line, "uv"); continue; }
    }

    std::cout << "FaceBakeryBakeQuad cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

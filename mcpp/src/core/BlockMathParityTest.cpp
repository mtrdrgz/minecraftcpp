// Parity test for net.minecraft.core.BlockMath (UV-lock face transformation),
// mirrored by core/BlockMath.h on the certified render/model/Joml.h math subset.
// Ground truth: tools/BlockMathParity.java. Floats compared as raw IEEE-754 bits.
//
//   block_math_parity --cases mcpp/build/block_math.tsv
//
// TAGs:
//   L2G   <dir>                         16f   VANILLA_UV_TRANSFORM_LOCAL_TO_GLOBAL[dir].matrix
//   G2L   <dir>                         16f   VANILLA_UV_TRANSFORM_GLOBAL_TO_LOCAL[dir].matrix
//   FACE_ROT   <qi> <side>              16f   getFaceTransformation(rotation(QS[qi]), side)
//   FACE_TRANS <ti> <side>             16f    getFaceTransformation(translation(TS[ti]), side)
//   FACE_ID    <side>                  16f     getFaceTransformation(identity, side)  (== identity)
//   NEAREST    <dx> <dy> <dz>          <name>  Direction.getApproximateNearest

#include "BlockMath.h"

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace bm = mc::core::block_math;
namespace joml = mc::render::model::joml;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }

// Same exact-float quaternions / translations as the GT tool.
const float QS[12][4] = {
    {0,0,0,1}, {0.5f,0.5f,0.5f,0.5f}, {0.5f,0,0,0.5f}, {0,0.25f,0,0.75f},
    {-0.5f,0.5f,-0.5f,0.5f}, {0.25f,-0.25f,0.5f,1.f}, {1,0,0,0}, {0,1,0,0},
    {0.125f,0.375f,-0.625f,0.75f}, {-0.5f,-0.5f,0.5f,0.5f}, {0.7071068f,0,0,0.7071068f},
    {0,0,0.38268343f,0.9238795f}
};
const float TS[3][3] = { {1,0,0}, {0.5f,-2,3}, {-1.25f,0.75f,10} };

bm::Transformation rotIn(int i) {
    joml::Matrix4f m;
    m.rotation(joml::Quaternionf{QS[i][0], QS[i][1], QS[i][2], QS[i][3]});
    return bm::Transformation(m);
}
bm::Transformation transIn(int i) {
    joml::Matrix4f m;
    m.translation(TS[i][0], TS[i][1], TS[i][2]);
    return bm::Transformation(m);
}

// Direction name -> Dir enum (declaration order).
bm::Dir dirByName(const std::string& s) {
    if (s == "DOWN")  return bm::Dir::DOWN;
    if (s == "UP")    return bm::Dir::UP;
    if (s == "NORTH") return bm::Dir::NORTH;
    if (s == "SOUTH") return bm::Dir::SOUTH;
    if (s == "WEST")  return bm::Dir::WEST;
    if (s == "EAST")  return bm::Dir::EAST;
    return bm::Dir::NORTH; // unreachable for valid TSV
}
const char* dirName(bm::Dir d) {
    switch (d) {
        case bm::Dir::DOWN:  return "DOWN";
        case bm::Dir::UP:    return "UP";
        case bm::Dir::NORTH: return "NORTH";
        case bm::Dir::SOUTH: return "SOUTH";
        case bm::Dir::WEST:  return "WEST";
        case bm::Dir::EAST:  return "EAST";
    }
    return "?";
}
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: block_math_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n"; };

    // Compare 16 matrix floats starting at field index `o` in p.
    auto m4 = [&](const joml::Matrix4f& m, const std::vector<std::string>& p, int o, const std::string& l) {
        const float got[16] = { m.m00,m.m01,m.m02,m.m03, m.m10,m.m11,m.m12,m.m13,
                                m.m20,m.m21,m.m22,m.m23, m.m30,m.m31,m.m32,m.m33 };
        for (int k = 0; k < 16; ++k)
            if (fb(got[k]) != static_cast<uint32_t>(std::stoul(p[o + k], nullptr, 16))) {
                fail(l + " idx=" + std::to_string(k)); return;
            }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "L2G") {
            bm::Dir d = dirByName(p[1]);
            m4(bm::localToGlobal()[(int)d].matrix, p, 2, line);
        } else if (t == "G2L") {
            bm::Dir d = dirByName(p[1]);
            m4(bm::globalToLocal()[(int)d].matrix, p, 2, line);
        } else if (t == "FACE_ROT") {
            int qi = std::stoi(p[1]); bm::Dir side = dirByName(p[2]);
            bm::Transformation out = bm::getFaceTransformation(rotIn(qi), side);
            m4(out.matrix, p, 3, line);
        } else if (t == "FACE_TRANS") {
            int ti = std::stoi(p[1]); bm::Dir side = dirByName(p[2]);
            bm::Transformation out = bm::getFaceTransformation(transIn(ti), side);
            m4(out.matrix, p, 3, line);
        } else if (t == "FACE_ID") {
            bm::Dir side = dirByName(p[1]);
            joml::Matrix4f idm; // identity
            bm::Transformation out = bm::getFaceTransformation(bm::Transformation(idm), side);
            m4(out.matrix, p, 2, line);
        } else if (t == "NEAREST") {
            bm::Dir got = bm::getApproximateNearest(bf(p[1]), bf(p[2]), bf(p[3]));
            if (std::string(dirName(got)) != p[4]) fail(line + " got=" + dirName(got));
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "BlockMath cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

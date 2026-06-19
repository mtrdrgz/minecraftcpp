// Parity gate for elembake::bakeCuboidGeometry (the element-level block-model bake) vs the
// real net.minecraft.client.resources.model.cuboid.UnbakedCuboidGeometry.bake. Bit-exact:
// for every QuadCollection bucket (unculled + per-Direction culled), the ordered list of
// quads (4 positions raw bits, 4 packed UVs, direction).
//
//   model_element_bake_parity --cases mcpp/build/unbaked_cuboid.tsv
//
// Stream format: see tools/UnbakedCuboidGeometryParity.java header (CASE/ELEM/FACE/OUT/QUAD/END).

#include "CuboidRotation.h"
#include "ModelElementBake.h"

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
namespace eb = mc::render::model::elembake;

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

struct ExpQuad {
    int dir;
    uint32_t pos[12];
    uint64_t uv[4];
};
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: model_element_bake_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long cases = 0, quadsChecked = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << why << "\n";
    };

    std::vector<eb::CuboidElement> elements;
    eb::QuadBuckets computed;
    bool baked = false;
    // expected[bucketKey+1] : key -1 -> index 0, 0..5 -> 1..6
    std::vector<ExpQuad> expected[7];
    int curBucket = -2;  // currently-filling expected bucket index (-2 = none)

    auto doBake = [&]() {
        // identity model state: hasModel=false, hasUvTransform=false, identity matrices.
        j::Matrix4f ident;
        computed = eb::bakeCuboidGeometry(elements, false, ident, false, ident);
        baked = true;
    };
    auto bucketVec = [&](int key) -> const std::vector<fb::BakedQuadGeom>& {
        return key < 0 ? computed.unculled : computed.culled[key];
    };
    auto compareCase = [&]() {
        for (int slot = 0; slot < 7; ++slot) {
            int key = slot - 1;  // slot0->-1, slot1->0 ...
            const auto& got = bucketVec(key);
            const auto& exp = expected[slot];
            if (got.size() != exp.size()) {
                fail("bucket " + std::to_string(key) + " size got=" + std::to_string(got.size()) +
                     " exp=" + std::to_string(exp.size()));
                continue;
            }
            for (size_t i = 0; i < exp.size(); ++i) {
                ++quadsChecked;
                const auto& g = got[i];
                const auto& e = exp[i];
                bool ok = (g.direction == e.dir);
                for (int k = 0; k < 4 && ok; ++k) {
                    if (fbits(g.pos[k].x) != e.pos[k * 3] || fbits(g.pos[k].y) != e.pos[k * 3 + 1] ||
                        fbits(g.pos[k].z) != e.pos[k * 3 + 2]) ok = false;
                    if (g.packedUV[k] != e.uv[k]) ok = false;
                }
                if (!ok) fail("bucket " + std::to_string(key) + " quad " + std::to_string(i));
            }
        }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];

        if (t == "CASE") {
            elements.clear();
            baked = false;
            curBucket = -2;
            for (int s = 0; s < 7; ++s) expected[s].clear();
        } else if (t == "ELEM") {
            eb::CuboidElement e;
            e.from = j::Vector3f{bf(p[1]), bf(p[2]), bf(p[3])};
            e.to = j::Vector3f{bf(p[4]), bf(p[5]), bf(p[6])};
            e.hasElement = std::stoi(p[7]) != 0;
            int axis = std::stoi(p[8]);
            float angle = bf(p[9]);
            bool rescale = std::stoi(p[10]) != 0;
            e.elementOrigin = j::Vector3f{bf(p[11]), bf(p[12]), bf(p[13])};
            if (e.hasElement)
                e.elementTransform = cr::computeTransform(cr::singleAxisTransformation(axis, angle), rescale);
            elements.push_back(e);
        } else if (t == "FACE") {
            eb::ElementFace face;
            int facing = std::stoi(p[1]);
            face.present = true;
            face.sprite = fb::SpriteUV{bf(p[2]), bf(p[3]), bf(p[4]), bf(p[5])};
            face.hasUv = std::stoi(p[6]) != 0;
            face.uv = fb::UVs{bf(p[7]), bf(p[8]), bf(p[9]), bf(p[10])};
            face.uvRotation = std::stoi(p[11]);
            face.tintIndex = std::stoi(p[12]);
            face.cullForDirection = std::stoi(p[13]);
            elements.back().faces[facing] = face;
        } else if (t == "OUT") {
            if (!baked) doBake();
            curBucket = std::stoi(p[1]) + 1;  // -1->0, 0..5->1..6
        } else if (t == "QUAD") {
            ExpQuad q;
            q.dir = std::stoi(p[1]);
            for (int k = 0; k < 12; ++k) q.pos[k] = static_cast<uint32_t>(std::stoul(p[2 + k], nullptr, 16));
            for (int k = 0; k < 4; ++k) q.uv[k] = lb(p[14 + k]);
            expected[curBucket].push_back(q);
        } else if (t == "END") {
            ++cases;
            compareCase();
        }
    }

    std::cout << "ModelElementBake cases=" << cases << " quads=" << quadsChecked << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

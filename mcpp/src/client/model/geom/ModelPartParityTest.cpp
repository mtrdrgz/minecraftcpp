// Parity test for the pure transform math of net.minecraft.client.model.geom.ModelPart
// mirrored by client/model/geom/ModelPart.h. Ground truth: tools/ModelPartParity.java
// (the REAL ModelPart/PartPose + org.joml.Matrix3f/Vector3f from the shipped jars).
//
// rotateBy(Quaternionf) is the headline: it runs Matrix3f.rotationZYX -> rotate(quat)
// -> getEulerAnglesZYX, exercising the libm sin/cosFromSin path AND the atan2/sqrt euler
// extraction. The remaining ops (store/load/offset*) are exact float copies/adds.
//
//   model_part_parity --cases mcpp/build/model_part.tsv

#include "client/model/geom/ModelPart.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mp = mc::client::model::geom;
using mc::render::model::joml::Quaternionf;
using mc::render::model::joml::Vector3f;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
uint32_t hx(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }

// Same batteries as ModelPartParity.java (must stay in lock-step).
const float ROTS[12][3] = {
    {0.f, 0.f, 0.f},
    {0.5f, 0.5f, 0.5f},
    {1.5707964f, 0.f, 0.f},
    {0.f, 1.5707964f, 0.f},
    {0.f, 0.f, 1.5707964f},
    {3.1415927f, 0.f, 0.f},
    {0.7853982f, -0.7853982f, 0.3926991f},
    {-1.2f, 0.85f, 2.4f},
    {0.123f, -2.9f, 1.7f},
    {-0.5f, -0.5f, -0.5f},
    {2.5f, 1.0f, -3.0f},
    {0.05f, 0.05f, 0.05f},
};
const float QS[10][4] = {
    {0.f, 0.f, 0.f, 1.f},
    {0.5f, 0.5f, 0.5f, 0.5f},
    {-0.5f, 0.5f, -0.5f, 0.5f},
    {1.f, 0.f, 0.f, 0.f},
    {0.f, 1.f, 0.f, 0.f},
    {0.f, 0.f, 1.f, 0.f},
    {0.125f, 0.375f, -0.625f, 0.75f},
    {0.5f, 0.f, 0.f, 0.5f},
    {-1.5f, 0.5f, 2.25f, -0.75f},
    {0.0625f, 0.0625f, 0.0625f, 0.0625f},
};
const float OFFS[4][3] = {
    {0.f, 0.f, 0.f},
    {1.5f, -2.25f, 0.75f},
    {-0.5f, 0.25f, -3.0f},
    {16.0f, 8.0f, -4.0f},
};
const float SCALES[4] = {1.0f, 0.5f, 2.0f, -1.25f};
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: model_part_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n";
    };

    // Compare 9 pose fields starting at TSV column o.
    auto cmpPose = [&](const mp::ModelPart& p, const std::vector<std::string>& cols, int o,
                       const std::string& l) {
        const float got[9] = {p.x,    p.y,    p.z,      p.xRot,  p.yRot,
                              p.zRot, p.xScale, p.yScale, p.zScale};
        for (int k = 0; k < 9; ++k)
            if (fb(got[k]) != hx(cols[o + k])) {
                fail(l + " field=" + std::to_string(k));
                return;
            }
    };
    auto cmpPartPose = [&](const mp::PartPose& p, const std::vector<std::string>& cols, int o,
                           const std::string& l) {
        const float got[9] = {p.x,    p.y,    p.z,      p.xRot,  p.yRot,
                              p.zRot, p.xScale, p.yScale, p.zScale};
        for (int k = 0; k < 9; ++k)
            if (fb(got[k]) != hx(cols[o + k])) {
                fail(l + " field=" + std::to_string(k));
                return;
            }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto c = split(line);
        const std::string& t = c[0];
        ++total;

        if (t == "ROTBY") {
            int r = std::stoi(c[1]), q = std::stoi(c[2]);
            mp::ModelPart p;
            p.setRotation(ROTS[r][0], ROTS[r][1], ROTS[r][2]);
            p.rotateBy(Quaternionf{QS[q][0], QS[q][1], QS[q][2], QS[q][3]});
            cmpPose(p, c, 3, line);
        } else if (t == "STORE") {
            int r = std::stoi(c[1]);
            mp::ModelPart p;
            p.setPos(ROTS[r][2] * 3.0f, ROTS[r][0] - 1.0f, ROTS[r][1]);
            p.setRotation(ROTS[r][0], ROTS[r][1], ROTS[r][2]);
            mp::PartPose sp = p.storePose();
            cmpPartPose(sp, c, 2, line);
        } else if (t == "LOAD") {
            int r = std::stoi(c[1]);
            float s = SCALES[r % 4];
            mp::PartPose lp = mp::PartPose::offsetAndRotation(
                                  ROTS[r][0], ROTS[r][1] + 4.0f, ROTS[r][2] - 2.0f, ROTS[r][0],
                                  ROTS[r][1], ROTS[r][2])
                                  .scaled(s);
            mp::ModelPart p;
            p.loadPose(lp);
            cmpPose(p, c, 2, line);
        } else if (t == "OFFPOS") {
            int r = std::stoi(c[1]), o = std::stoi(c[2]);
            mp::ModelPart p;
            p.setPos(ROTS[r][0], ROTS[r][1], ROTS[r][2]);
            p.offsetPos(Vector3f{OFFS[o][0], OFFS[o][1], OFFS[o][2]});
            cmpPose(p, c, 3, line);
        } else if (t == "OFFROT") {
            int r = std::stoi(c[1]), o = std::stoi(c[2]);
            mp::ModelPart p;
            p.setRotation(ROTS[r][0], ROTS[r][1], ROTS[r][2]);
            p.offsetRotation(Vector3f{OFFS[o][0], OFFS[o][1], OFFS[o][2]});
            cmpPose(p, c, 3, line);
        } else if (t == "OFFSCL") {
            int o = std::stoi(c[2]);
            mp::ModelPart p;
            p.loadPose(mp::PartPose::offsetAndRotation(0, 0, 0, 0, 0, 0));
            p.offsetScale(Vector3f{OFFS[o][0], OFFS[o][1], OFFS[o][2]});
            cmpPose(p, c, 3, line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "ModelPart checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

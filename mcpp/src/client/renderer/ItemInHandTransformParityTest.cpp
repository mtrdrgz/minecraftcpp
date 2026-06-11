// Bit-exact parity gate for the PURE first-person hand/item PoseStack transforms in
// net.minecraft.client.renderer.ItemInHandRenderer (Minecraft 26.1.2). Replays the
// inputs emitted by tools/ItemInHandTransformParity.java through client/renderer/
// ItemInHandTransform.h and compares the float result / the top Pose's 16 pose + 9
// normal floats + trustedNormals bit-for-bit against the REAL class.
//
//   item_in_hand_transform_parity --cases mcpp/build/item_in_hand_transform.tsv
//
// Row formats (leading TAG):
//   TILT \t i \t <xRot bits> \t <result bits>
//   ARMT \t i \t <arm 0/1> \t <inverseArmHeight bits> \t <16 pose> \t <9 normal> \t <trusted 0/1>
//   ARMA \t i \t <arm 0/1> \t <attackValue bits>      \t <16 pose> \t <9 normal> \t <trusted 0/1>

#include "ItemInHandTransform.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace iih = mc::client::renderer;
namespace r = mc::render;
namespace j = mc::render::model::joml;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
float fromBits(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16)));
}
uint32_t hx(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }

long long g_total = 0, g_mism = 0;
int g_shown = 0;

void fail(const std::string& line) {
    ++g_mism;
    if (g_shown++ < 40) std::cerr << "MISMATCH " << line << "\n";
}

// Compare the live PoseStack top against the 26 trailing columns starting at offset o.
void checkPose(r::PoseStack& ps, const std::vector<std::string>& p, int o,
               const std::string& line) {
    r::Pose& pose = ps.last();
    const j::Matrix4f& m = pose.pose;
    const j::Matrix3f& n = pose.normal;
    const float gotP[16] = { m.m00, m.m01, m.m02, m.m03, m.m10, m.m11, m.m12, m.m13,
                             m.m20, m.m21, m.m22, m.m23, m.m30, m.m31, m.m32, m.m33 };
    const float gotN[9]  = { n.m00, n.m01, n.m02, n.m10, n.m11, n.m12, n.m20, n.m21, n.m22 };
    bool ok = true;
    for (int k = 0; k < 16 && ok; ++k) if (fb(gotP[k]) != hx(p[o + k])) ok = false;
    for (int k = 0; k < 9 && ok; ++k)  if (fb(gotN[k]) != hx(p[o + 16 + k])) ok = false;
    int gotTrusted = pose.trustedNormals ? 1 : 0;
    if (ok && gotTrusted != std::stoi(p[o + 25])) ok = false;
    if (!ok) fail(line);
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: item_in_hand_transform_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.size() < 3) continue;
        const std::string& tag = p[0];

        if (tag == "TILT") {
            // TILT \t i \t <xRot> \t <result>
            ++g_total;
            float xRot = fromBits(p[2]);
            float got = iih::calculateMapTilt(xRot);
            if (fb(got) != hx(p[3])) fail(line);
        } else if (tag == "ARMT") {
            // ARMT \t i \t <arm> \t <inverseArmHeight> \t <16 pose> \t <9 normal> \t <trusted>
            ++g_total;
            iih::HumanoidArm arm = std::stoi(p[2]) == 1 ? iih::HumanoidArm::RIGHT
                                                        : iih::HumanoidArm::LEFT;
            float h = fromBits(p[3]);
            r::PoseStack ps;
            iih::applyItemArmTransform(ps, arm, h);
            checkPose(ps, p, 4, line);
        } else if (tag == "ARMA") {
            // ARMA \t i \t <arm> \t <attackValue> \t <16 pose> \t <9 normal> \t <trusted>
            ++g_total;
            iih::HumanoidArm arm = std::stoi(p[2]) == 1 ? iih::HumanoidArm::RIGHT
                                                        : iih::HumanoidArm::LEFT;
            float a = fromBits(p[3]);
            r::PoseStack ps;
            iih::applyItemArmAttackTransform(ps, arm, a);
            checkPose(ps, p, 4, line);
        } else {
            ++g_total;
            fail(line);
        }
    }

    std::cout << "ItemInHandTransform checks=" << g_total << " mismatches=" << g_mism << "\n";
    return g_mism == 0 ? 0 : 1;
}

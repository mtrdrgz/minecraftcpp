// Parity test for the PURE, GL-free keyframe interpolators of
// net.minecraft.client.animation.AnimationChannel.Interpolations (26.1.2),
// mirrored in client/animation/AnimationChannelInterpolations.h. Floats are
// compared as raw IEEE-754 bits. Ground truth: tools/AnimationChannelInterpolationsParity.java.
//
//   anim_channel_interp_parity --cases mcpp/build/anim_channel_interp.tsv
//
// The Java GT drives the REAL Interpolation lambdas through real org.joml.Vector3f
// (lerp uses the TRUE java.lang.Math.fma) + real Mth.catmullrom; here we recompute
// with the header and require a bit-for-bit match on every output component.

#include "client/animation/AnimationChannelInterpolations.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace anim = mc::client::animation;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16)));
}
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
uint32_t ux(const std::string& s) {
    return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
}
anim::Vec3f loadVec(const std::vector<std::string>& p, int o) {
    return anim::Vec3f{bf(p[o]), bf(p[o + 1]), bf(p[o + 2])};
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: anim_channel_interp_parity --cases <tsv>\n";
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
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };
    auto hx = [](uint32_t u) {
        char b[16];
        std::snprintf(b, sizeof(b), "%08x", u);
        return std::string(b);
    };
    auto eqVec = [&](const anim::Vec3f& got, const std::vector<std::string>& p, int o,
                     const std::string& l) {
        if (fb(got.x) != ux(p[o]) || fb(got.y) != ux(p[o + 1]) || fb(got.z) != ux(p[o + 2]))
            fail(l + "  GOT=" + hx(fb(got.x)) + " " + hx(fb(got.y)) + " " + hx(fb(got.z)) +
                 " EXP=" + p[o] + " " + p[o + 1] + " " + p[o + 2]);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];

        if (tag == "LINEAR") {
            // LINEAR post0(3) pre1(3) alpha scale out(3)
            anim::Vec3f post0 = loadVec(p, 1);
            anim::Vec3f pre1 = loadVec(p, 4);
            float alpha = bf(p[7]);
            float scale = bf(p[8]);
            // Build the 2-keyframe array exactly as the GT did: prev=0,next=1.
            // LINEAR reads keyframes[0].postTarget and keyframes[1].preTarget.
            std::vector<anim::Keyframe> kfs(2);
            kfs[0].postTarget = post0;  // preTarget of kf0 is unused by LINEAR
            kfs[1].preTarget = pre1;    // postTarget of kf1 is unused by LINEAR
            anim::Vec3f got = anim::linear(kfs, alpha, 0, 1, scale);
            ++total;
            eqVec(got, p, 9, line);
        } else if (tag == "CATMULLROM") {
            // CATMULLROM p0(3) p1(3) p2(3) p3(3) alpha scale out(3)
            anim::Vec3f p0 = loadVec(p, 1);
            anim::Vec3f p1 = loadVec(p, 4);
            anim::Vec3f p2 = loadVec(p, 7);
            anim::Vec3f p3 = loadVec(p, 10);
            float alpha = bf(p[13]);
            float scale = bf(p[14]);
            // Rebuild a 4-keyframe array whose postTargets ARE the resolved p0..p3, then
            // call with prev=1,next=2 so the header resolves:
            //   max(0,prev-1)=0 -> p0 ; prev=1 -> p1 ; next=2 -> p2 ; min(3,next+1)=3 -> p3.
            std::vector<anim::Keyframe> kfs(4);
            kfs[0].postTarget = p0;
            kfs[1].postTarget = p1;
            kfs[2].postTarget = p2;
            kfs[3].postTarget = p3;
            anim::Vec3f got = anim::catmullrom(kfs, alpha, 1, 2, scale);
            ++total;
            eqVec(got, p, 15, line);
        } else {
            fail(line + " (unknown tag)");
        }
    }

    std::cout << "AnimationChannelInterpolations checks=" << total << " mismatches=" << mism
              << "\n";
    return mism == 0 ? 0 : 1;
}

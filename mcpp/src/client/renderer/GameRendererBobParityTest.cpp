// Bit-exact parity test for the C++ 1:1 port of the GameRenderer view-bob
// PoseStack transforms (client/renderer/GameRendererBob.h) against the REAL
// net.minecraft.client.renderer.GameRenderer.{bobHurt,bobView}, driven by
// tools/GameRendererBobParity.java.
//
//   GameRendererBobParityTest --cases <tsv>
//
// Each TSV row carries the raw IEEE-754 bits of the inputs plus the 16 pose +
// 9 normal floats and the trustedNormals flag produced by the real method on a
// fresh PoseStack. We rebuild the inputs, run the C++ helper on a fresh
// mc::render::PoseStack, and assert every emitted bit matches. Prints
//   "GameRendererBob checks=N mismatches=M"
// and exits nonzero iff M>0.

#include "client/renderer/GameRendererBob.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

float fbits(std::uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
double dbits(std::uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
std::uint32_t bitsf(float f) { std::uint32_t b; std::memcpy(&b, &f, 4); return b; }

std::uint32_t hx32(const std::string& s) { return (std::uint32_t)std::stoul(s, nullptr, 16); }
std::uint64_t hx64(const std::string& s) { return (std::uint64_t)std::stoull(s, nullptr, 16); }

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

// Read the top Pose of a PoseStack into a flat {16 pose, 9 normal} + trusted.
struct Snapshot {
    std::uint32_t pose[16];
    std::uint32_t normal[9];
    int trusted;
};

Snapshot snap(mc::render::PoseStack& ps) {
    auto& p = ps.last().pose;
    auto& n = ps.last().normal;
    Snapshot s{};
    const float pv[16] = {
        p.m00, p.m01, p.m02, p.m03, p.m10, p.m11, p.m12, p.m13,
        p.m20, p.m21, p.m22, p.m23, p.m30, p.m31, p.m32, p.m33};
    for (int i = 0; i < 16; ++i) s.pose[i] = bitsf(pv[i]);
    const float nv[9] = { n.m00, n.m01, n.m02, n.m10, n.m11, n.m12, n.m20, n.m21, n.m22 };
    for (int i = 0; i < 9; ++i) s.normal[i] = bitsf(nv[i]);
    s.trusted = ps.last().trustedNormals ? 1 : 0;
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    const char* casesPath = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--cases") == 0 && i + 1 < argc) casesPath = argv[++i];
    }
    if (!casesPath) { std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]); return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", casesPath); return 2; }

    long long checks = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto f = split(line);
        if (f.empty()) continue;

        Snapshot exp{};
        Snapshot got{};

        if (f[0] == "HURT") {
            // HURT i isLiving isDead deathTime hurtTime hurtDuration hurtDir tiltDouble <16> <9> trusted
            if (f.size() != 9 + 16 + 9 + 1) { std::fprintf(stderr, "bad HURT row arity %zu\n", f.size()); return 2; }
            mc::client::renderer::CameraEntityState e;
            e.isLiving = f[2] == "1";
            e.isDeadOrDying = f[3] == "1";
            e.deathTime = fbits(hx32(f[4]));
            e.hurtTime = fbits(hx32(f[5]));
            e.hurtDuration = std::stoi(f[6]);
            e.hurtDir = fbits(hx32(f[7]));
            double tilt = dbits(hx64(f[8]));

            mc::render::PoseStack ps;
            mc::client::renderer::bobHurt(e, tilt, ps);
            got = snap(ps);

            int idx = 9;
            for (int k = 0; k < 16; ++k) exp.pose[k] = hx32(f[idx++]);
            for (int k = 0; k < 9; ++k) exp.normal[k] = hx32(f[idx++]);
            exp.trusted = std::stoi(f[idx++]);
        } else if (f[0] == "VIEW") {
            // VIEW i isPlayer walk bob <16> <9> trusted
            if (f.size() != 5 + 16 + 9 + 1) { std::fprintf(stderr, "bad VIEW row arity %zu\n", f.size()); return 2; }
            mc::client::renderer::CameraEntityState e;
            e.isPlayer = f[2] == "1";
            e.backwardsInterpolatedWalkDistance = fbits(hx32(f[3]));
            e.bob = fbits(hx32(f[4]));

            mc::render::PoseStack ps;
            mc::client::renderer::bobView(e, ps);
            got = snap(ps);

            int idx = 5;
            for (int k = 0; k < 16; ++k) exp.pose[k] = hx32(f[idx++]);
            for (int k = 0; k < 9; ++k) exp.normal[k] = hx32(f[idx++]);
            exp.trusted = std::stoi(f[idx++]);
        } else {
            continue;
        }

        bool ok = (exp.trusted == got.trusted);
        for (int k = 0; k < 16 && ok; ++k) ok = ok && (exp.pose[k] == got.pose[k]);
        for (int k = 0; k < 9 && ok; ++k) ok = ok && (exp.normal[k] == got.normal[k]);
        ++checks;
        if (!ok) {
            ++mismatches;
            if (mismatches <= 12) {
                std::fprintf(stderr, "MISMATCH %s row tag=%s\n", f[1].c_str(), f[0].c_str());
                for (int k = 0; k < 16; ++k)
                    if (exp.pose[k] != got.pose[k])
                        std::fprintf(stderr, "  pose[%d] exp=%08x got=%08x\n", k, exp.pose[k], got.pose[k]);
                for (int k = 0; k < 9; ++k)
                    if (exp.normal[k] != got.normal[k])
                        std::fprintf(stderr, "  normal[%d] exp=%08x got=%08x\n", k, exp.normal[k], got.normal[k]);
                if (exp.trusted != got.trusted)
                    std::fprintf(stderr, "  trusted exp=%d got=%d\n", exp.trusted, got.trusted);
            }
        }
    }

    std::printf("GameRendererBob checks=%lld mismatches=%lld\n", checks, mismatches);
    return mismatches == 0 ? 0 : 1;
}

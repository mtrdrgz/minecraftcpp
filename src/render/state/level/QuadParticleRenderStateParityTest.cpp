// Bit-exact parity gate for the PURE billboard-quad geometry produced by
// net.minecraft.client.renderer.state.level.QuadParticleRenderState
// (renderRotatedQuad / renderVertex) — the math that turns every extracted
// particle into its four QUADS vertices.
//
// Ground truth: tools/QuadParticleRenderStateParity.java drives the REAL class
// via reflection, capturing the four emitted vertices through a no-op
// VertexConsumer. This test re-drives the C++ port in
// render/state/level/QuadParticleRenderState.h and compares every position and
// UV coordinate bit-for-bit (Float.floatToRawIntBits on the Java side vs
// std::bit_cast<int32_t> here) and color/light as decimal ints.
//
//   mcpp/build/quad_particle_parity.exe --cases mcpp/build/quad_particle.tsv
//
// TSV rows:
//   CASE <id>
//   VTX  <id> <corner> <xBits> <yBits> <zBits> <uBits> <vBits> <color> <light>
//
// The case inputs are reconstructed here from the SAME battery the Java tool
// uses (kept in lock-step), so the C++ recomputes from scratch — it does not
// read the Java outputs as inputs.

#include "render/state/level/QuadParticleRenderState.h"

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace lvl = mc::render::state::level;

namespace {

int32_t f2i(float f) { return std::bit_cast<int32_t>(f); }

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

long long L(const std::string& s) { return std::stoll(s); }
int I(const std::string& s) { return std::stoi(s); }

// ── The SAME battery as QuadParticleRenderStateParity.java (lock-step). ───────
const float QUATS[][4] = {
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f, -1.0f },
    { 0.70710677f, 0.0f, 0.0f, 0.70710677f },
    { 0.0f, 0.70710677f, 0.0f, 0.70710677f },
    { 0.0f, 0.0f, 0.70710677f, 0.70710677f },
    { 0.5f, 0.5f, 0.5f, 0.5f },
    { 0.27059805f, 0.27059805f, 0.65328145f, 0.65328148f },
    { 0.18301269f, -0.18301269f, 0.6830127f, 0.6830127f },
    { -0.35355338f, 0.14644662f, 0.35355338f, 0.8535534f },
    { 0.1f, 0.2f, 0.3f, 0.9f },
    { 2.0f, -1.0f, 0.5f, 1.0f },
    { 1e-4f, 1e-4f, 1e-4f, 1.0f },
};
const float POSS[][3] = {
    { 0.0f, 0.0f, 0.0f },
    { 12.5f, -3.25f, 7.0f },
    { -128.0f, 64.0f, -255.5f },
    { 0.015625f, -0.5f, 100.0f },
    { 1234.5f, 5678.25f, -9012.75f },
};
const float SCALES[] = { 0.1f, 0.2f, 1.0f, 2.5f, 0.017320509f };
const float UVS[][4] = {
    { 0.0f, 1.0f, 0.0f, 1.0f },
    { 0.125f, 0.1875f, 0.5f, 0.5625f },
    { 0.33333334f, 0.6666667f, 0.1f, 0.9f },
};
const int COLORS[] = { (int)0xFFFFFFFF, (int)0x80FF8040, 0x00000000, 0x12345678 };
const int LIGHTS[] = { 0x00F000F0, 0x000F0000, 0x00FFFFFF, 0 };

constexpr int N_UV = 3, N_COL = 4, N_LIT = 4;

// Reconstruct the quad for case id, matching the Java loop ordering exactly.
std::array<lvl::QuadVertex, 4> quadForId(int id) {
    const int nScales = (int)(sizeof(SCALES) / sizeof(SCALES[0]));
    const int nPoss = (int)(sizeof(POSS) / sizeof(POSS[0]));
    const int perPos = nScales;
    const int perQuat = nPoss * nScales;
    const int qi = id / perQuat;
    const int rem = id % perQuat;
    const int pi = rem / perPos;
    const int si = rem % perPos;
    const float* q = QUATS[qi];
    const float* p = POSS[pi];
    const float s = SCALES[si];
    const float* uv = UVS[id % N_UV];
    const int color = COLORS[id % N_COL];
    const int light = LIGHTS[id % N_LIT];
    return lvl::renderRotatedQuad(p[0], p[1], p[2], q[0], q[1], q[2], q[3], s,
                                  uv[0], uv[1], uv[2], uv[3], color, light);
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: quad_particle_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long checks = 0, mismatches = 0;
    int shown = 0;
    auto fail = [&](const std::string& what) {
        ++mismatches;
        if (shown++ < 40) std::cerr << "MISMATCH " << what << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p[0] != "VTX") continue;  // CASE rows are informational
        int id = I(p[1]);
        int corner = I(p[2]);
        auto quad = quadForId(id);
        const lvl::QuadVertex& vtx = quad[corner];

        // position bits
        ++checks;
        if (f2i(vtx.x) != (int32_t)L(p[3]))
            fail("id=" + std::to_string(id) + " c=" + std::to_string(corner) + " x got=" +
                 std::to_string(f2i(vtx.x)) + " want=" + p[3]);
        ++checks;
        if (f2i(vtx.y) != (int32_t)L(p[4]))
            fail("id=" + std::to_string(id) + " c=" + std::to_string(corner) + " y got=" +
                 std::to_string(f2i(vtx.y)) + " want=" + p[4]);
        ++checks;
        if (f2i(vtx.z) != (int32_t)L(p[5]))
            fail("id=" + std::to_string(id) + " c=" + std::to_string(corner) + " z got=" +
                 std::to_string(f2i(vtx.z)) + " want=" + p[5]);
        // uv bits
        ++checks;
        if (f2i(vtx.u) != (int32_t)L(p[6]))
            fail("id=" + std::to_string(id) + " c=" + std::to_string(corner) + " u");
        ++checks;
        if (f2i(vtx.v) != (int32_t)L(p[7]))
            fail("id=" + std::to_string(id) + " c=" + std::to_string(corner) + " v");
        // color / light ints
        ++checks;
        if (vtx.color != I(p[8]))
            fail("id=" + std::to_string(id) + " c=" + std::to_string(corner) + " color");
        ++checks;
        if (vtx.light != I(p[9]))
            fail("id=" + std::to_string(id) + " c=" + std::to_string(corner) + " light");
    }

    std::cout << "QuadParticleRenderState checks=" << checks
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

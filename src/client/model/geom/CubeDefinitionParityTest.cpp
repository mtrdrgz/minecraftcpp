// Parity test for the PURE, GL-free cube vertex-builder math of
// net.minecraft.client.model.geom.ModelPart.Cube / .Polygon / .Vertex
// (Minecraft 26.1.2), mirrored by client/model/geom/CubeDefinition.h.
// Ground truth: tools/CubeDefinitionParity.java (the REAL ModelPart.Cube /
// Polygon / Vertex + net.minecraft.core.Direction.getUnitVec3f from client.jar).
//
// The Cube ctor builds 8 grown/mirrored box corners, cuts per-face texture
// rects into UVs, and tags each face with its (optionally X-mirrored) Direction
// normal — all deterministic float arithmetic, no VertexConsumer/GL. We rebuild
// each cube + each CubeDeformation.extend result and compare every float bit-for-
// bit (Float.floatToRawIntBits) against the jar's output.
//
//   cube_definition_parity --cases mcpp/build/cube_definition.tsv

#include "client/model/geom/CubeDefinition.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace geom = mc::client::model::geom;

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

// MUST stay in lock-step with CubeDefinitionParity.java CUBES[].
// {minX,minY,minZ, w,h,d, gx,gy,gz, xTexOffs,yTexOffs, xTexSize,yTexSize, mirror, faceMask}
const float CUBES[][15] = {
    {-4.f, -4.f, -4.f, 8.f, 8.f, 8.f, 0.f, 0.f, 0.f, 0.f, 0.f, 64.f, 64.f, 0.f, 63.f},
    {-4.f, -4.f, -4.f, 8.f, 8.f, 8.f, 0.f, 0.f, 0.f, 0.f, 0.f, 64.f, 64.f, 1.f, 63.f},
    {-2.f, 0.f, -2.f, 4.f, 12.f, 4.f, 0.25f, 0.25f, 0.25f, 16.f, 16.f, 64.f, 64.f, 0.f, 63.f},
    {-2.f, 0.f, -2.f, 4.f, 12.f, 4.f, 0.25f, 0.5f, 1.0f, 16.f, 16.f, 64.f, 64.f, 0.f, 63.f},
    {-2.f, 0.f, -2.f, 4.f, 12.f, 4.f, 0.25f, 0.5f, 1.0f, 16.f, 16.f, 64.f, 64.f, 1.f, 63.f},
    {-3.5f, 1.5f, -0.5f, 7.f, 3.f, 5.f, 0.f, 0.f, 0.f, 40.f, 16.f, 64.f, 32.f, 0.f, 63.f},
    {-3.5f, 1.5f, -0.5f, 7.f, 3.f, 5.f, 0.1f, 0.2f, 0.3f, 40.f, 16.f, 128.f, 128.f, 1.f, 63.f},
    {0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 16.f, 16.f, 0.f, 63.f},
    {0.f, 0.f, 0.f, 2.f, 2.f, 2.f, -0.5f, -0.5f, -0.5f, 0.f, 0.f, 64.f, 64.f, 0.f, 63.f},
    {-4.f, -4.f, -4.f, 8.f, 8.f, 8.f, 0.f, 0.f, 0.f, 0.f, 0.f, 64.f, 64.f, 0.f, 3.f},
    {-4.f, -4.f, -4.f, 8.f, 8.f, 8.f, 0.f, 0.f, 0.f, 0.f, 0.f, 64.f, 64.f, 0.f, 60.f},
    {-4.f, -4.f, -4.f, 8.f, 8.f, 8.f, 0.f, 0.f, 0.f, 0.f, 0.f, 64.f, 64.f, 1.f, 21.f},
    {-1.f, -2.f, -3.f, 2.f, 4.f, 6.f, 0.05f, 0.f, 0.15f, 8.f, 8.f, 64.f, 64.f, 0.f, 63.f},
    {-1.f, -2.f, -3.f, 2.f, 4.f, 6.f, 0.05f, 0.f, 0.15f, 8.f, 8.f, 64.f, 64.f, 1.f, 63.f},
};

geom::Cube buildCube(const float* c) {
    int xTexOffs = static_cast<int>(c[9]), yTexOffs = static_cast<int>(c[10]);
    bool mirror = c[13] != 0.f;
    int mask = static_cast<int>(c[14]);
    return geom::Cube::make(xTexOffs, yTexOffs, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8],
                            mirror, c[11], c[12], (mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0,
                            (mask & 8) != 0, (mask & 16) != 0, (mask & 32) != 0);
}

// MUST stay in lock-step with CubeDefinitionParity.java defs/scalarFactors/vecFactors.
const float DEFS[][3] = {
    {0.f, 0.f, 0.f}, {0.25f, 0.25f, 0.25f}, {-1.f, 0.5f, 2.25f}, {0.1f, 0.2f, 0.3f}};
const float SCALAR_FACTORS[] = {0.f, 0.5f, -0.75f, 1.5f};
const float VEC_FACTORS[][3] = {{0.f, 0.f, 0.f}, {0.1f, -0.2f, 0.3f}, {1.5f, 2.5f, -3.5f}};
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: cube_definition_parity --cases <tsv>\n";
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
    auto chk = [&](float got, const std::string& exp, const std::string& l) {
        ++total;
        if (fb(got) != hx(exp)) fail(l);
    };

    // Pre-build all cubes once so VTX/POLY/BOX rows resolve cheaply.
    std::vector<geom::Cube> cubes;
    cubes.reserve(sizeof(CUBES) / sizeof(CUBES[0]));
    for (const auto& c : CUBES) cubes.push_back(buildCube(c));

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto col = split(line);
        const std::string& t = col[0];

        if (t == "BOX") {
            int idx = std::stoi(col[1]);
            const geom::Cube& cube = cubes.at(idx);
            chk(cube.minX, col[2], line + " minX");
            chk(cube.minY, col[3], line + " minY");
            chk(cube.minZ, col[4], line + " minZ");
            chk(cube.maxX, col[5], line + " maxX");
            chk(cube.maxY, col[6], line + " maxY");
            chk(cube.maxZ, col[7], line + " maxZ");
            // polygon count is an int (decimal).
            ++total;
            if (static_cast<int>(cube.polygons.size()) != std::stoi(col[8]))
                fail(line + " polyCount");
        } else if (t == "POLY") {
            int idx = std::stoi(col[1]), pi = std::stoi(col[2]);
            const geom::Polygon& p = cubes.at(idx).polygons.at(pi);
            chk(p.normal.x, col[3], line + " nx");
            chk(p.normal.y, col[4], line + " ny");
            chk(p.normal.z, col[5], line + " nz");
        } else if (t == "VTX") {
            int idx = std::stoi(col[1]), pi = std::stoi(col[2]), vi = std::stoi(col[3]);
            const geom::Vertex& v = cubes.at(idx).polygons.at(pi).vertices.at(vi);
            chk(v.x, col[4], line + " x");
            chk(v.y, col[5], line + " y");
            chk(v.z, col[6], line + " z");
            chk(v.u, col[7], line + " u");
            chk(v.v, col[8], line + " v");
            chk(v.worldX(), col[9], line + " worldX");
            chk(v.worldY(), col[10], line + " worldY");
            chk(v.worldZ(), col[11], line + " worldZ");
        } else if (t == "GROW1") {
            int di = std::stoi(col[1]), fi = std::stoi(col[2]);
            geom::CubeDeformation base{DEFS[di][0], DEFS[di][1], DEFS[di][2]};
            geom::CubeDeformation e = base.extend(SCALAR_FACTORS[fi]);
            chk(e.growX, col[3], line + " gx");
            chk(e.growY, col[4], line + " gy");
            chk(e.growZ, col[5], line + " gz");
        } else if (t == "GROW3") {
            int di = std::stoi(col[1]), fi = std::stoi(col[2]);
            geom::CubeDeformation base{DEFS[di][0], DEFS[di][1], DEFS[di][2]};
            geom::CubeDeformation e =
                base.extend(VEC_FACTORS[fi][0], VEC_FACTORS[fi][1], VEC_FACTORS[fi][2]);
            chk(e.growX, col[3], line + " gx");
            chk(e.growY, col[4], line + " gy");
            chk(e.growZ, col[5], line + " gz");
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "CubeDefinition checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

// Parity gate for aolight::prepareQuadShape (BlockModelLighter.prepareQuadShape) — per-quad
// geometry classification (faceShape[12] + facePartial + faceCubic). Bit-exact faceShape (raw
// float bits) + bool flags vs the reflection-driven real method.
//
//   block_model_lighter_shape_parity --cases mcpp/build/bml_shape.tsv

#include "BlockModelLighterShape.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ao = mc::render::block::aolight;
namespace j = mc::render::model::joml;

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
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: block_model_lighter_shape_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, bad = 0;
    int shown = 0;
    auto fail = [&](const std::string& w) { ++bad; if (shown++ < 30) std::cerr << "MISMATCH " << w << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p[0] != "SHP") continue;
        ++n;
        int dir = std::stoi(p[1]);
        bool collisionFull = std::stoi(p[2]) != 0;
        j::Vector3f vp[4];
        for (int i = 0; i < 4; ++i) vp[i] = j::Vector3f{bf(p[3 + i * 3]), bf(p[4 + i * 3]), bf(p[5 + i * 3])};
        // columns: 3..14 positions, 15..26 faceShape, 27 facePartial, 28 faceCubic
        ao::QuadShape r = ao::prepareQuadShape(vp, dir, collisionFull, true);
        bool ok = true;
        for (int i = 0; i < 12 && ok; ++i)
            if (fbits(r.faceShape[i]) != static_cast<uint32_t>(std::stoul(p[15 + i], nullptr, 16))) ok = false;
        if (!ok) { fail("faceShape dir" + std::to_string(dir)); continue; }
        if ((r.facePartial ? 1 : 0) != std::stoi(p[27])) { fail("facePartial dir" + std::to_string(dir)); continue; }
        if ((r.faceCubic ? 1 : 0) != std::stoi(p[28])) { fail("faceCubic dir" + std::to_string(dir) + " cf=" + p[2]); continue; }
    }
    std::cout << "BlockModelLighterShape cases=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}

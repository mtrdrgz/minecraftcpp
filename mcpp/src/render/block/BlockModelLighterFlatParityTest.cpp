// Parity gate for aolight::prepareQuadFlat (BlockModelLighter.prepareQuadFlat, flat block lighting)
// vs the real public method. Compares the single per-quad color + lightCoords (all 4 vertices same).
//
//   block_model_lighter_flat_parity --cases mcpp/build/bml_flat.tsv

#include "BlockModelLighterFlat.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ao = mc::render::block::aolight;
namespace j = mc::render::model::joml;
namespace card = mc::world::level::cardinal;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: block_model_lighter_flat_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, bad = 0;
    int shown = 0;
    auto fail = [&](const std::string& w) { ++bad; if (shown++ < 30) std::cerr << "MISMATCH " << w << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p[0] != "FLAT") continue;
        ++n;
        // FLAT dir card shade collisionFull emissive emission packAtPos packAtPosDir lm  p0..p3(12)  color light uniform faceCubic
        int dir = std::stoi(p[1]);
        const card::CardinalLighting& cl = (p[2] == "NETHER") ? card::NETHER : card::DEFAULT;
        bool materialShade = std::stoi(p[3]) != 0;
        bool collisionFull = std::stoi(p[4]) != 0;
        bool emissive = std::stoi(p[5]) != 0;
        int emission = std::stoi(p[6]);
        int packAtPos = std::stoi(p[7]);
        int packAtPosDir = std::stoi(p[8]);
        int lm = std::stoi(p[9]);
        j::Vector3f vp[4];
        for (int i = 0; i < 4; ++i) vp[i] = j::Vector3f{bf(p[10 + i * 3]), bf(p[11 + i * 3]), bf(p[12 + i * 3])};
        int expColor = std::stoi(p[22]);
        int expLight = std::stoi(p[23]);

        ao::FlatResult r = ao::prepareQuadFlat(vp, dir, materialShade, cl, lm, collisionFull,
                                               emissive, emission, packAtPos, packAtPosDir);
        if (r.color != expColor || r.lightCoords != expLight)
            fail("dir" + std::to_string(dir) + " " + p[2] + " sh" + p[3] + " got C=" + std::to_string(r.color) +
                 " L=" + std::to_string(r.lightCoords) + " exp C=" + p[22] + " L=" + p[23]);
    }
    std::cout << "BlockModelLighterFlat cases=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}

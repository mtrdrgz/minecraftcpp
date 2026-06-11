// Parity gate for aolight::prepareQuadAmbientOcclusionBlend (the AO smooth-lighting blend) vs the
// real BlockModelLighter.prepareQuadAmbientOcclusion, driven with an AIR level + position-varying
// brightness (known light gradient, uniform shade). Compares the 4 per-vertex colors + lightCoords.
//
//   block_model_lighter_ao_parity --cases mcpp/build/bml_ao.tsv

#include "BlockModelLighterAO.h"

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
    if (casesPath.empty()) { std::cerr << "usage: block_model_lighter_ao_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, bad = 0;
    int shown = 0;
    auto fail = [&](const std::string& w) { ++bad; if (shown++ < 30) std::cerr << "MISMATCH " << w << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p[0] != "AO") continue;
        ++n;
        int dir = std::stoi(p[1]);
        const card::CardinalLighting& cl = (p[2] == "NETHER") ? card::NETHER : card::DEFAULT;
        bool materialShade = std::stoi(p[3]) != 0;
        // p[4] = faceCubic (informational; C++ recomputes via prepareQuadShape with AIR collision=false)
        float us = bf(p[5]);
        j::Vector3f vp[4];
        for (int i = 0; i < 4; ++i) vp[i] = j::Vector3f{bf(p[6 + i * 3]), bf(p[7 + i * 3]), bf(p[8 + i * 3])};
        int l0 = std::stoi(p[18]), l1 = std::stoi(p[19]), l2 = std::stoi(p[20]), l3 = std::stoi(p[21]);
        int lc02 = std::stoi(p[22]), lc03 = std::stoi(p[23]), lc12 = std::stoi(p[24]), lc13 = std::stoi(p[25]);
        int lCenter = std::stoi(p[26]);

        ao::AOResult r = ao::prepareQuadAmbientOcclusionBlend(
            vp, dir, materialShade, cl, /*isCollisionShapeFullBlock=*/false,
            us, us, us, us, us, us, us, us, us,  // all shades uniform (AIR)
            l0, l1, l2, l3, lc02, lc03, lc12, lc13, lCenter);

        bool ok = true;
        for (int i = 0; i < 4 && ok; ++i) if (r.color[i] != std::stoi(p[27 + i])) ok = false;
        for (int i = 0; i < 4 && ok; ++i) if (r.lightCoords[i] != std::stoi(p[31 + i])) ok = false;
        if (!ok) {
            std::ostringstream m;
            m << "dir" << dir << " " << p[2] << " sh" << p[3] << " | got C[";
            for (int i = 0; i < 4; ++i) m << r.color[i] << (i < 3 ? "," : "");
            m << "] L[";
            for (int i = 0; i < 4; ++i) m << r.lightCoords[i] << (i < 3 ? "," : "");
            m << "]";
            fail(m.str());
        }
    }
    std::cout << "BlockModelLighterAO cases=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}

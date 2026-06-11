// Parity test for the PURE rendering math in com.mojang.blaze3d.platform.Lighting
// mirrored by render/Lighting.h. Ground truth: tools/LightingParity.java drives
// the REAL class' static diffuse-light constants (reflection) + the real
// org.joml pose transforms. Floats compared as raw IEEE-754 bits; the UBO size
// as decimal. All math is GL-free.
//
//   lighting_parity --cases mcpp/build/lighting.tsv

#include "Lighting.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace L = mc::render::lighting;
namespace j = mc::render::model::joml;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
uint32_t hx(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: lighting_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long checks = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n"; };

    auto v3 = [&](const j::Vector3f& got, const std::vector<std::string>& p, int o, const std::string& l) {
        ++checks;
        if (fb(got.x) != hx(p[o]) || fb(got.y) != hx(p[o + 1]) || fb(got.z) != hx(p[o + 2])) fail(l);
    };
    auto i1 = [&](int got, const std::string& exp, const std::string& l) {
        ++checks;
        if (got != std::stoi(exp)) fail(l);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];

        if (t == "DIFFUSE0")            v3(L::diffuseLight0(), p, 1, line);
        else if (t == "DIFFUSE1")       v3(L::diffuseLight1(), p, 1, line);
        else if (t == "NETHER0")        v3(L::netherDiffuseLight0(), p, 1, line);
        else if (t == "NETHER1")        v3(L::netherDiffuseLight1(), p, 1, line);
        else if (t == "INV0")           v3(L::inventoryDiffuseLight0(), p, 1, line);
        else if (t == "INV1")           v3(L::inventoryDiffuseLight1(), p, 1, line);
        else if (t == "UBO_SIZE")       i1(L::uboSize(), p[1], line);
        else if (t == "ITEMS_FLAT0")    v3(L::itemsFlat0(), p, 1, line);
        else if (t == "ITEMS_FLAT1")    v3(L::itemsFlat1(), p, 1, line);
        else if (t == "ITEMS_3D0")      v3(L::items3d0(), p, 1, line);
        else if (t == "ITEMS_3D1")      v3(L::items3d1(), p, 1, line);
        else if (t == "ENTITY_IN_UI0")  v3(L::entityInUi0(), p, 1, line);
        else if (t == "ENTITY_IN_UI1")  v3(L::entityInUi1(), p, 1, line);
        else if (t == "PLAYER_SKIN0")   v3(L::playerSkin0(), p, 1, line);
        else if (t == "PLAYER_SKIN1")   v3(L::playerSkin1(), p, 1, line);
        else if (t == "LEVEL_DEFAULT0") v3(L::levelDefault0(), p, 1, line);
        else if (t == "LEVEL_DEFAULT1") v3(L::levelDefault1(), p, 1, line);
        else if (t == "LEVEL_NETHER0")  v3(L::levelNether0(), p, 1, line);
        else if (t == "LEVEL_NETHER1")  v3(L::levelNether1(), p, 1, line);
        else { std::cerr << "unknown row tag: " << t << "\n"; return 2; }
    }

    std::cout << "Lighting checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

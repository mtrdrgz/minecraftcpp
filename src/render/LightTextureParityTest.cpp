// Parity test for mcpp/src/render/LightTextureMath.h — the pure brightness curve of
// net.minecraft.client.renderer.Lightmap (formerly LightTexture, Minecraft 26.1.2).
//
// Ground truth: tools/LightTextureParity.java vs the REAL net.minecraft classes.
// Both row families (GETBRIGHTNESS = real Lightmap.getBrightness per dimension;
// CURVE = real Mth.lerp over a broad ambient/level sweep) reduce to the same input
// pair (ambientLight float, level int), so both dispatch to the same C++ recompute.
// Floats are compared BIT-FOR-BIT via std::bit_cast.
//
//   light_texture_parity --cases mcpp/build/light_texture.tsv

#include "LightTextureMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace lt = mc::render::lighttexture;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: light_texture_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "GETBRIGHTNESS" || tag == "CURVE") {
            // <tag> ambientBits(f) level | outBits(f)
            if (p.size() < 4) continue;
            float ambient = bf(p[1]);
            int level = std::stoi(p[2]);
            uint32_t expect = static_cast<uint32_t>(std::stoul(p[3], nullptr, 16));
            uint32_t got = fb(lt::getBrightness(ambient, level));
            ++cases;
            if (got != expect) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << tag << " ambient=" << p[1] << " level=" << level
                              << " expect=" << p[3] << " got=" << std::hex << got << std::dec << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "LightTexture cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

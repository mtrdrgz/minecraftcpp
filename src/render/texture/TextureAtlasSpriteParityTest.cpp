// Parity test for mcpp/src/render/texture/TextureAtlasSpriteMath.h — the pure UV math of
// net.minecraft.client.renderer.texture.TextureAtlasSprite (Minecraft 26.1.2): the
// constructor's u0/u1/v0/v1 computation and getU/getV interpolation.
//
// Ground truth: tools/TextureAtlasSpriteParity.java vs the REAL net.minecraft class,
// constructed reflectively (Unsafe-allocated SpriteContents + protected ctor). Floats are
// compared BIT-FOR-BIT via std::bit_cast.
//
//   texture_atlas_sprite_parity --cases mcpp/build/texture_atlas_sprite.tsv

#include "TextureAtlasSpriteMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace tex = mc::render::texture;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t hx(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: texture_atlas_sprite_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long checks = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "BOUNDS") {
            // BOUNDS atlasW atlasH x y padding spriteW spriteH | u0 u1 v0 v1
            if (p.size() < 12) continue;
            int atlasW = std::stoi(p[1]), atlasH = std::stoi(p[2]);
            int x = std::stoi(p[3]), y = std::stoi(p[4]), padding = std::stoi(p[5]);
            int sw = std::stoi(p[6]), sh = std::stoi(p[7]);
            tex::TextureAtlasSpriteUv s = tex::TextureAtlasSpriteUv::make(atlasW, atlasH, x, y, padding, sw, sh);
            const uint32_t expU0 = hx(p[8]), expU1 = hx(p[9]), expV0 = hx(p[10]), expV1 = hx(p[11]);
            const uint32_t gotU0 = fb(s.getU0()), gotU1 = fb(s.getU1()), gotV0 = fb(s.getV0()), gotV1 = fb(s.getV1());
            for (int k = 0; k < 4; ++k) {
                ++checks;
                uint32_t exp = k == 0 ? expU0 : k == 1 ? expU1 : k == 2 ? expV0 : expV1;
                uint32_t got = k == 0 ? gotU0 : k == 1 ? gotU1 : k == 2 ? gotV0 : gotV1;
                if (got != exp) {
                    ++mismatches;
                    if (mismatches <= 20)
                        std::cerr << "BOUNDS[" << k << "] atlas=" << atlasW << "x" << atlasH
                                  << " xy=" << x << "," << y << " pad=" << padding
                                  << " size=" << sw << "x" << sh
                                  << " expect=" << std::hex << exp << " got=" << got << std::dec << "\n";
                }
            }
        } else if (tag == "GETUV") {
            // GETUV atlasW atlasH x y padding spriteW spriteH offset | getU getV
            if (p.size() < 11) continue;
            int atlasW = std::stoi(p[1]), atlasH = std::stoi(p[2]);
            int x = std::stoi(p[3]), y = std::stoi(p[4]), padding = std::stoi(p[5]);
            int sw = std::stoi(p[6]), sh = std::stoi(p[7]);
            float off = bf(p[8]);
            tex::TextureAtlasSpriteUv s = tex::TextureAtlasSpriteUv::make(atlasW, atlasH, x, y, padding, sw, sh);
            const uint32_t expU = hx(p[9]), expV = hx(p[10]);
            const uint32_t gotU = fb(s.getU(off)), gotV = fb(s.getV(off));
            ++checks;
            if (gotU != expU) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << "GETUV(U) atlas=" << atlasW << "x" << atlasH << " off=" << p[8]
                              << " expect=" << p[9] << " got=" << std::hex << gotU << std::dec << "\n";
            }
            ++checks;
            if (gotV != expV) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << "GETUV(V) atlas=" << atlasW << "x" << atlasH << " off=" << p[8]
                              << " expect=" << p[10] << " got=" << std::hex << gotV << std::dec << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "TextureAtlasSprite checks=" << checks << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

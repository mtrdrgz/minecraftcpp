// Headless verification of the mesher biome-tint blend (BiomeTint.h) against the
// real colormaps + biome JSON. Needs tools/provision_runtime.sh.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "BiomeTint.h"
#include "../../world/level/biome/BiomeRegistry.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {
bool g_ok = true;
void check(bool c, const char* label) { g_ok &= c; std::printf("%-34s %s\n", label, c ? "OK" : "FAIL"); }

std::vector<std::int32_t> loadArgb(const std::string& path) {
    int w = 0, h = 0, ch = 0;
    unsigned char* d = stbi_load(path.c_str(), &w, &h, &ch, 3);
    if (!d) return {};
    std::vector<std::int32_t> px(static_cast<std::size_t>(w) * h);
    for (int i = 0; i < w * h; ++i)
        px[i] = std::int32_t(0xFF000000u) | (d[i*3] << 16) | (d[i*3+1] << 8) | d[i*3+2];
    stbi_image_free(d);
    return px;
}
}  // namespace

int main(int argc, char** argv) {
    std::string cm = "assets/client-extract/assets/minecraft/textures/colormap";
    std::string bd = "26.1.2/data/minecraft/worldgen/biome";
    for (int a = 1; a + 1 < argc; ++a) {
        if (std::string(argv[a]) == "--colormap") cm = argv[++a];
        else if (std::string(argv[a]) == "--biomes") bd = argv[++a];
    }
    const auto grass = loadArgb(cm + "/grass.png");
    const auto foliage = loadArgb(cm + "/foliage.png");
    if (grass.size() != 65536 || foliage.size() != 65536) { std::fprintf(stderr, "bad colormaps\n"); return 2; }

    const mc::biome::BiomeRegistry reg = mc::biome::BiomeRegistry::loadFromDirectory(bd);
    const mc::biome::Biome* plains = reg.find("minecraft:plains");
    const mc::biome::Biome* forest = reg.find("minecraft:forest");
    if (!plains || !forest) { std::fprintf(stderr, "biomes missing\n"); return 2; }

    using namespace mc::render::biometint;

    // Uniform plains, radius 0 -> #91BD59.
    auto allPlains = [&](int, int, int) { return plains; };
    auto t0 = tint("grass_block_top", allPlains, 100, 64, 200, grass, foliage, 0);
    check(t0 && t0->r == 0x91 && t0->g == 0xBD && t0->b == 0x59, "plains grass r0 = #91BD59");

    // Uniform plains, radius 2 (blend of identical columns) -> still #91BD59.
    auto t2 = tint("grass_block_top", allPlains, 100, 64, 200, grass, foliage, 2);
    check(t2 && t2->r == 0x91 && t2->g == 0xBD && t2->b == 0x59, "plains grass r2 = #91BD59");

    // Water resolver returns the biome's waterColor (no blend modifier).
    auto tw = tint("water_still", allPlains, 0, 64, 0, grass, foliage, 0);
    const std::uint32_t wc = plains->effects.waterColor;
    check(tw && tw->r == ((wc >> 16) & 0xFF) && tw->g == ((wc >> 8) & 0xFF) && tw->b == (wc & 0xFF),
          "plains water = biome waterColor");

    // Non-tinted texture -> nullopt (mesher keeps its own handling).
    check(!tint("stone", allPlains, 0, 64, 0, grass, foliage), "stone -> no biome tint");

    // Two-biome blend (plains for dx<0, forest for dx>=0) lands strictly between the two.
    auto split = [&](int x, int, int) { return x < 100 ? plains : forest; };
    auto tb = tint("grass_block_top", split, 100, 64, 200, grass, foliage, 2);
    // plains #91BD59, forest #79C05A — blended green channel between 0xBD and 0xC0.
    bool between = tb && tb->g >= 0xBD && tb->g <= 0xC0 && tb->r >= 0x79 && tb->r <= 0x91;
    check(between, "plains|forest blend is in-between");

    std::printf("\n%s\n", g_ok ? "ALL OK" : "FAILURES PRESENT");
    return g_ok ? 0 : 1;
}

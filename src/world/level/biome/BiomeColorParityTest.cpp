// Runnable, headless verification of the biome grass/foliage colour resolution
// (BiomeColor.h) against the REAL client colormap + REAL biome worldgen JSON.
//
// Ground truth: the grass.png / foliage.png colormaps shipped in the client jar,
// decoded independently, indexed by the certified ColorMapColorUtil formula. This
// needs the provisioned runtime (tools/provision_runtime.sh):
//   assets/client-extract/assets/minecraft/textures/colormap/{grass,foliage}.png
//   26.1.2/data/minecraft/worldgen/biome/*.json

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "BiomeColor.h"
#include "BiomeRegistry.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {
bool g_ok = true;

std::vector<std::int32_t> loadColormapArgb(const std::string& path) {
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 3);
    if (!data) { std::fprintf(stderr, "cannot load %s\n", path.c_str()); return {}; }
    std::vector<std::int32_t> pixels(static_cast<std::size_t>(w) * h);
    for (int i = 0; i < w * h; ++i) {
        const int r = data[i * 3 + 0], gg = data[i * 3 + 1], b = data[i * 3 + 2];
        pixels[i] = static_cast<std::int32_t>(0xFF000000u) | (r << 16) | (gg << 8) | b;
    }
    stbi_image_free(data);
    return pixels;
}

void checkHex(const char* label, std::int32_t got, std::uint32_t expectedRgb) {
    const std::uint32_t rgb = static_cast<std::uint32_t>(got) & 0xFFFFFFu;
    const bool ok = rgb == expectedRgb;
    g_ok &= ok;
    std::printf("%-28s got=#%06X expected=#%06X %s\n", label, rgb, expectedRgb, ok ? "OK" : "FAIL");
}
}  // namespace

int main(int argc, char** argv) {
    std::string assets = "assets/client-extract/assets/minecraft/textures/colormap";
    std::string biomeDir = "26.1.2/data/minecraft/worldgen/biome";
    for (int a = 1; a + 1 < argc; ++a) {
        if (std::string(argv[a]) == "--colormap") assets = argv[++a];
        else if (std::string(argv[a]) == "--biomes") biomeDir = argv[++a];
    }

    const std::vector<std::int32_t> grass = loadColormapArgb(assets + "/grass.png");
    const std::vector<std::int32_t> foliage = loadColormapArgb(assets + "/foliage.png");
    if (grass.size() != 65536 || foliage.size() != 65536) {
        std::fprintf(stderr, "colormaps not 256x256 (grass=%zu foliage=%zu)\n", grass.size(), foliage.size());
        return 2;
    }

    const mc::biome::BiomeRegistry reg = mc::biome::BiomeRegistry::loadFromDirectory(biomeDir);

    auto grassOf = [&](const std::string& id) {
        const mc::biome::Biome* b = reg.find("minecraft:" + id);
        if (!b) { std::fprintf(stderr, "biome %s missing\n", id.c_str()); g_ok = false; return 0; }
        return mc::biome::color::grassColor(*b, 0.0, 0.0, grass);
    };

    // Expected values cross-verified by a second, independent PNG decoder against the
    // real grass.png colormap (plains temp=0.8/downfall=0.4, forest 0.7/0.8).
    checkHex("plains grass",  grassOf("plains"),  0x91BD59);
    checkHex("forest grass",  grassOf("forest"),  0x79C05A);

    // GrassColorModifier formula checks (independent of biome data).
    using namespace mc::biome;
    checkHex("dark_forest modifier",
             color::modifyGrassColor(GrassColorModifier::DARK_FOREST, 0, 0, 0x91BD59),
             // ((0x91BD59 & 0xFEFEFE) + 2634762) >> 1 = 0x5C7831
             0x5C7831);
    // SWAMP is position-noise driven; just assert it returns one of the two swamp colors.
    {
        const std::int32_t c = color::modifyGrassColor(GrassColorModifier::SWAMP, 12.0, 34.0, 0);
        const std::uint32_t rgb = static_cast<std::uint32_t>(c) & 0xFFFFFFu;
        const bool ok = (rgb == 0x4C763C) || (rgb == 0x6A7039);
        g_ok &= ok;
        std::printf("%-28s got=#%06X expected #4C763C|#6A7039 %s\n", "swamp modifier", rgb, ok ? "OK" : "FAIL");
    }

    std::printf("\n%s\n", g_ok ? "ALL OK" : "FAILURES PRESENT");
    return g_ok ? 0 : 1;
}

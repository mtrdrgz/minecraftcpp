// Test for the faithful applyBiomeDecoration() step. Builds a synthetic flat
// plains chunk, runs decoration, and checks the ported VEGETAL_DECORATION
// features (patch_grass_plain / flower_plains) place onto grass_block,
// deterministically, gated by the real minecraft:biome filter.
//
// Ground truth for the seeding/order is the biome JSON (plains step-9 list:
// index 4 = flower_plains, index 5 = patch_grass_plain) + the placed_feature
// chains; this exercises the whole real pipeline, not the old heuristic.

#include "BiomeDecorator.h"
#include "BiomeFeatures.h"
#include "../../block/BlockState.h"
#include "../../block/BlockTags.h"
#include "../../block/Blocks.h"
#include "../../chunk/LevelChunk.h"

#include <iostream>
#include <map>
#include <string>
#include <tuple>

using namespace mc;
using namespace mc::levelgen::feature;

namespace {
bool g_ok = true;
void check(bool c, const std::string& label) {
    if (!c) { g_ok = false; std::cerr << "FAIL: " << label << '\n'; }
}

std::string nameOf(uint32_t id) {
    const BlockState* s = getBlockState(id);
    return (s && s->block) ? ("minecraft:" + s->block->name) : "minecraft:air";
}

// A flat plains chunk: grass_block at y=70 over dirt, air above.
std::unique_ptr<LevelChunk> makePlains(int cx, int cz) {
    auto c = std::make_unique<LevelChunk>(ChunkPos{ cx, cz });
    const uint32_t grass = getDefaultBlockStateId("grass_block", 0);
    const uint32_t dirt = getDefaultBlockStateId("dirt", 0);
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) {
            const int wx = cx * 16 + x, wz = cz * 16 + z;
            for (int y = 60; y < 70; ++y) c->setBlock(wx, y, wz, dirt);
            c->setBlock(wx, 70, wz, grass);
        }
    c->computeHeightmap();
    return c;
}

// Collect plants placed above the surface as (x,y,z)->block.
std::map<std::tuple<int, int, int>, std::string> collectPlants(const LevelChunk& c) {
    std::map<std::tuple<int, int, int>, std::string> out;
    const ChunkPos p = c.pos();
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z)
            for (int y = 71; y < 80; ++y) {
                const uint32_t id = c.getBlock(p.x * 16 + x, y, p.z * 16 + z);
                if (id != 0) out[{ x, y, z }] = nameOf(id);
            }
    return out;
}
} // namespace

int main() {
    initBlocks();
    BiomeFeatures bf;
    mc::block::BlockTags tags;
    try {
        bf = BiomeFeatures::loadFromDirectory("26.1.2/data/minecraft/worldgen/biome");
        tags = mc::block::BlockTags::loadFromDirectory("26.1.2/data/minecraft/tags/block");
    } catch (const std::exception& e) {
        std::cerr << "load error: " << e.what() << '\n';
        return 2;
    }

    // Data parity: the plains VEGETAL_DECORATION (step 9) order pins the seeds.
    const auto& veg = bf.featuresForStep("minecraft:plains", GenerationStep::VEGETAL_DECORATION);
    check(veg.size() == 11, "plains step-9 has 11 features");
    if (veg.size() == 11) {
        check(veg[4] == "minecraft:flower_plains", "plains[9][4] == flower_plains");
        check(veg[5] == "minecraft:patch_grass_plain", "plains[9][5] == patch_grass_plain");
    }
    check(bf.biomeHasFeature("minecraft:plains", 9, "minecraft:patch_grass_plain"), "biome filter: plains has grass");
    check(!bf.biomeHasFeature("minecraft:ocean", 9, "minecraft:patch_grass_plain"), "biome filter: ocean lacks grass");

    auto plains = [](int, int, int) { return std::string("minecraft:plains"); };
    auto ocean = [](int, int, int) { return std::string("minecraft:ocean"); };

    // Decorate a plains chunk.
    auto c1 = makePlains(0, 0);
    applyBiomeDecoration(*c1, 42LL, plains, bf, tags);
    auto plants1 = collectPlants(*c1);
    check(!plants1.empty(), "plains chunk got vegetation");

    int grassN = 0, flowerN = 0, onGrass = 0;
    for (auto& [pos, name] : plants1) {
        const auto [x, y, z] = pos;
        if (name == "minecraft:short_grass") ++grassN;
        else ++flowerN; // dandelion/poppy/tulips/etc.
        if (y == 71 && nameOf(c1->getBlock(x, 70, z)) == "minecraft:grass_block") ++onGrass;
    }
    check(grassN > 0, "short_grass placed (got " + std::to_string(grassN) + ")");
    check(onGrass == (int)plants1.size(), "every plant sits on grass_block at y=71");
    std::cerr << "  placed " << grassN << " grass + " << flowerN << " flowers\n";

    // Determinism: same seed + chunk => byte-identical placement.
    auto c1b = makePlains(0, 0);
    applyBiomeDecoration(*c1b, 42LL, plains, bf, tags);
    check(collectPlants(*c1b) == plants1, "decoration is deterministic");

    // Biome gating: an ocean column gets none of the plains plants.
    auto c2 = makePlains(0, 0);
    applyBiomeDecoration(*c2, 42LL, ocean, bf, tags);
    check(collectPlants(*c2).empty(), "ocean biome places no plains vegetation");

    // Different chunk coords => different decoration seed => different layout.
    auto c3 = makePlains(5, -3);
    applyBiomeDecoration(*c3, 42LL, plains, bf, tags);
    // compare relative (x,y,z)->name maps; overwhelmingly likely to differ
    check(collectPlants(*c3) != plants1, "different chunk => different layout");

    if (!g_ok) { std::cerr << "BiomeDecorator tests FAILED\n"; return 1; }
    std::cout << "BiomeDecorator tests passed (" << bf.biomeCount() << " biomes loaded)\n";
    return 0;
}

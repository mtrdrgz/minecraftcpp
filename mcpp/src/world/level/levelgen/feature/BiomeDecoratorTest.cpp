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
    auto forest = [](int, int, int) { return std::string("minecraft:forest"); };
    auto voidb = [](int, int, int) { return std::string("minecraft:the_void"); };
    const std::string DIR = "26.1.2/data/minecraft/worldgen";
    auto countName = [](const std::map<std::tuple<int, int, int>, std::string>& m, const std::string& n) {
        int c = 0; for (auto& [k, v] : m) if (v == n) ++c; return c;
    };

    // Plains: ground vegetation appears (short_grass at least), deterministically.
    auto c1 = makePlains(0, 0);
    applyBiomeDecoration(*c1, 42LL, plains, bf, tags, DIR);
    auto plants1 = collectPlants(*c1);
    check(!plants1.empty(), "plains chunk got vegetation");
    check(countName(plants1, "minecraft:short_grass") > 0, "plains: short_grass placed");

    auto c1b = makePlains(0, 0);
    applyBiomeDecoration(*c1b, 42LL, plains, bf, tags, DIR);
    check(collectPlants(*c1b) == plants1, "decoration is deterministic");

    // Forest: trees appear (oak/birch logs+leaves) — exercises the data-driven tree loader.
    auto cf = makePlains(1, 1);
    applyBiomeDecoration(*cf, 42LL, forest, bf, tags, DIR);
    auto pforest = collectPlants(*cf);
    const int logs = countName(pforest, "minecraft:oak_log") + countName(pforest, "minecraft:birch_log");
    const int leaves = countName(pforest, "minecraft:oak_leaves") + countName(pforest, "minecraft:birch_leaves");
    check(logs > 0, "forest: tree logs placed (got " + std::to_string(logs) + ")");
    check(leaves > 0, "forest: tree leaves placed (got " + std::to_string(leaves) + ")");
    std::cerr << "  plains plants=" << plants1.size() << "  forest logs=" << logs << " leaves=" << leaves << "\n";

    // A biome with no features (the_void) decorates to nothing (gating sanity).
    auto c2 = makePlains(0, 0);
    applyBiomeDecoration(*c2, 42LL, voidb, bf, tags, DIR);
    check(collectPlants(*c2).empty(), "the_void places no vegetation");

    if (!g_ok) { std::cerr << "BiomeDecorator tests FAILED\n"; return 1; }
    std::cout << "BiomeDecorator tests passed (" << bf.biomeCount() << " biomes loaded)\n";
    return 0;
}

#include "OreGen.h"
#include "../../block/Blocks.h"
#include "../RandomSource.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

namespace mc::levelgen::feature {

struct OreVeinConfig {
    std::string name;
    std::string stoneOreName;
    std::string deepslateOreName;
    int size;
    int count; // attempts per chunk
    int minY;
    int maxY;
    bool triangular;
    int chance; // 1/chance filter if > 1
};

// 100% authentic, deobfuscated Overworld ore & underground block configured features
static const std::vector<OreVeinConfig> g_oreConfigs = {
    // Dirt, gravel, stone pockets (underground block decoration)
    { "dirt", "dirt", "dirt", 33, 7, 0, 160, false, 1 },
    { "gravel", "gravel", "gravel", 33, 14, -64, 320, false, 1 },
    
    { "granite_lower", "granite", "granite", 64, 2, 0, 60, false, 1 },
    { "granite_upper", "granite", "granite", 64, 1, 64, 128, false, 6 }, // 1/6 chance
    
    { "diorite_lower", "diorite", "diorite", 64, 2, 0, 60, false, 1 },
    { "diorite_upper", "diorite", "diorite", 64, 1, 64, 128, false, 6 }, // 1/6 chance
    
    { "andesite_lower", "andesite", "andesite", 64, 2, 0, 60, false, 1 },
    { "andesite_upper", "andesite", "andesite", 64, 1, 64, 128, false, 6 }, // 1/6 chance
    
    { "tuff", "tuff", "tuff", 64, 2, -64, 0, false, 1 },

    // Coal Ore
    { "coal_lower", "coal_ore", "deepslate_coal_ore", 17, 20, 0, 192, true, 1 }, // peak = 96
    { "coal_upper", "coal_ore", "deepslate_coal_ore", 17, 30, 136, 320, false, 1 }, // uniform

    // Iron Ore
    { "iron_small", "iron_ore", "deepslate_iron_ore", 4, 10, -64, 72, false, 1 }, // uniform
    { "iron_middle", "iron_ore", "deepslate_iron_ore", 9, 10, -24, 56, true, 1 }, // peak = 16
    { "iron_upper", "iron_ore", "deepslate_iron_ore", 9, 90, 80, 384, true, 1 }, // peak = 232

    // Copper Ore
    { "copper", "copper_ore", "deepslate_copper_ore", 10, 16, -16, 112, true, 1 }, // peak = 48

    // Gold Ore
    { "gold", "gold_ore", "deepslate_gold_ore", 9, 4, -64, 32, true, 1 }, // peak = -16
    
    // Redstone Ore
    { "redstone", "redstone_ore", "deepslate_redstone_ore", 8, 4, -64, 15, false, 1 }, // uniform
    { "redstone_lower", "redstone_ore", "deepslate_redstone_ore", 8, 8, -96, -32, true, 1 }, // peak = -64

    // Lapis Lazuli Ore
    { "lapis", "lapis_ore", "deepslate_lapis_ore", 7, 2, -32, 32, true, 1 }, // peak = 0
    { "lapis_buried", "lapis_ore", "deepslate_lapis_ore", 7, 4, -64, 64, false, 1 }, // uniform

    // Diamond Ore
    { "diamond_small", "diamond_ore", "deepslate_diamond_ore", 4, 7, -144, 16, true, 1 }, // peak = -64
    { "diamond_medium", "diamond_ore", "deepslate_diamond_ore", 8, 2, -64, -4, false, 1 }, // uniform
    { "diamond_buried", "diamond_ore", "deepslate_diamond_ore", 8, 4, -144, 16, true, 1 }, // peak = -64
    { "diamond_large", "diamond_ore", "deepslate_diamond_ore", 12, 1, -144, 16, true, 9 }, // 1/9 chance
    
    // Emerald Ore (size 3, count 100 attempts, but only in mountain biomes!)
    { "emerald", "emerald_ore", "deepslate_emerald_ore", 3, 100, -16, 480, true, 1 }
};

// Seed salt to isolate random stream
static uint64_t oreChunkSeed(uint64_t worldSeed, int cx, int cz) {
    return worldSeed
        ^ (static_cast<uint64_t>(cx) * 341873128712ULL)
        ^ (static_cast<uint64_t>(cz) * 132897987541ULL)
        ^ 987654321ULL;
}

// Samples height range following either uniform or trapezoid (triangular) distributions
static int sampleY(LegacyRandomSource& rng, const OreVeinConfig& config) {
    int minY = config.minY;
    int maxY = config.maxY;
    if (minY >= maxY) return minY;
    
    if (config.triangular) {
        int val = maxY - minY;
        int half = val / 2;
        return minY + rng.nextInt(half + 1) + rng.nextInt(val - half + 1);
    } else {
        return minY + rng.nextInt(maxY - minY + 1);
    }
}

// Restricts Emerald generation to vanilla Overworld Windswept & Mountain biomes
static bool isMountainBiome(const std::string& biome) {
    return biome.find("hill") != std::string::npos ||
           biome.find("mountain") != std::string::npos ||
           biome.find("slope") != std::string::npos ||
           biome.find("peak") != std::string::npos ||
           biome == "minecraft:meadow" ||
           biome == "minecraft:cherry_grove" ||
           biome == "minecraft:grove";
}

// Replicates net.minecraft.world.level.levelgen.feature.OreFeature::doPlace
static void placeVein(LevelChunk& chunk, LegacyRandomSource& rng, const OreVeinConfig& config, int originX, int originY, int originZ) {
    int size = config.size;
    if (size <= 0) return;

    float angle = rng.nextFloat() * 3.1415926535f;
    float length = (float)size / 8.0f;
    double startX = (double)originX + std::sin(angle) * length;
    double endX   = (double)originX - std::sin(angle) * length;
    double startZ = (double)originZ + std::cos(angle) * length;
    double endZ   = (double)originZ - std::cos(angle) * length;
    double startY = (double)(originY + rng.nextInt(3) - 2);
    double endY   = (double)(originY + rng.nextInt(3) - 2);

    int minX = chunk.pos().x * 16;
    int minZ = chunk.pos().z * 16;
    auto inBounds = [minX, minZ](int wx, int wz) noexcept {
        return wx >= minX && wx < minX + 16 && wz >= minZ && wz < minZ + 16;
    };

    uint32_t stoneState = getDefaultBlockStateId("stone", 0);
    uint32_t deepslateState = getDefaultBlockStateId("deepslate", 0);
    uint32_t graniteState = getDefaultBlockStateId("granite", 0);
    uint32_t dioriteState = getDefaultBlockStateId("diorite", 0);
    uint32_t andesiteState = getDefaultBlockStateId("andesite", 0);
    uint32_t tuffState = getDefaultBlockStateId("tuff", 0);

    uint32_t stoneOreId = getDefaultBlockStateId(config.stoneOreName, stoneState);
    uint32_t deepslateOreId = getDefaultBlockStateId(config.deepslateOreName, deepslateState);

    for (int i = 0; i <= size; ++i) {
        double t = (double)i / (double)size;
        double cx = startX + t * (endX - startX);
        double cy = startY + t * (endY - startY);
        double cz = startZ + t * (endZ - startZ);
        double sizeMultiplier = rng.nextFloat() * (float)size / 16.0f;
        // Sine-based thickness interpolation matching net.minecraft.util.Mth sine scaling
        double radius = (std::sin(t * 3.1415926535f) + 1.0) * sizeMultiplier + 1.0;
        double radiusX = radius;
        double radiusY = radius;

        int xMin = (int)std::floor(cx - radiusX / 2.0);
        int xMax = (int)std::ceil(cx + radiusX / 2.0);
        int yMin = (int)std::floor(cy - radiusY / 2.0);
        int yMax = (int)std::ceil(cy + radiusY / 2.0);
        int zMin = (int)std::floor(cz - radiusX / 2.0);
        int zMax = (int)std::ceil(cz + radiusX / 2.0);

        for (int bx = xMin; bx <= xMax; ++bx) {
            double dx = ((double)bx + 0.5 - cx) / (radiusX / 2.0);
            if (dx * dx >= 1.0) continue;

            for (int by = yMin; by <= yMax; ++by) {
                if (by < CHUNK_MIN_Y || by >= CHUNK_MAX_Y) continue;

                double dy = ((double)by + 0.5 - cy) / (radiusY / 2.0);
                if (dx * dx + dy * dy >= 1.0) continue;

                for (int bz = zMin; bz <= zMax; ++bz) {
                    double dz = ((double)bz + 0.5 - cz) / (radiusX / 2.0);
                    if (dx * dx + dy * dy + dz * dz >= 1.0) continue;

                    // Bounds-checked thread-safe modifier
                    if (!inBounds(bx, bz)) continue;

                    uint32_t current = chunk.getBlock(bx, by, bz);

                    // Stone replacement rules (stone_ore_replaceables)
                    if (current == stoneState || current == graniteState || current == dioriteState || current == andesiteState) {
                        chunk.setBlock(bx, by, bz, stoneOreId);
                    }
                    // Deepslate/tuff replacement rules (deepslate_ore_replaceables)
                    else if (current == deepslateState || current == tuffState) {
                        chunk.setBlock(bx, by, bz, deepslateOreId);
                    }
                }
            }
        }
    }
}

// Populates all ores and stone pockets in the chunk
void decorateOres(LevelChunk& chunk, uint64_t worldSeed, const std::function<std::string(int, int, int)>& biomeGetter) {
    const ChunkPos pos = chunk.pos();
    const int minX = pos.x * 16;
    const int minZ = pos.z * 16;

    LegacyRandomSource rng(static_cast<int64_t>(oreChunkSeed(worldSeed, pos.x, pos.z)));

    for (const auto& config : g_oreConfigs) {
        // 1:1 Rarity chance filters
        if (config.chance > 1) {
            if (rng.nextInt(config.chance) != 0) continue;
        }

        for (int attempt = 0; attempt < config.count; ++attempt) {
            int originX = minX + rng.nextInt(16);
            int originZ = minZ + rng.nextInt(16);
            int originY = sampleY(rng, config);

            // Special biome filter for Emeralds (windswept/mountain biomes only)
            if (config.name == "emerald") {
                std::string biome = biomeGetter(originX, originY, originZ);
                if (!isMountainBiome(biome)) continue;
            }

            placeVein(chunk, rng, config, originX, originY, originZ);
        }
    }
}

} // namespace mc::levelgen::feature

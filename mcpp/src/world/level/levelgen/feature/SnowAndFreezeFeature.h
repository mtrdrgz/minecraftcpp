#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.SnowAndFreezeFeature
// ("freeze_top_layer") plus the Biome climate predicates it calls:
//   Biome.shouldFreeze(level, pos, false)  (Biome.java:145-169)
//   Biome.shouldSnow(level, pos)           (Biome.java:183-196)
//   Biome.getHeightAdjustedTemperature     (Biome.java:112-121; TEMPERATURE_NOISE =
//     PerlinSimplexNoise(WorldgenRandom(LegacyRandomSource(1234)), [0]), Biome.java:61)
//   Biome.TemperatureModifier.FROZEN       (Biome.java:395-409; FROZEN_TEMPERATURE_NOISE
//     seed 3456 octaves [-2,-1,0], BIOME_INFO_NOISE seed 2345 octaves [0], Biome.java:62-66)
//
// Per column (dx 0..15 OUTER, dz 0..15 INNER — SnowAndFreezeFeature.java:26-45):
//   y      = getHeight(MOTION_BLOCKING, x, z)        (WorldGenRegion: stored top + 1)
//   biome  = level.getBiome(topPos)                  (the ZOOMED biome manager)
//   if biome.shouldFreeze(level, below, false): setBlock(below, ICE, 2)
//   if biome.shouldSnow(level, top):  setBlock(top, SNOW, 2); if below
//     hasProperty(SNOWY) (SnowyDirtBlock family: grass_block / podzol / mycelium —
//     Blocks.java registrations) setBlock(below, below[snowy=true], 2) — id-invisible,
//     performed for write parity.
//
// NO RNG draws at all: the random parameter is never touched (worldgen brightness
// is 0 everywhere — the certified GT proxy returns 0, FullChunkDecorateParity.java).
//
// Temperature is a pure function (the Java per-biome 1024-entry cache only
// memoizes the same deterministic value, Biome.java:123-139).

#include "TreeFeature.h"   // WorldGenLevel/BlockPos/hooks plumbing + mthFloor
#include "../Noise.h"
#include "../RandomSource.h"

#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace mc::levelgen::feature {

// Per-biome climate settings, loaded from data biome JSON (Biome.ClimateSettings
// codec: has_precipitation, temperature, optional temperature_modifier, downfall).
struct BiomeClimate {
    float temperature = 0.0f;
    bool hasPrecipitation = true;
    bool frozenModifier = false;   // temperature_modifier == "frozen"
};

struct SnowFreezeHooks {
    std::function<std::string(BlockPos)> getBiome;                    // zoomed biome id at pos
    std::function<const BiomeClimate&(const std::string&)> climate;   // biome id -> climate (throws if unknown: fail-closed)
    std::function<bool(const std::string&)> isAir;
    // SnowLayerBlock.canSurvive (SnowLayerBlock.java:77-86) on the state BELOW pos:
    // #cannot_support_snow_layer -> false; #support_override_snow_layer -> true; else
    // collision-shape top face full (Block.isFaceFull(getCollisionShape, UP)) ||
    // (below is snow && layers == 8 — never true at worldgen: features place layers=1).
    std::function<bool(BlockPos)> snowCanSurvive;
    std::function<bool(const std::string&)> hasSnowyProperty;         // grass_block/podzol/mycelium
    // FluidState at a stored state + the LiquidBlock check: shouldFreeze requires
    // fluid.is(Fluids.WATER) && block instanceof LiquidBlock — in the id grid both
    // hold exactly for "minecraft:water" (worldgen writes source water only).
    int seaLevel = 63;
    int levelMinY = 0, levelMaxY = 0;                                 // isInsideBuildHeight bounds (max EXCLUSIVE)
};

namespace snowfreeze_detail {

// Biome.TEMPERATURE_NOISE / FROZEN_TEMPERATURE_NOISE / BIOME_INFO_NOISE (Biome.java:61-66).
inline const PerlinSimplexNoise& temperatureNoise() {
    static const PerlinSimplexNoise noise = [] {
        WorldgenRandom random(std::make_shared<LegacyRandomSource>(1234));
        return PerlinSimplexNoise(random, { 0 });
    }();
    return noise;
}
inline const PerlinSimplexNoise& frozenTemperatureNoise() {
    static const PerlinSimplexNoise noise = [] {
        WorldgenRandom random(std::make_shared<LegacyRandomSource>(3456));
        return PerlinSimplexNoise(random, { -2, -1, 0 });
    }();
    return noise;
}
inline const PerlinSimplexNoise& biomeInfoNoise() {
    static const PerlinSimplexNoise noise = [] {
        WorldgenRandom random(std::make_shared<LegacyRandomSource>(2345));
        return PerlinSimplexNoise(random, { 0 });
    }();
    return noise;
}

// Biome.TemperatureModifier.modifyTemperature (Biome.java:389-409).
inline float modifyTemperature(const BiomeClimate& climate, BlockPos pos) {
    if (!climate.frozenModifier) return climate.temperature;
    const double groundValueLargeVariation = frozenTemperatureNoise().getValue(pos.x * 0.05, pos.z * 0.05, false) * 7.0;
    const double groundValueEdgeVariation = biomeInfoNoise().getValue(pos.x * 0.2, pos.z * 0.2, false);
    const double icePatches = groundValueLargeVariation + groundValueEdgeVariation;
    if (icePatches < 0.3) {
        const double groundValueSmallVariation = biomeInfoNoise().getValue(pos.x * 0.09, pos.z * 0.09, false);
        if (groundValueSmallVariation < 0.8) {
            return 0.2f;
        }
    }
    return climate.temperature;
}

// Biome.getHeightAdjustedTemperature (Biome.java:112-121). NOTE pos.getX()/8.0F:
// int / FLOAT literal -> float division, then widened to double for getValue.
inline float heightAdjustedTemperature(const BiomeClimate& climate, BlockPos pos, int seaLevel) {
    const float adjusted = modifyTemperature(climate, pos);
    const int snowLevel = seaLevel + 17;
    if (pos.y > snowLevel) {
        const float v = static_cast<float>(temperatureNoise().getValue(
            static_cast<double>(static_cast<float>(pos.x) / 8.0f),
            static_cast<double>(static_cast<float>(pos.z) / 8.0f), false) * 8.0);
        return adjusted - (v + static_cast<float>(pos.y) - static_cast<float>(snowLevel)) * 0.05f / 40.0f;
    }
    return adjusted;
}

// Biome.warmEnoughToRain (Biome.java:175-177).
inline bool warmEnoughToRain(const BiomeClimate& climate, BlockPos pos, int seaLevel) {
    return heightAdjustedTemperature(climate, pos, seaLevel) >= 0.15f;
}

} // namespace snowfreeze_detail

// Biome.shouldFreeze(level, pos, checkNeighbors) (Biome.java:145-169). Worldgen
// brightness is 0 (< 10) always.
inline bool biomeShouldFreeze(const SnowFreezeHooks& hooks, const BiomeClimate& climate,
                              WorldGenLevel& level, BlockPos pos, bool checkNeighbors) {
    using namespace snowfreeze_detail;
    if (warmEnoughToRain(climate, pos, hooks.seaLevel)) return false;
    if (pos.y >= hooks.levelMinY && pos.y < hooks.levelMaxY) {
        // fluid.is(Fluids.WATER) && block instanceof LiquidBlock == id "minecraft:water".
        if (level.getBlockState(pos) == "minecraft:water") {
            if (!checkNeighbors) return true;
            const bool surroundedByWater =
                level.getBlockState(BlockPos{ pos.x - 1, pos.y, pos.z }) == "minecraft:water"
                && level.getBlockState(BlockPos{ pos.x + 1, pos.y, pos.z }) == "minecraft:water"
                && level.getBlockState(BlockPos{ pos.x, pos.y, pos.z - 1 }) == "minecraft:water"
                && level.getBlockState(BlockPos{ pos.x, pos.y, pos.z + 1 }) == "minecraft:water";
            if (!surroundedByWater) return true;
        }
    }
    return false;
}

// Biome.shouldSnow (Biome.java:183-196): precipitation SNOW (hasPrecipitation &&
// coldEnoughToSnow) && (isAir || snow) && SNOW.canSurvive(level, pos).
inline bool biomeShouldSnow(const SnowFreezeHooks& hooks, const BiomeClimate& climate,
                            WorldGenLevel& level, BlockPos pos) {
    using namespace snowfreeze_detail;
    if (!climate.hasPrecipitation) return false;                       // Precipitation.NONE
    if (warmEnoughToRain(climate, pos, hooks.seaLevel)) return false;  // RAIN, not SNOW
    if (pos.y >= hooks.levelMinY && pos.y < hooks.levelMaxY) {
        const std::string state = level.getBlockState(pos);
        if ((hooks.isAir(state) || state == "minecraft:snow") && hooks.snowCanSurvive(pos)) {
            return true;
        }
    }
    return false;
}

// SnowAndFreezeFeature.place (SnowAndFreezeFeature.java:20-49).
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeSnowAndFreezePlacer(
        std::shared_ptr<const SnowFreezeHooks> hooks) {
    return [hooks = std::move(hooks)](WorldGenLevel& level, RandomSource&, BlockPos origin) -> bool {
        for (int dx = 0; dx < 16; ++dx) {
            for (int dz = 0; dz < 16; ++dz) {
                const int x = origin.x + dx;
                const int z = origin.z + dz;
                const int y = level.getHeight(Heightmap::Types::MOTION_BLOCKING, x, z);
                const BlockPos topPos{ x, y, z };
                const BlockPos belowPos{ x, y - 1, z };
                const BiomeClimate& climate = hooks->climate(hooks->getBiome(topPos));
                if (biomeShouldFreeze(*hooks, climate, level, belowPos, false)) {
                    level.setBlock(belowPos, "minecraft:ice", 2);
                }
                if (biomeShouldSnow(*hooks, climate, level, topPos)) {
                    level.setBlock(topPos, "minecraft:snow", 2);
                    // belowState.setValue(SNOWY, true): same id; the write is kept 1:1.
                    const std::string belowState = level.getBlockState(belowPos);
                    if (hooks->hasSnowyProperty(belowState)) {
                        level.setBlock(belowPos, belowState, 2);
                    }
                }
            }
        }
        return true;
    };
}

} // namespace mc::levelgen::feature

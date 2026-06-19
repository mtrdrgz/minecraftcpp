#pragma once

// Pure temperature math from net.minecraft.world.level.biome.Biome (26.1.2).
//
// This header ports ONLY the registry-free temperature curve:
//   - Biome.TemperatureModifier.modifyTemperature (NONE and FROZEN), verbatim.
//   - Biome.getHeightAdjustedTemperature(pos, seaLevel), verbatim.
//   - Biome.getTemperature(pos, seaLevel) — the value-producing path. Java memoizes
//     the result in a per-thread Long2FloatLinkedOpenHashMap keyed by pos.asLong();
//     that cache is a pure memoization (it stores exactly the value
//     getHeightAdjustedTemperature returns and never alters it), so the math result
//     is identical whether or not the cache is present. We expose the uncached value.
//
// Everything else on Biome (codecs, special effects, generation/mob settings, the
// registry Holder lookups, shouldFreeze/shouldSnow which need a live LevelReader) is
// SKIPPED — see unportedMethods in the gate output.
//
// The three PerlinSimplexNoise statics are constructed EXACTLY as Biome.java does:
//   TEMPERATURE_NOISE        = new PerlinSimplexNoise(new WorldgenRandom(new LegacyRandomSource(1234L)), ImmutableList.of(0));
//   FROZEN_TEMPERATURE_NOISE = new PerlinSimplexNoise(new WorldgenRandom(new LegacyRandomSource(3456L)), ImmutableList.of(-2, -1, 0));
//   BIOME_INFO_NOISE         = new PerlinSimplexNoise(new WorldgenRandom(new LegacyRandomSource(2345L)), ImmutableList.of(0));
//
// PerlinSimplexNoise (mc::levelgen) is the certified engine port in
// world/level/levelgen/Noise.{h,cpp}; we reuse it (NOT re-port it).

#include "../levelgen/Noise.h"
#include "../levelgen/RandomSource.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace mc::biome {

// net.minecraft.world.level.biome.Biome.TemperatureModifier
enum class TemperatureModifier { NONE, FROZEN };

class BiomeTemperature {
public:
    // Build the three noise generators identically to Biome's static initializers.
    BiomeTemperature()
        : m_temperatureNoise(makeNoise(1234LL, {0})),
          m_frozenTemperatureNoise(makeNoise(3456LL, {-2, -1, 0})),
          m_biomeInfoNoise(makeNoise(2345LL, {0})) {}

    // Biome.TemperatureModifier.modifyTemperature(BlockPos, float)
    float modifyTemperature(TemperatureModifier modifier, int32_t x, int32_t z, float baseTemperature) const {
        switch (modifier) {
            case TemperatureModifier::NONE:
                return baseTemperature;
            case TemperatureModifier::FROZEN: {
                double groundValueLargeVariation =
                    m_frozenTemperatureNoise.getValue(x * 0.05, z * 0.05, false) * 7.0;
                double groundValueEdgeVariation =
                    m_biomeInfoNoise.getValue(x * 0.2, z * 0.2, false);
                double icePatches = groundValueLargeVariation + groundValueEdgeVariation;
                if (icePatches < 0.3) {
                    double groundValueSmallVariation =
                        m_biomeInfoNoise.getValue(x * 0.09, z * 0.09, false);
                    if (groundValueSmallVariation < 0.8) {
                        return 0.2F;
                    }
                }
                return baseTemperature;
            }
        }
        return baseTemperature;
    }

    // Biome.getHeightAdjustedTemperature(BlockPos, int)
    float getHeightAdjustedTemperature(TemperatureModifier modifier, float baseTemperature,
                                       int32_t x, int32_t y, int32_t z, int32_t seaLevel) const {
        float adjustedTemperature = modifyTemperature(modifier, x, z, baseTemperature);
        int32_t snowLevel = seaLevel + 17;
        if (y > snowLevel) {
            // (float)(TEMPERATURE_NOISE.getValue(x / 8.0F, z / 8.0F, false) * 8.0)
            float xf = static_cast<float>(x) / 8.0F;
            float zf = static_cast<float>(z) / 8.0F;
            float v = static_cast<float>(m_temperatureNoise.getValue(xf, zf, false) * 8.0);
            // adjustedTemperature - (v + y - snowLevel) * 0.05F / 40.0F
            return adjustedTemperature
                   - (v + static_cast<float>(y) - static_cast<float>(snowLevel)) * 0.05F / 40.0F;
        }
        return adjustedTemperature;
    }

    // Biome.getTemperature(BlockPos, int) — uncached value (see header note).
    float getTemperature(TemperatureModifier modifier, float baseTemperature,
                         int32_t x, int32_t y, int32_t z, int32_t seaLevel) const {
        return getHeightAdjustedTemperature(modifier, baseTemperature, x, y, z, seaLevel);
    }

    // Biome.warmEnoughToRain(BlockPos, int)
    bool warmEnoughToRain(TemperatureModifier modifier, float baseTemperature,
                          int32_t x, int32_t y, int32_t z, int32_t seaLevel) const {
        return getTemperature(modifier, baseTemperature, x, y, z, seaLevel) >= 0.15F;
    }

    // Biome.coldEnoughToSnow(BlockPos, int)
    bool coldEnoughToSnow(TemperatureModifier modifier, float baseTemperature,
                          int32_t x, int32_t y, int32_t z, int32_t seaLevel) const {
        return !warmEnoughToRain(modifier, baseTemperature, x, y, z, seaLevel);
    }

    // Biome.shouldMeltFrozenOceanIcebergSlightly(BlockPos, int)
    bool shouldMeltFrozenOceanIcebergSlightly(TemperatureModifier modifier, float baseTemperature,
                                              int32_t x, int32_t y, int32_t z, int32_t seaLevel) const {
        return getTemperature(modifier, baseTemperature, x, y, z, seaLevel) > 0.1F;
    }

private:
    static mc::levelgen::PerlinSimplexNoise makeNoise(int64_t seed, std::vector<int32_t> octaves) {
        mc::levelgen::WorldgenRandom rng(std::make_shared<mc::levelgen::LegacyRandomSource>(seed));
        return mc::levelgen::PerlinSimplexNoise(rng, std::move(octaves));
    }

    mc::levelgen::PerlinSimplexNoise m_temperatureNoise;
    mc::levelgen::PerlinSimplexNoise m_frozenTemperatureNoise;
    mc::levelgen::PerlinSimplexNoise m_biomeInfoNoise;
};

} // namespace mc::biome

#pragma once

// 1:1 port of the biome grass/foliage colour resolution:
//   net.minecraft.world.level.biome.Biome#getGrassColor / getFoliageColor /
//   getBaseGrassColor / *FromTexture (Biome.java:202-235)
//   net.minecraft.world.level.biome.BiomeSpecialEffects.GrassColorModifier#modifyColor
//   (BiomeSpecialEffects.java:74-98)
//
// Builds on the certified mc::level::colormap (ColorMapColorUtil/GrassColor/
// FoliageColor) and, for the SWAMP modifier, the certified PerlinSimplexNoise
// (Biome.BIOME_INFO_NOISE). Every constant comes from the decompiled source — no
// values are invented.
//
// This is the pure colour math. Wiring it into the mesher (loading the grass/foliage
// colormap pixels + per-position biome lookup + the client-side BiomeColors blend) is
// the remaining integration step (see TASKLIST "Texturas / coloreado").

#include "Biome.h"
#include "../ColorMapColorUtil.h"
#include "../levelgen/Noise.h"
#include "../levelgen/RandomSource.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace mc::biome::color {

// net.minecraft.util.ARGB.opaque(int): color | 0xFF000000.
inline std::int32_t opaque(std::int32_t color) {
    return color | static_cast<std::int32_t>(0xFF000000u);
}

// Biome.BIOME_INFO_NOISE = new PerlinSimplexNoise(
//     new WorldgenRandom(new LegacyRandomSource(2345L)), ImmutableList.of(0)).
inline const mc::levelgen::PerlinSimplexNoise& biomeInfoNoise() {
    static const std::unique_ptr<mc::levelgen::PerlinSimplexNoise> noise = [] {
        auto legacy = std::make_shared<mc::levelgen::LegacyRandomSource>(2345LL);
        mc::levelgen::WorldgenRandom random(legacy);
        return std::make_unique<mc::levelgen::PerlinSimplexNoise>(
            random, std::vector<std::int32_t>{0});
    }();
    return *noise;
}

// GrassColorModifier.modifyColor(x, z, baseColor).
inline std::int32_t modifyGrassColor(GrassColorModifier mod, double x, double z, std::int32_t baseColor) {
    switch (mod) {
        case GrassColorModifier::DARK_FOREST:
            // ARGB.opaque((baseColor & 16711422) + 2634762 >> 1)
            return opaque(((baseColor & 16711422) + 2634762) >> 1);
        case GrassColorModifier::SWAMP: {
            // groundValue < -0.1 ? -11766212 : -9801671
            const double groundValue = biomeInfoNoise().getValue(x * 0.0225, z * 0.0225, false);
            return groundValue < -0.1 ? -11766212 : -9801671;
        }
        case GrassColorModifier::NONE:
        default:
            return baseColor;
    }
}

// Biome.getGrassColorFromTexture: GrassColor.get(clamp(temp), clamp(downfall)).
inline std::int32_t grassColorFromTexture(const Biome& b, const std::vector<std::int32_t>& grassPixels) {
    const double temp = std::clamp(static_cast<double>(b.climate.temperature), 0.0, 1.0);
    const double rain = std::clamp(static_cast<double>(b.climate.downfall), 0.0, 1.0);
    return mc::level::colormap::grassGet(temp, rain, grassPixels);
}

// Biome.getBaseGrassColor: grassColorOverride.orElse(getGrassColorFromTexture()).
inline std::int32_t baseGrassColor(const Biome& b, const std::vector<std::int32_t>& grassPixels) {
    return b.effects.grassColor.has_value() ? static_cast<std::int32_t>(*b.effects.grassColor)
                                    : grassColorFromTexture(b, grassPixels);
}

// Biome.getGrassColor(x, z): grassColorModifier.modifyColor(x, z, getBaseGrassColor()).
inline std::int32_t grassColor(const Biome& b, double x, double z,
                               const std::vector<std::int32_t>& grassPixels) {
    return modifyGrassColor(b.effects.grassColorModifier, x, z, baseGrassColor(b, grassPixels));
}

// Biome.getFoliageColor: foliageColorOverride.orElse(getFoliageColorFromTexture()).
inline std::int32_t foliageColor(const Biome& b, const std::vector<std::int32_t>& foliagePixels) {
    if (b.effects.foliageColor.has_value()) return static_cast<std::int32_t>(*b.effects.foliageColor);
    const double temp = std::clamp(static_cast<double>(b.climate.temperature), 0.0, 1.0);
    const double rain = std::clamp(static_cast<double>(b.climate.downfall), 0.0, 1.0);
    return mc::level::colormap::foliageGet(temp, rain, foliagePixels);
}

}  // namespace mc::biome::color

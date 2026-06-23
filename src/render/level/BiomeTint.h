#pragma once

// Per-position biome tint for the chunk mesher — the block-colour layer vanilla
// applies on top of the grey grass/foliage/water textures.
//
// 1:1 port of:
//   net.minecraft.client.multiplayer.ClientLevel#calculateBlockTint  (the (2r+1)²
//       column box blend; default biomeBlendRadius = 2)
//   net.minecraft.client.renderer.BiomeColors  (the GRASS/FOLIAGE/WATER resolvers)
//   net.minecraft.client.color.block.BlockColors  (which textures get which resolver)
//
// The colour resolution itself is the certified mc::biome::color (BiomeColor.h) on
// the certified ColorMapColorUtil + the real grass.png/foliage.png colormaps.
//
// Decoupled from the world: the caller supplies `biomeAt(x,y,z) -> const Biome*`
// (in-game backed by the generator's biome source; in tests by a stub) and the
// decoded colormap pixel arrays. Returns std::nullopt for textures that are not
// biome-tinted (the mesher keeps its existing handling for those).

#include "../../world/level/biome/Biome.h"
#include "../../world/level/biome/BiomeColor.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace mc::render::biometint {

struct Rgb { std::uint8_t r, g, b; };

enum class TintClass { NONE, GRASS, FOLIAGE, WATER };

// net.minecraft.client.color.block.BlockColors registrations, expressed at the
// texture-sprite granularity the mesher works in. 1:1 with BlockColors.java:
//   grass(): FERN, SHORT_GRASS, POTTED_FERN, BUSH; doubleTallGrass(): TALL_GRASS,
//   LARGE_FERN; grassBlock(): GRASS_BLOCK; PINK_PETALS, WILDFLOWERS; sugarCane().
//   foliage(): OAK/JUNGLE/ACACIA/DARK_OAK/MANGROVE leaves + VINE.
//   waterParticles(): WATER. (SPRUCE/BIRCH leaves use a FIXED constant — NONE here,
//   the mesher applies it. CHERRY/AZALEA/PALE_OAK leaves are NOT tinted in vanilla.)
inline TintClass tintClassForTexture(const std::string& name) {
    if (name == "grass_block_top" || name == "grass_block_side_overlay" ||
        name == "short_grass" || name == "fern" || name == "potted_fern" ||
        name == "tall_grass_top" || name == "tall_grass_bottom" ||
        name == "large_fern_top" || name == "large_fern_bottom" ||
        name == "bush" || name == "sugar_cane" ||
        name == "pink_petals" || name == "wildflowers") {
        return TintClass::GRASS;
    }
    if (name == "water_still" || name == "water_flow") {
        return TintClass::WATER;
    }
    if (name == "oak_leaves" || name == "jungle_leaves" || name == "acacia_leaves" ||
        name == "dark_oak_leaves" || name == "mangrove_leaves" || name == "vine") {
        return TintClass::FOLIAGE;
    }
    return TintClass::NONE;
}

// ClientLevel.options.biomeBlendRadius default.
inline constexpr int DEFAULT_BLEND_RADIUS = 2;

using BiomeAt = std::function<const mc::biome::Biome*(int x, int y, int z)>;

// ColorResolver.getColor for a single column (BiomeColors resolvers).
inline std::int32_t resolveColumn(TintClass cls, const mc::biome::Biome& b, int x, int z,
                                  const std::vector<std::int32_t>& grass,
                                  const std::vector<std::int32_t>& foliage) {
    switch (cls) {
        case TintClass::GRASS:   return mc::biome::color::grassColor(b, x, z, grass);
        case TintClass::FOLIAGE: return mc::biome::color::foliageColor(b, foliage);
        case TintClass::WATER:   return static_cast<std::int32_t>(0xFF000000u | b.effects.waterColor);
        default:                 return static_cast<std::int32_t>(0xFFFFFFFFu);
    }
}

// ClientLevel.calculateBlockTint: average the resolver over the (2r+1)² column box
// at the block's Y; r==0 collapses to the single-column colour.
inline std::optional<Rgb> tint(const std::string& texture, const BiomeAt& biomeAt,
                               int wx, int wy, int wz,
                               const std::vector<std::int32_t>& grass,
                               const std::vector<std::int32_t>& foliage,
                               int radius = DEFAULT_BLEND_RADIUS) {
    if (!biomeAt) return std::nullopt;
    const TintClass cls = tintClassForTexture(texture);
    if (cls == TintClass::NONE) return std::nullopt;

    auto colourAt = [&](int x, int z) -> std::int32_t {
        const mc::biome::Biome* b = biomeAt(x, wy, z);
        if (!b) return static_cast<std::int32_t>(0xFFFFFFFFu);
        return resolveColumn(cls, *b, x, z, grass, foliage);
    };

    if (radius <= 0) {
        const std::int32_t c = colourAt(wx, wz);
        return Rgb{ std::uint8_t(c >> 16), std::uint8_t(c >> 8), std::uint8_t(c) };
    }

    const int count = (radius * 2 + 1) * (radius * 2 + 1);
    long totalR = 0, totalG = 0, totalB = 0;
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const std::int32_t c = colourAt(wx + dx, wz + dz);
            totalR += (c >> 16) & 0xFF;
            totalG += (c >> 8) & 0xFF;
            totalB += c & 0xFF;
        }
    }
    return Rgb{ std::uint8_t(totalR / count), std::uint8_t(totalG / count), std::uint8_t(totalB / count) };
}

}  // namespace mc::render::biometint

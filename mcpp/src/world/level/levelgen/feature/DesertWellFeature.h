#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.DesertWellFeature
// ("desert_well", rarity 1/1000 in SURFACE_STRUCTURES).
//
// RNG order (DesertWellFeature.java:28-107):
//   origin = origin.above(); descend while isEmptyBlock && y > minY+2 (no draws)
//   IS_SAND (block == minecraft:sand) fail -> false (no draws)
//   the 5x5 floor check + all structure writes: NO draws
//   finally TWO Util.getRandom(waterPositions, random) = nextInt(5) each placing
//   suspicious_sand (flags 3) 1 and 2 below a random water cell; the brushable
//   block entity's setLootTable(..., pos.asLong()) consumes NO randomness
//   (unlike chests' random.nextLong()).

#include "TreeFeature.h"   // WorldGenLevel, BlockPos

#include <array>
#include <functional>
#include <string>

namespace mc::levelgen::feature {

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeDesertWellPlacer(
        std::function<bool(const std::string&)> /*unused; kept for symmetry*/ = {}) {
    return [](WorldGenLevel& level, RandomSource& random, BlockPos originIn) -> bool {
        BlockPos origin{ originIn.x, originIn.y + 1, originIn.z };
        while (level.isEmptyBlock(origin) && origin.y > level.getMinY() + 2) {
            origin = BlockPos{ origin.x, origin.y - 1, origin.z };
        }
        if (level.getBlockState(origin) != "minecraft:sand") {   // IS_SAND BlockStatePredicate.forBlock
            return false;
        }
        for (int ox = -2; ox <= 2; ++ox) {
            for (int oz = -2; oz <= 2; ++oz) {
                if (level.isEmptyBlock(BlockPos{ origin.x + ox, origin.y - 1, origin.z + oz })
                    && level.isEmptyBlock(BlockPos{ origin.x + ox, origin.y - 2, origin.z + oz })) {
                    return false;
                }
            }
        }
        for (int oy = -2; oy <= 0; ++oy) {
            for (int ox = -2; ox <= 2; ++ox) {
                for (int oz = -2; oz <= 2; ++oz) {
                    level.setBlock(BlockPos{ origin.x + ox, origin.y + oy, origin.z + oz }, "minecraft:sandstone", 2);
                }
            }
        }
        level.setBlock(origin, "minecraft:water", 2);
        // Direction.Plane.HORIZONTAL = {NORTH, EAST, SOUTH, WEST} (Direction.java:577).
        static constexpr int HORIZONTAL[4] = { 2, 5, 3, 4 };   // NORTH, EAST, SOUTH, WEST
        for (int d : HORIZONTAL) {
            level.setBlock(treeRelative(origin, d), "minecraft:water", 2);
        }
        const BlockPos sandCenter{ origin.x, origin.y - 1, origin.z };
        level.setBlock(sandCenter, "minecraft:sand", 2);
        for (int d : HORIZONTAL) {
            level.setBlock(treeRelative(sandCenter, d), "minecraft:sand", 2);
        }
        for (int ox = -2; ox <= 2; ++ox) {
            for (int oz = -2; oz <= 2; ++oz) {
                if (ox == -2 || ox == 2 || oz == -2 || oz == 2) {
                    level.setBlock(BlockPos{ origin.x + ox, origin.y + 1, origin.z + oz }, "minecraft:sandstone", 2);
                }
            }
        }
        level.setBlock(BlockPos{ origin.x + 2, origin.y + 1, origin.z }, "minecraft:sandstone_slab", 2);
        level.setBlock(BlockPos{ origin.x - 2, origin.y + 1, origin.z }, "minecraft:sandstone_slab", 2);
        level.setBlock(BlockPos{ origin.x, origin.y + 1, origin.z + 2 }, "minecraft:sandstone_slab", 2);
        level.setBlock(BlockPos{ origin.x, origin.y + 1, origin.z - 2 }, "minecraft:sandstone_slab", 2);
        for (int ox = -1; ox <= 1; ++ox) {
            for (int oz = -1; oz <= 1; ++oz) {
                if (ox == 0 && oz == 0) {
                    level.setBlock(BlockPos{ origin.x + ox, origin.y + 4, origin.z + oz }, "minecraft:sandstone", 2);
                } else {
                    level.setBlock(BlockPos{ origin.x + ox, origin.y + 4, origin.z + oz }, "minecraft:sandstone_slab", 2);
                }
            }
        }
        for (int oy = 1; oy <= 3; ++oy) {
            level.setBlock(BlockPos{ origin.x - 1, origin.y + oy, origin.z - 1 }, "minecraft:sandstone", 2);
            level.setBlock(BlockPos{ origin.x - 1, origin.y + oy, origin.z + 1 }, "minecraft:sandstone", 2);
            level.setBlock(BlockPos{ origin.x + 1, origin.y + oy, origin.z - 1 }, "minecraft:sandstone", 2);
            level.setBlock(BlockPos{ origin.x + 1, origin.y + oy, origin.z + 1 }, "minecraft:sandstone", 2);
        }
        // List.of(center, east, south, west, north) — DesertWellFeature.java:101-105.
        const std::array<BlockPos, 5> waterPositions{
            origin,
            BlockPos{ origin.x + 1, origin.y, origin.z },   // east
            BlockPos{ origin.x, origin.y, origin.z + 1 },   // south
            BlockPos{ origin.x - 1, origin.y, origin.z },   // west
            BlockPos{ origin.x, origin.y, origin.z - 1 },   // north
        };
        const BlockPos sus1 = waterPositions[static_cast<std::size_t>(random.nextInt(5))];
        level.setBlock(BlockPos{ sus1.x, sus1.y - 1, sus1.z }, "minecraft:suspicious_sand", 3);
        const BlockPos sus2 = waterPositions[static_cast<std::size_t>(random.nextInt(5))];
        level.setBlock(BlockPos{ sus2.x, sus2.y - 2, sus2.z }, "minecraft:suspicious_sand", 3);
        return true;
    };
}

} // namespace mc::levelgen::feature

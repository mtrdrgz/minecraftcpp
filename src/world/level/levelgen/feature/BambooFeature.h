#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.BambooFeature (26.1.2).
//
// RNG order (BambooFeature.place, :31-76):
//   isEmptyBlock(origin) gate (no draw; on failure NO draws and placed stays 0)
//   BAMBOO.defaultBlockState().canSurvive (BambooStalkBlock.java:130-132:
//     below.is(#supports_bamboo)) — no draw
//   height = nextInt(12) + 5
//   nextFloat < probability -> r = nextInt(4) + 1, podzol disk over
//     WORLD_SURFACE heightmap tops (#beneath_bamboo_podzol_replaceable -> podzol)
//   trunk column while isEmptyBlock, then the 3 leaf-topped stalks when the
//   column reached >= 3 (all writes flag 2; bamboo LEAVES/AGE/STAGE properties
//   are id-invisible)
// placed++ fires whenever the ORIGIN was empty (even if canSurvive failed), so
// the feature returns true in that case (BambooFeature.java:71-74).

#include "../placement/PlacementContext.h"
#include "../placement/PlacedFeature.h"
#include "../RandomSource.h"
#include "../Heightmap.h"
#include "../../../../core/Math.h"

#include <functional>
#include <string>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

struct BambooHooks {
    std::function<bool(const std::string&)> beneathBambooPodzolReplaceable; // #beneath_bamboo_podzol_replaceable
};

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeBambooPlacer(
        double probability, BambooHooks hooks) {
    return [probability, hooks = std::move(hooks)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        int placed = 0;
        BlockPos bambooPos = origin;
        if (level.isEmptyBlock(bambooPos)) {
            if (level.canSurvive("minecraft:bamboo", bambooPos)) {   // BambooStalkBlock.canSurvive
                const int height = random.nextInt(12) + 5;
                if (random.nextFloat() < static_cast<float>(probability)) {
                    const int r = random.nextInt(4) + 1;
                    for (int xx = origin.x - r; xx <= origin.x + r; ++xx) {
                        for (int zz = origin.z - r; zz <= origin.z + r; ++zz) {
                            const int xd = xx - origin.x, zd = zz - origin.z;
                            if (xd * xd + zd * zd <= r * r) {
                                // getHeight == stored heightmap + 1 (WorldGenRegion.java:391-393);
                                // -1 lands ON the surface block.
                                const BlockPos podzolPos{ xx, level.getHeight(Heightmap::Types::WORLD_SURFACE, xx, zz) - 1, zz };
                                if (hooks.beneathBambooPodzolReplaceable(level.getBlockState(podzolPos))) {
                                    level.setBlock(podzolPos, "minecraft:podzol", 2);
                                }
                            }
                        }
                    }
                }
                for (int i = 0; i < height && level.isEmptyBlock(bambooPos); ++i) {
                    level.setBlock(bambooPos, "minecraft:bamboo", 2);   // BAMBOO_TRUNK
                    bambooPos = BlockPos{ bambooPos.x, bambooPos.y + 1, bambooPos.z };
                }
                if (bambooPos.y - origin.y >= 3) {
                    // BAMBOO_FINAL_LARGE / TOP_LARGE / TOP_SMALL: same id.
                    level.setBlock(bambooPos, "minecraft:bamboo", 2);
                    bambooPos = BlockPos{ bambooPos.x, bambooPos.y - 1, bambooPos.z };
                    level.setBlock(bambooPos, "minecraft:bamboo", 2);
                    bambooPos = BlockPos{ bambooPos.x, bambooPos.y - 1, bambooPos.z };
                    level.setBlock(bambooPos, "minecraft:bamboo", 2);
                }
            }
            ++placed;
        }
        return placed > 0;
    };
}

} // namespace mc::levelgen::feature

#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.SpringFeature with
// SpringConfiguration. Consumes NO RNG.
//
// Order of checks (SpringFeature.java:15-82):
//   1. blockAbove  must be in validBlocks                         (:19-21)
//   2. requiresBlockBelow -> blockBelow must be in validBlocks    (:23-25)
//   3. current block must be air or in validBlocks                (:27-30)
//   4. rockCount = #(west,east,north,south,below in validBlocks)  (:33-52)
//   5. holeCount = #(west,east,north,south,below isEmptyBlock)    (:54-73)
//   6. both equal the configured counts -> setBlock(origin,
//      config.state.createLegacyBlock(), 2) + scheduleTick (a worldgen no-op:
//      the WorldGenRegion/harness proxy drops scheduleTick)        (:75-79)
//
// config.state is a FLUID state ("minecraft:water"/"minecraft:lava" [falling]);
// createLegacyBlock -> Blocks.WATER/LAVA with LEVEL = getLegacyLevel(state) = 0
// for any SOURCE fluid (FlowingFluid.getLegacyLevel: isSource -> 0, regardless of
// FALLING) — i.e. the plain source liquid block. The caller passes that block id.
//
// isEmptyBlock == BlockState.isAir(). The chunk pipeline feeding this port never
// stores cave_air/void_air (the aquifer carves to minecraft:air; the certified
// ground truth contains zero cave_air cells), so the plain-air compare is exact.
//
// Direction offsets (Direction.java:33-38): west=(-1,0,0) east=(1,0,0)
// north=(0,0,-1) south=(0,0,1) below=(0,-1,0).

#include "../placement/PlacementContext.h"   // WorldGenLevel
#include "../placement/PlacedFeature.h"      // FeaturePlacer
#include "../RandomSource.h"
#include "../../../../core/Math.h"           // BlockPos

#include <functional>
#include <set>
#include <string>
#include <utility>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeSpringPlacer(
        std::string legacyBlock,             // config.state.createLegacyBlock() block id
        bool requiresBlockBelow, int rockCount, int holeCount,
        std::set<std::string> validBlocks) {
    return [legacyBlock = std::move(legacyBlock), requiresBlockBelow, rockCount, holeCount,
            validBlocks = std::move(validBlocks)](
               WorldGenLevel& level, RandomSource&, BlockPos origin) -> bool {
        const auto isValid = [&](BlockPos p) {
            return validBlocks.count(level.getBlockState(p)) != 0;
        };
        const BlockPos above{ origin.x, origin.y + 1, origin.z };
        const BlockPos below{ origin.x, origin.y - 1, origin.z };
        const BlockPos west{ origin.x - 1, origin.y, origin.z };
        const BlockPos east{ origin.x + 1, origin.y, origin.z };
        const BlockPos north{ origin.x, origin.y, origin.z - 1 };
        const BlockPos south{ origin.x, origin.y, origin.z + 1 };

        if (!isValid(above)) return false;                            // :19-21
        if (requiresBlockBelow && !isValid(below)) return false;      // :23-25
        const std::string current = level.getBlockState(origin);
        if (current != "minecraft:air" && validBlocks.count(current) == 0) return false;  // :27-30

        int rocks = 0;                                                // :33-52
        if (isValid(west)) ++rocks;
        if (isValid(east)) ++rocks;
        if (isValid(north)) ++rocks;
        if (isValid(south)) ++rocks;
        if (isValid(below)) ++rocks;

        int holes = 0;                                                // :54-73
        if (level.isEmptyBlock(west)) ++holes;
        if (level.isEmptyBlock(east)) ++holes;
        if (level.isEmptyBlock(north)) ++holes;
        if (level.isEmptyBlock(south)) ++holes;
        if (level.isEmptyBlock(below)) ++holes;

        if (rocks == rockCount && holes == holeCount) {               // :75-79
            level.setBlock(origin, legacyBlock, 2);
            // level.scheduleTick(origin, fluid, 0): worldgen no-op (proxy drops it).
            return true;
        }
        return false;
    };
}

} // namespace mc::levelgen::feature

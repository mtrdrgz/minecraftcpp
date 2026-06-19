#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.SimpleBlockFeature
// (SimpleBlockFeature.java:17-48) for the worldgen-reachable configs
// (grass/bush/flowers/double plants, pale_moss_vegetation's pale_moss_carpet;
// schedule_tick only on flower_pale_garden — counted hard no-op tick).
//
// RNG: toPlace.getOptionalState (simple providers: no draw; weighted providers one
// draw; the canSurvive gate and the DoublePlantBlock branch consume none either —
// DoublePlantBlock.placeAt is two plain setBlocks, DoublePlantBlock.java:86-90).
// The MossyCarpetBlock branch (SimpleBlockFeature.java:33-35) calls
// MossyCarpetBlock.placeAt(level, origin, level.getRandom(), 2) — the topper's
// nextBoolean draws come from the WorldGenRegion REGION random (WorldGenRegion.java:
// 86,386-388), NOT the feature random; supplied by the harness hook.

#include "../placement/PlacementContext.h"   // WorldGenLevel
#include "../placement/PlacedFeature.h"      // FeaturePlacer
#include "../RandomSource.h"
#include "DiskFeature.h"                     // DiskStateProvider

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace mc::levelgen::feature {

// `isDoublePlant`: state.getBlock() instanceof DoublePlantBlock — supplied by the
// harness (BlockBehaviour.h doublePlantBlocks table).
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeSimpleBlockPlacer(
        DiskStateProvider toPlace, std::function<bool(const std::string&)> isDoublePlant,
        std::function<bool(const std::string&)> isAir,
        std::function<bool(const std::string&)> isMossyCarpet = {},
        std::function<void(WorldGenLevel&, BlockPos)> mossyCarpetPlaceAt = {}) {
    return [toPlace = std::move(toPlace), isDoublePlant = std::move(isDoublePlant),
            isAir = std::move(isAir), isMossyCarpet = std::move(isMossyCarpet),
            mossyCarpetPlaceAt = std::move(mossyCarpetPlaceAt)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        const std::optional<std::string> state = toPlace(level, random, origin);
        if (!state.has_value()) {
            return false;
        }
        if (level.canSurvive(*state, origin)) {
            if (isMossyCarpet && isMossyCarpet(*state)) {
                // MossyCarpetBlock branch (SimpleBlockFeature.java:33-35):
                // MossyCarpetBlock.placeAt(level, origin, level.getRandom(), 2).
                mossyCarpetPlaceAt(level, origin);
            } else if (isDoublePlant(*state)) {
                // DoublePlantBlock branch (SimpleBlockFeature.java:30-36):
                // require an empty block above, then placeAt (lower, then upper; the
                // HALF/waterlogged properties are id-invisible).
                const BlockPos above{ origin.x, origin.y + 1, origin.z };
                if (!isAir(level.getBlockState(above))) {
                    return false;
                }
                level.setBlock(origin, *state, 2);
                level.setBlock(above, *state, 2);
            } else {
                level.setBlock(origin, *state, 2);
            }
            return true;
        }
        return false;
    };
}

} // namespace mc::levelgen::feature

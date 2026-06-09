#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.SimpleBlockFeature
// (SimpleBlockFeature.java:18-50) for the worldgen-reachable configs
// (grass/bush/flowers/double plants; schedule_tick is absent/false on all of them,
// and MossyCarpetBlock never occurs — fail closed in the loader).
//
// RNG: toPlace.getOptionalState (simple providers: no draw; the canSurvive gate and
// the DoublePlantBlock branch consume none either — DoublePlantBlock.placeAt is two
// plain setBlocks, DoublePlantBlock.java:86-90).

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
        std::function<bool(const std::string&)> isAir) {
    return [toPlace = std::move(toPlace), isDoublePlant = std::move(isDoublePlant),
            isAir = std::move(isAir)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        const std::optional<std::string> state = toPlace(level, random, origin);
        if (!state.has_value()) {
            return false;
        }
        if (level.canSurvive(*state, origin)) {
            if (isDoublePlant(*state)) {
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

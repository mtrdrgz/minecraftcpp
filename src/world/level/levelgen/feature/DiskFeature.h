#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.DiskFeature (+ the
// markAboveForPostProcessing helper it inherits from Feature) with DiskConfiguration.
//
// RNG order (must match Java exactly):
//   r = config.radius().sample(random)                      (DiskFeature.java:25)
//   column iteration: BlockPos.betweenClosed(origin±r on x/z) — X varies fastest,
//   then Y (constant), then Z (BlockPos.java betweenClosed computeNext:
//   x = index % width, z = index / (width*height))           (DiskFeature.java:28)
//   per accepted column cell the state provider runs; the vanilla disk providers
//   (simple_state_provider, rule_based_state_provider) consume NO RNG.
//
// placeColumn (DiskFeature.java:39-64): scan y from top = originY+halfHeight down
// to bottom = originY-halfHeight-1 EXCLUSIVE; where config.target().test passes and
// the provider yields a state, setBlock(pos, state, 2) and — for the FIRST placed
// block of each contiguous placed run (placedAbove false) — markAboveForPostProcessing.
//
// markAboveForPostProcessing (Feature.java:206-217): mark pos+1 and pos+2 on the
// chunk containing them, stopping before the first air block.
//
// The target BlockPredicate and the BlockStateProvider are supplied as resolved
// functors so this header stays decoupled from the JSON/tag subsystems.

#include "../placement/PlacementContext.h"   // WorldGenLevel
#include "../placement/PlacedFeature.h"      // FeaturePlacer
#include "../RandomSource.h"
#include "../IntProvider.h"
#include "../../../../core/Math.h"           // BlockPos

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

// BlockPredicate.test(level, pos) resolved to a functor.
using DiskBlockPredicate = std::function<bool(WorldGenLevel&, BlockPos)>;
// BlockStateProvider.getOptionalState(level, random, pos): nullopt == null.
using DiskStateProvider =
    std::function<std::optional<std::string>(WorldGenLevel&, RandomSource&, BlockPos)>;

// Feature.markAboveForPostProcessing (Feature.java:206-217).
inline void diskMarkAboveForPostProcessing(WorldGenLevel& level, BlockPos placePos) {
    BlockPos pos = placePos;
    for (int i = 0; i < 2; ++i) {
        pos = BlockPos{ pos.x, pos.y + 1, pos.z };
        if (level.getBlockState(pos) == "minecraft:air") {
            return;
        }
        level.markPosForPostprocessing(pos);
    }
}

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeDiskPlacer(
        DiskStateProvider stateProvider, DiskBlockPredicate target,
        mc::valueproviders::IntProviderPtr radius, int halfHeight) {
    return [stateProvider = std::move(stateProvider), target = std::move(target),
            radius = std::move(radius), halfHeight](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        bool placedAny = false;
        const int originY = origin.y;
        const int top = originY + halfHeight;                       // DiskFeature.java:23
        const int bottom = originY - halfHeight - 1;                // DiskFeature.java:24
        const int r = radius->sample(random);                       // DiskFeature.java:25

        // BlockPos.betweenClosed(origin.offset(-r,0,-r), origin.offset(r,0,r)):
        // X fastest, Z outer (Y is a single value).
        for (int z = origin.z - r; z <= origin.z + r; ++z) {
            for (int x = origin.x - r; x <= origin.x + r; ++x) {
                const int xd = x - origin.x;
                const int zd = z - origin.z;
                if (xd * xd + zd * zd <= r * r) {                   // DiskFeature.java:31
                    // placeColumn (DiskFeature.java:39-64)
                    bool placedAbove = false;
                    for (int y = top; y > bottom; --y) {
                        const BlockPos pos{ x, y, z };
                        if (target(level, pos)) {
                            std::optional<std::string> state = stateProvider(level, random, pos);
                            if (state.has_value()) {
                                level.setBlock(pos, *state, 2);
                                if (!placedAbove) {
                                    diskMarkAboveForPostProcessing(level, pos);
                                }
                                placedAny = true;
                                placedAbove = true;
                            }
                        } else {
                            placedAbove = false;
                        }
                    }
                }
            }
        }
        return placedAny;
    };
}

} // namespace mc::levelgen::feature

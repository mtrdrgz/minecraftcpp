#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.KelpFeature
// (KelpFeature.java:18-53, NoneFeatureConfiguration).
//
// RNG order (must match Java exactly):
//   y = level.getHeight(OCEAN_FLOOR, ox, oz)   (no draw; frozen non-WG map +1,
//                                               see SeagrassFeature.h)
//   if block at kelpPos is(WATER):
//       height = 1 + nextInt(10)               (one draw)
//       for h in 0..=height:
//           if is(WATER, kelpPos) && is(WATER, above) && KELP_PLANT.canSurvive(kelpPos):
//               h == height: setBlock(kelpPos, KELP[age=nextInt(4)+20])  (one draw) ; placed++
//               else:        setBlock(kelpPos, KELP_PLANT)
//           else if h > 0:
//               below = kelpPos.below()
//               if KELP.canSurvive(below) && !is(KELP, below.below()):
//                   setBlock(below, KELP[age=nextInt(4)+20])             (one draw) ; placed++
//               break
//           kelpPos = kelpPos.above()
//
// The nextInt(4) age draw happens exactly when a kelp HEAD is written, and must
// be consumed even though the parity comparison only records block ids.
//
// canSurvive for kelp/kelp_plant (GrowingPlantBlock.canSurvive,
// GrowingPlantBlock.java:47-55, with KelpBlock.canAttachTo KelpBlock.java:46-48
// and KelpPlantBlock.canAttachTo KelpPlantBlock.java:40-42):
//   attached = pos.below()
//   !attached.is(CANNOT_SUPPORT_KELP)
//     && (attached.is(KELP) || attached.is(KELP_PLANT)
//         || attached.isFaceSturdy(level, attachedPos, UP))
// delegated to the WorldGenLevel boundary.

#include "SeagrassFeature.h"                 // stateIsBlock (BlockState.is(Block))
#include "../placement/PlacementContext.h"   // WorldGenLevel
#include "../placement/PlacedFeature.h"      // FeaturePlacer
#include "../RandomSource.h"
#include "../Heightmap.h"
#include "../../../../core/Math.h"           // BlockPos

#include <string>

namespace mc::levelgen::feature {

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeKelpPlacer() {
    return [](WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        int placed = 0;
        const int y = level.getHeight(Heightmap::Types::OCEAN_FLOOR, origin.x, origin.z);
        BlockPos kelpPos{ origin.x, y, origin.z };
        if (stateIsBlock(level, kelpPos, "minecraft:water")) {
            const int height = 1 + random.nextInt(10);
            for (int h = 0; h <= height; ++h) {
                const BlockPos above{ kelpPos.x, kelpPos.y + 1, kelpPos.z };
                if (stateIsBlock(level, kelpPos, "minecraft:water")
                    && stateIsBlock(level, above, "minecraft:water")
                    && level.canSurvive("minecraft:kelp_plant", kelpPos)) {
                    if (h == height) {
                        // stateTop.setValue(KelpBlock.AGE, random.nextInt(4) + 20)
                        const int age = random.nextInt(4) + 20;
                        level.setBlock(kelpPos, "minecraft:kelp[age=" + std::to_string(age) + "]", 2);
                        ++placed;
                    } else {
                        level.setBlock(kelpPos, "minecraft:kelp_plant", 2);
                    }
                } else if (h > 0) {
                    const BlockPos below{ kelpPos.x, kelpPos.y - 1, kelpPos.z };
                    const BlockPos belowBelow{ kelpPos.x, kelpPos.y - 2, kelpPos.z };
                    if (level.canSurvive("minecraft:kelp", below)
                        && !stateIsBlock(level, belowBelow, "minecraft:kelp")) {
                        const int age = random.nextInt(4) + 20;
                        level.setBlock(below, "minecraft:kelp[age=" + std::to_string(age) + "]", 2);
                        ++placed;
                    }
                    break;
                }
                kelpPos = above;
            }
        }
        return placed > 0;
    };
}

} // namespace mc::levelgen::feature

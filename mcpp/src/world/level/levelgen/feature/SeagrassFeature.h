#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.SeagrassFeature
// (SeagrassFeature.java:19-50) with ProbabilityFeatureConfiguration.
//
// RNG order (must match Java exactly):
//   x = nextInt(8) - nextInt(8)            (two draws)
//   z = nextInt(8) - nextInt(8)            (two draws)
//   y = level.getHeight(OCEAN_FLOOR, ox+x, oz+z)   -- WorldGenRegion semantics:
//       chunk heightmap value + 1, and the non-WG OCEAN_FLOOR map is primed at
//       the FEATURES step start and frozen (ProtoChunk at CARVERS status only
//       live-updates the *_WG pair).
//   if block at grassPos is(WATER):
//       isTall = nextDouble() < probability      (one draw)
//       state  = isTall ? TALL_SEAGRASS (default = half=lower) : SEAGRASS
//       if state.canSurvive(level, grassPos):
//           tall: place lower+upper ONLY if block above is(WATER)
//           short: place
//           placedAny = true  (set whenever canSurvive passes, even if the tall
//                              above-water check rejected the actual writes --
//                              Java sets it outside that inner if)
//
// canSurvive is delegated to the WorldGenLevel boundary (the level implements
// SeagrassBlock/TallSeagrassBlock survival: mayPlaceOn = isFaceSturdy(UP) &&
// !CANNOT_SUPPORT_SEAGRASS; tall lower additionally fluidState.is(WATER) &&
// isFull -- SeagrassBlock.java:46-48, TallSeagrassBlock.java:45-47,67-76,
// VegetationBlock.java:44-47, DoublePlantBlock.java:77-84).

#include "../placement/PlacementContext.h"   // WorldGenLevel
#include "../placement/PlacedFeature.h"      // FeaturePlacer
#include "../RandomSource.h"
#include "../Heightmap.h"
#include "../../block/BlockStates.h"         // blockName (BlockState.is(Block) = block identity)
#include "../../../../core/Math.h"           // BlockPos

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::Heightmap;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

// BlockState.is(Block): block identity, independent of state properties.
inline bool stateIsBlock(const WorldGenLevel& level, BlockPos pos, const char* block) {
    return mc::block::blockName(level.getBlockState(pos)) == block;
}

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeSeagrassPlacer(double probability) {
    return [probability](WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        bool placedAny = false;
        const int x = random.nextInt(8) - random.nextInt(8);
        const int z = random.nextInt(8) - random.nextInt(8);
        const int y = level.getHeight(Heightmap::Types::OCEAN_FLOOR, origin.x + x, origin.z + z);
        const BlockPos grassPos{ origin.x + x, y, origin.z + z };
        if (stateIsBlock(level, grassPos, "minecraft:water")) {
            const bool isTall = random.nextDouble() < probability;
            const char* state = isTall ? "minecraft:tall_seagrass" : "minecraft:seagrass";
            if (level.canSurvive(state, grassPos)) {
                if (isTall) {
                    const BlockPos above{ grassPos.x, grassPos.y + 1, grassPos.z };
                    if (stateIsBlock(level, above, "minecraft:water")) {
                        level.setBlock(grassPos, "minecraft:tall_seagrass", 2);   // half=lower (default)
                        level.setBlock(above, "minecraft:tall_seagrass[half=upper]", 2);
                    }
                } else {
                    level.setBlock(grassPos, state, 2);
                }
                placedAny = true;
            }
        }
        return placedAny;
    };
}

} // namespace mc::levelgen::feature

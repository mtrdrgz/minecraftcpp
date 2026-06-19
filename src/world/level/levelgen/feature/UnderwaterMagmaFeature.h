#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.UnderwaterMagmaFeature
// with UnderwaterMagmaConfiguration, plus the Column.scan helper it uses.
//
// place (UnderwaterMagmaFeature.java:27-50):
//   floorY = getFloorY(level, origin, config): Column.scan(level, origin,
//     floorSearchRange, inside = is WATER, edge = !is WATER).map(Column::getFloor)
//     — empty unless origin itself is water AND the downward scan ends on a
//     non-water edge within range (Column.java:scan/scanDirection).
//   floorPos = origin.atY(floorY); bounds = floorPos ± placementRadiusAroundFloor.
//   BlockPos.betweenClosedStream(bounds)              — X fastest, then Y, then Z
//     .filter(pos -> random.nextFloat() < placementProbabilityPerValidPosition)
//          — ONE nextFloat PER BOX CELL, consumed before any validity check
//     .filter(isValidPlacement)
//     .map: setBlock(pos, MAGMA_BLOCK, 2)
//   placed > 0.
//
// Column.scanDirection (Column.java): from y=origin.y, `for (i = 1; i < range &&
// inside(pos); i++) move(dir)` then the edge predicate is tested at the final pos
// (so at most range-1 moves; an exhausted scan still ending on water yields empty).
//
// isValidPlacement (UnderwaterMagmaFeature.java:59-71):
//   !isWaterOrAir(state at pos) && !isVisibleFromOutside(pos.below(), UP)
//   && no horizontal neighbour (NORTH, EAST, SOUTH, WEST — Direction.Plane.
//   HORIZONTAL order, Direction.java:576) isVisibleFromOutside(pos+dir,
//   dir.getOpposite()).
// isVisibleFromOutside (:77-81): faceOcclusionShape(coveredDirection) == empty ||
//   !isShapeFullBlock — i.e. the neighbour block is NOT a full occluding cube;
//   modelled by mc::block::isFaceOccludingFullBlock (see BlockBehaviour.h for the
//   per-block grounding).
//
// The magma_block writes go through level.setBlock(pos, state, 2); the level's
// WorldGenRegion.setBlock mirror is responsible for the post-process marking of
// the block above (Blocks.java:4398 .postProcess(Blocks::postProcessAbove)).

#include "../placement/PlacementContext.h"   // WorldGenLevel
#include "../placement/PlacedFeature.h"      // FeaturePlacer
#include "../RandomSource.h"
#include "../../block/BlockBehaviour.h"      // isFaceOccludingFullBlock
#include "../../../../core/Math.h"           // BlockPos

#include <functional>
#include <optional>
#include <set>
#include <string>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

// Tracks blocks whose occlusion shape had to be defaulted (must stay empty; the
// caller surfaces it like blocksMotion's defaulted set).
inline std::set<std::string>& underwaterMagmaOcclusionDefaulted() {
    static std::set<std::string> s;
    return s;
}

namespace detail {

inline bool umIsWater(WorldGenLevel& level, BlockPos p) {
    return level.getBlockState(p) == "minecraft:water";
}

// Column.scanDirection (Column.java); dirY = +1 (UP) / -1 (DOWN).
inline std::optional<int> umScanDirection(WorldGenLevel& level, int searchRange,
                                          BlockPos origin, int dirY) {
    BlockPos pos = origin;
    for (int i = 1; i < searchRange && umIsWater(level, pos); ++i) {
        pos = BlockPos{ pos.x, pos.y + dirY, pos.z };
    }
    return !umIsWater(level, pos) ? std::optional<int>(pos.y) : std::nullopt;
}

// UnderwaterMagmaFeature.isVisibleFromOutside (:77-81).
inline bool umIsVisibleFromOutside(WorldGenLevel& level, BlockPos pos) {
    bool defaulted = false;
    const bool occludes = mc::block::isFaceOccludingFullBlock(level.getBlockState(pos), &defaulted);
    if (defaulted) underwaterMagmaOcclusionDefaulted().insert(level.getBlockState(pos));
    return !occludes;
}

inline bool umIsWaterOrAir(const std::string& state) {
    return state == "minecraft:water" || state == "minecraft:air";
}

// UnderwaterMagmaFeature.isValidPlacement (:59-71).
inline bool umIsValidPlacement(WorldGenLevel& level, BlockPos pos) {
    if (umIsWaterOrAir(level.getBlockState(pos))) return false;
    if (umIsVisibleFromOutside(level, BlockPos{ pos.x, pos.y - 1, pos.z })) return false;
    // Direction.Plane.HORIZONTAL = {NORTH, EAST, SOUTH, WEST} (Direction.java:576).
    static const int horiz[4][2] = { {0,-1}, {1,0}, {0,1}, {-1,0} };
    for (const auto& d : horiz) {
        if (umIsVisibleFromOutside(level, BlockPos{ pos.x + d[0], pos.y, pos.z + d[1] })) {
            return false;
        }
    }
    return true;
}

} // namespace detail

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeUnderwaterMagmaPlacer(
        int floorSearchRange, int placementRadiusAroundFloor,
        float placementProbabilityPerValidPosition) {
    return [floorSearchRange, placementRadiusAroundFloor, placementProbabilityPerValidPosition](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        // getFloorY (UnderwaterMagmaFeature.java:52-57) via Column.scan.
        if (!detail::umIsWater(level, origin)) return false;   // Column.scan early-out
        // The UP scan runs first in Column.scan; its result (ceiling) is unused by
        // getFloor but the scan itself is side-effect-free, so order is moot — kept
        // for fidelity of reads.
        (void)detail::umScanDirection(level, floorSearchRange, origin, +1);
        const std::optional<int> floor = detail::umScanDirection(level, floorSearchRange, origin, -1);
        if (!floor.has_value()) return false;

        const BlockPos floorPos{ origin.x, *floor, origin.z };
        const int r = placementRadiusAroundFloor;
        // BoundingBox.fromCorners normalises; betweenClosedStream iterates X fastest,
        // then Y, then Z (BlockPos.java betweenClosed computeNext).
        int placed = 0;
        for (int z = floorPos.z - r; z <= floorPos.z + r; ++z) {
            for (int y = floorPos.y - r; y <= floorPos.y + r; ++y) {
                for (int x = floorPos.x - r; x <= floorPos.x + r; ++x) {
                    // nextFloat consumed for EVERY cell, before validity.
                    if (random.nextFloat() < placementProbabilityPerValidPosition) {
                        const BlockPos pos{ x, y, z };
                        if (detail::umIsValidPlacement(level, pos)) {
                            level.setBlock(pos, "minecraft:magma_block", 2);
                            ++placed;
                        }
                    }
                }
            }
        }
        return placed > 0;
    };
}

} // namespace mc::levelgen::feature

#pragma once

// 1:1 port of the PURE, RNG-driven corridor-sizing geometry in
//   net.minecraft.world.level.levelgen.structure.structures
//     .MineshaftPieces.MineShaftCorridor.findCorridorSize(...)   [MineshaftPieces.java:144-168]
//
// findCorridorSize is the only self-contained piece of MineShaftCorridor
// geometry: given a foot position (footX,footY,footZ) and an orientation
// Direction, it draws ONE RandomSource.nextInt(3) (yielding corridorLength in
// [2,4]), then for decreasing corridorLength builds a candidate BoundingBox
// (blockLength = corridorLength*5; box spans blockLength-1 along the corridor
// axis and 2 across), moves it to the foot, and returns the first box that does
// not collide with an already-placed piece (StructurePieceAccessor
// .findCollisionPiece == null). If every candidate collides it returns null.
//
// This header ports EXACTLY that logic. The StructurePieceAccessor collision
// query is abstracted as a caller-supplied predicate `collides(box) -> bool`
// (true == there IS a colliding piece, i.e. Java's findCollisionPiece != null).
// Block reads/writes, registries, datapacks and the rest of the Mineshaft
// generator are NOT involved — this is pure int arithmetic + one RNG draw +
// the certified BoundingBox.
//
// Reuses the already-certified, header-only:
//   * BoundingBox (../BoundingBox.h)  — ctor + move(dx,dy,dz) + Direction enum
//   * RandomSource (../../RandomSource.h) — nextInt(int) (LegacyRandomSource)
//
// Direction ordinals (net.minecraft.core.Direction declaration order):
//   DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5.
// The switch in findCorridorSize handles SOUTH/WEST/EAST explicitly; every other
// value (NORTH in practice) falls through to `default`. We mirror that exactly.
//
// Certified byte-exact by mine_shaft_corridor_parity
// (tools/MineShaftCorridorParity.java drives the REAL MineShaftCorridor
//  .findCorridorSize via reflection on a no-collision accessor).

#include <cstdint>
#include <functional>
#include <optional>

#include "world/level/levelgen/structure/BoundingBox.h"
#include "world/level/levelgen/RandomSource.h"

namespace mc::levelgen::structure::structures {

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;

// 1:1 port of MineShaftCorridor.findCorridorSize (MineshaftPieces.java:144-168).
//
//   for (int corridorLength = random.nextInt(3) + 2; corridorLength > 0; corridorLength--) {
//      int blockLength = corridorLength * 5;
//      BoundingBox box = switch (direction) {
//         default      -> new BoundingBox(0, 0, -(blockLength - 1), 2, 2, 0);
//         case SOUTH   -> new BoundingBox(0, 0, 0, 2, 2, blockLength - 1);
//         case WEST    -> new BoundingBox(-(blockLength - 1), 0, 0, 0, 2, 2);
//         case EAST    -> new BoundingBox(0, 0, 0, blockLength - 1, 2, 2);
//      };
//      box.move(footX, footY, footZ);
//      if (structurePieceAccessor.findCollisionPiece(box) == null) return box;
//   }
//   return null;
//
// `collides` returns true iff Java's findCollisionPiece(box) != null (a colliding
// piece exists). When it always returns false (a fresh region, as in the parity
// gate), the FIRST iteration returns and the result is the box for the initial
// corridorLength = nextInt(3) + 2.
inline std::optional<BoundingBox> findCorridorSize(
    mc::levelgen::RandomSource& random,
    int32_t footX, int32_t footY, int32_t footZ,
    Direction direction,
    const std::function<bool(const BoundingBox&)>& collides) {
    for (int32_t corridorLength = random.nextInt(3) + 2; corridorLength > 0; corridorLength--) {
        int32_t blockLength = corridorLength * 5;

        BoundingBox box = [&]() -> BoundingBox {
            switch (direction) {
                case Direction::SOUTH:
                    return BoundingBox(0, 0, 0, 2, 2, blockLength - 1);
                case Direction::WEST:
                    return BoundingBox(-(blockLength - 1), 0, 0, 0, 2, 2);
                case Direction::EAST:
                    return BoundingBox(0, 0, 0, blockLength - 1, 2, 2);
                default:  // NORTH (and any non-horizontal) — Java `default` arm
                    return BoundingBox(0, 0, -(blockLength - 1), 2, 2, 0);
            }
        }();

        box.move(footX, footY, footZ);
        if (!collides(box)) {
            return box;
        }
    }

    return std::nullopt;
}

}  // namespace mc::levelgen::structure::structures

#pragma once

// 1:1 port of the PURE, RNG-driven crossing-placement geometry in
//   net.minecraft.world.level.levelgen.structure.structures
//     .MineshaftPieces.MineShaftCrossing.findCrossing(...)  [MineshaftPieces.java:625-647]
//
// findCrossing is a self-contained piece of MineShaftCrossing geometry: given a
// foot position (footX,footY,footZ) and an orientation Direction, it draws ONE
// RandomSource.nextInt(4) (y1 = 6 iff the draw is 0, else y1 = 2), then builds a
// candidate BoundingBox per direction (a fixed 5x(y1+1)x5 footprint, oriented so
// the crossing extends away from the foot), moves it to the foot, and returns the
// box iff it does not collide with an already-placed piece
// (StructurePieceAccessor.findCollisionPiece == null); otherwise null.
//
// This is DISTINCT from the already-gated MineShaftStairs.findStairs
// (mineshaft_stairs_box_parity: a single fixed box, NO RNG draw) and
// MineShaftCorridor.findCorridorSize (mine_shaft_corridor_parity: a nextInt(3)
// length loop). findCrossing has its own nextInt(4) height pick and its own
// per-direction boxes, and unlike findCorridorSize it is a SINGLE candidate (no
// retry loop) — so a colliding region yields null directly.
//
// This header ports EXACTLY that logic. The StructurePieceAccessor collision
// query is abstracted as a caller-supplied predicate `collides(box) -> bool`
// (true == there IS a colliding piece, i.e. Java's findCollisionPiece != null).
// Block reads/writes, registries, datapacks and the rest of the Mineshaft
// generator are NOT involved — this is one RNG draw + a direction switch + the
// certified BoundingBox ctor/move.
//
// Reuses the already-certified, header-only:
//   * BoundingBox (../BoundingBox.h)  — ctor + move(dx,dy,dz) + Direction enum
//   * RandomSource (../../RandomSource.h) — nextInt(int) (LegacyRandomSource)
//
// Direction ordinals (net.minecraft.core.Direction declaration order):
//   DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5.
// The switch in findCrossing handles SOUTH/WEST/EAST explicitly; every other
// value (NORTH in practice, also DOWN/UP) falls through to `default`. We mirror
// that exactly.
//
// Certified byte-exact by mine_shaft_crossing_box_parity
// (tools/MineShaftCrossingBoxParity.java drives the REAL MineShaftCrossing
//  .findCrossing on a no-collision accessor).

#include <cstdint>
#include <functional>
#include <optional>

#include "world/level/levelgen/structure/BoundingBox.h"
#include "world/level/levelgen/RandomSource.h"

namespace mc::levelgen::structure::structures {

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;

// 1:1 port of MineShaftCrossing.findCrossing (MineshaftPieces.java:625-647).
//
//   int y1;
//   if (random.nextInt(4) == 0) { y1 = 6; } else { y1 = 2; }
//   BoundingBox box = switch (direction) {
//      default    -> new BoundingBox(-1, 0, -4, 3, y1, 0);
//      case SOUTH -> new BoundingBox(-1, 0,  0, 3, y1, 4);
//      case WEST  -> new BoundingBox(-4, 0, -1, 0, y1, 3);
//      case EAST  -> new BoundingBox( 0, 0, -1, 4, y1, 3);
//   };
//   box.move(footX, footY, footZ);
//   return structurePieceAccessor.findCollisionPiece(box) != null ? null : box;
//
// `collides` returns true iff Java's findCollisionPiece(box) != null (a colliding
// piece exists). When it always returns false (a fresh region, as in the parity
// gate), findCrossing always returns the moved box.
inline std::optional<BoundingBox> findCrossing(
    mc::levelgen::RandomSource& random,
    int32_t footX, int32_t footY, int32_t footZ,
    Direction direction,
    const std::function<bool(const BoundingBox&)>& collides) {
    int32_t y1;
    if (random.nextInt(4) == 0) {
        y1 = 6;
    } else {
        y1 = 2;
    }

    BoundingBox box = [&]() -> BoundingBox {
        switch (direction) {
            case Direction::SOUTH:
                return BoundingBox(-1, 0, 0, 3, y1, 4);
            case Direction::WEST:
                return BoundingBox(-4, 0, -1, 0, y1, 3);
            case Direction::EAST:
                return BoundingBox(0, 0, -1, 4, y1, 3);
            default:  // NORTH (and any non-horizontal) — Java `default` arm
                return BoundingBox(-1, 0, -4, 3, y1, 0);
        }
    }();

    box.move(footX, footY, footZ);
    if (collides(box)) {
        return std::nullopt;
    }
    return box;
}

}  // namespace mc::levelgen::structure::structures

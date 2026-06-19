#pragma once

// 1:1 port of the PURE, RNG-driven room-placement geometry in
//   net.minecraft.world.level.levelgen.structure.structures
//     .MineshaftPieces.MineShaftRoom(int genDepth, RandomSource random,
//          int west, int north, MineshaftStructure.Type type)
//                                                  [MineshaftPieces.java:1068-1076]
//
// The MineShaftRoom constructor's ONLY computation is the BoundingBox it hands to
// its super-constructor (StructurePiece just stores the box; no further RNG draw):
//
//   super(StructurePieceType.MINE_SHAFT_ROOM, genDepth, type,
//         new BoundingBox(west, 50, north,
//                         west  + 7 + random.nextInt(6),    // maxX  (draw #1)
//                         54        + random.nextInt(6),    // maxY  (draw #2)
//                         north + 7 + random.nextInt(6)));  // maxZ  (draw #3)
//
// Java evaluates the BoundingBox constructor arguments strictly left-to-right, so
// the three RandomSource.nextInt(6) draws happen in the order maxX, maxY, maxZ.
// The box is anchored at (west, 50, north) with a 8..13 (x), 5..10 (y), 8..13 (z)
// span. `type` (NORMAL/MESA) affects only later block placement, NOT this box.
//
// This is DISTINCT from the already-gated Mineshaft box helpers:
//   * MineShaftStairs.findStairs (mineshaft_stairs_box_parity) — a single fixed
//     box, NO RNG draw.
//   * MineShaftCorridor.findCorridorSize (mine_shaft_corridor_parity) — a
//     nextInt(3) length loop over a direction switch.
//   * MineShaftCrossing.findCrossing (mine_shaft_crossing_box_parity) — one
//     nextInt(4) height pick + a direction switch.
// MineShaftRoom draws THREE nextInt(6) and has no direction switch — it is the
// only Mineshaft box anchored to (west,50,north) rather than oriented to a foot.
//
// Block reads/writes, registries, datapacks and the rest of the Mineshaft
// generator are NOT involved — this is three RNG draws + the certified
// BoundingBox ctor (which normalizes only at the int-wrap edges of west/north).
//
// Reuses the already-certified, header-only:
//   * BoundingBox (../BoundingBox.h)        — ctor (with inverted-bounds fix)
//   * RandomSource (../../RandomSource.h)    — nextInt(int) (LegacyRandomSource)
//
// Certified byte-exact by mine_shaft_room_box_parity
// (tools/MineShaftRoomBoxParity.java drives the REAL MineShaftRoom constructor and
//  reads back getBoundingBox()).

#include <cstdint>

#include "world/level/levelgen/structure/BoundingBox.h"
#include "world/level/levelgen/RandomSource.h"

namespace mc::levelgen::structure::structures {

using mc::levelgen::structure::BoundingBox;

// 1:1 port of the MineShaftRoom constructor's BoundingBox computation
// (MineshaftPieces.java:1068-1076). `west`/`north` are the StructurePiece west/
// north anchors; `random` is replayed with the same seed the Java ctor used.
//
//   new BoundingBox(west, 50, north,
//                   west + 7 + random.nextInt(6),
//                   54       + random.nextInt(6),
//                   north + 7 + random.nextInt(6))
//
// The draws are issued in left-to-right argument order (maxX, then maxY, then
// maxZ), exactly as Java evaluates them. The wrapping adds go through the
// certified BoundingBox::iadd so the result matches Java's int overflow, and the
// BoundingBox ctor applies the same inverted-bounds normalization if west/north
// sit close enough to Integer.MAX_VALUE that the +7+draw wraps below the min.
inline BoundingBox makeRoomBox(mc::levelgen::RandomSource& random,
                               int32_t west, int32_t north) {
    using mc::levelgen::structure::iadd;
    // Draw order matches Java argument evaluation: maxX, maxY, maxZ.
    int32_t maxX = iadd(iadd(west, 7), random.nextInt(6));
    int32_t maxY = iadd(54, random.nextInt(6));
    int32_t maxZ = iadd(iadd(north, 7), random.nextInt(6));
    return BoundingBox(west, 50, north, maxX, maxY, maxZ);
}

}  // namespace mc::levelgen::structure::structures

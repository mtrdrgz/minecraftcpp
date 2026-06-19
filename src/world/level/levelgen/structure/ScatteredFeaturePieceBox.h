#pragma once

// 1:1 port of the RNG-driven constructor geometry of
//   net.minecraft.world.level.levelgen.structure.ScatteredFeaturePiece (26.1.2)
// together with the random-orientation helper it relies on,
//   net.minecraft.world.level.levelgen.structure.StructurePiece
//       .getRandomHorizontalDirection(RandomSource)        [lines 80-82]
//   -> net.minecraft.core.Direction.Plane.HORIZONTAL
//          .getRandomDirection(RandomSource)               [Direction.java:588-590]
//   -> net.minecraft.util.Util.getRandom(T[], RandomSource) [Util.java:777-779]
//          == faces[random.nextInt(faces.length)]
//      with HORIZONTAL.faces = {NORTH, EAST, SOUTH, WEST}   [Direction.java:577]
//
// Every scattered-feature temple (DesertPyramidPiece 21x15x21, SwampHutPiece
// 7x7x9, JungleTemplePiece, ...) builds its world bounding box in its ctor:
//
//   ScatteredFeaturePiece(type, west, floor=64, north, width, height, depth,
//                         getRandomHorizontalDirection(random))            // 1 nextInt(4) draw
//     -> super(type, 0, StructurePiece.makeBoundingBox(west,floor,north,dir,
//                                                       width,height,depth)) // [StructurePiece 72-78]
//        this.setOrientation(direction)                                     // [StructurePiece 533-557]
//
// This header ports EXACTLY that pure chain: one RandomSource.nextInt(4) draw to
// pick the horizontal direction, then the certified makeBoundingBox + setOrientation
// math from StructurePieceMath.h. There are NO world writes, NO registry access,
// NO datapack here — the postProcess() body (generateBox/placeBlock/createChest)
// is deliberately NOT ported.
//
// Reuses (does NOT redefine) the already-certified ports:
//   - StructurePieceMath.h : makeBoundingBox, setOrientation, BoundingBox,
//                            Direction, Mirror, Rotation, Axis
//                            (gated by structure_piece_math_parity)
//   - world/level/levelgen/RandomSource.h/.cpp : the seeded RandomSource
//                            (gated by worldgen_random_parity et al.)
//
// Certified byte-exact by scattered_feature_box_parity
// (tools/ScatteredFeatureBoxParity.java), driving the REAL SwampHutPiece and
// DesertPyramidPiece constructors.

#include "world/level/levelgen/structure/StructurePieceMath.h"
#include "world/level/levelgen/RandomSource.h"

namespace mc::levelgen::structure::piece {

// Direction.Plane.HORIZONTAL.faces — Direction.java:577.
// Declaration order is {NORTH, EAST, SOUTH, WEST}; Util.getRandom indexes this
// array by random.nextInt(4), so the order here is load-bearing.
inline constexpr Direction HORIZONTAL_FACES[4]{
    Direction::NORTH, Direction::EAST, Direction::SOUTH, Direction::WEST};

// net.minecraft.core.Direction.Plane.HORIZONTAL.getRandomDirection(random)
//   == Util.getRandom(faces, random) == faces[random.nextInt(faces.length)].
// (Direction.java:588-590 / Util.java:777-779). faces.length == 4.
inline Direction getRandomHorizontalDirection(mc::levelgen::RandomSource& random) {
    return HORIZONTAL_FACES[random.nextInt(4)];
}

// The result of constructing a ScatteredFeaturePiece: the world-space bounding
// box plus the orientation/rotation/mirror state set by setOrientation.
struct ScatteredFeaturePieceCtor {
    BoundingBox boundingBox{};
    Direction orientation = Direction::NORTH;  // always a horizontal here
    Mirror mirror = Mirror::NONE;
    Rotation rotation = Rotation::NONE;
};

// net.minecraft.world.level.levelgen.structure.ScatteredFeaturePiece ctor
// (ScatteredFeaturePiece.java:17-32) — exactly as invoked by every scattered
// temple piece. `floor` is the fixed y the piece passes (64 for desert pyramid /
// swamp hut). The single RandomSource draw (nextInt(4)) happens first, as the
// `getRandomHorizontalDirection(random)` argument is evaluated before super(...).
inline ScatteredFeaturePieceCtor makeScatteredFeaturePiece(
    mc::levelgen::RandomSource& random,
    int west, int floor, int north,
    int width, int height, int depth) {
    Direction direction = getRandomHorizontalDirection(random);

    ScatteredFeaturePieceCtor out;
    out.boundingBox = makeBoundingBox(west, floor, north, direction, width, height, depth);

    // setOrientation(direction): mirror/rotation derivation (StructurePiece 533-557).
    StructurePieceMath m;
    m.setOrientation(true, direction);
    out.orientation = direction;
    out.mirror = m.mirror;
    out.rotation = m.rotation;
    return out;
}

// Convenience wrappers for the two concrete scattered-feature pieces with
// constant dimensions, used by the parity gate.
//   SwampHutPiece:        width=7,  height=7,  depth=9,  floor=64 (SwampHutPiece.java:30)
//   DesertPyramidPiece:   width=21, height=15, depth=21, floor=64 (DesertPyramidPiece.java:31)
inline ScatteredFeaturePieceCtor makeSwampHutPiece(
    mc::levelgen::RandomSource& random, int west, int north) {
    return makeScatteredFeaturePiece(random, west, 64, north, 7, 7, 9);
}

inline ScatteredFeaturePieceCtor makeDesertPyramidPiece(
    mc::levelgen::RandomSource& random, int west, int north) {
    return makeScatteredFeaturePiece(random, west, 64, north, 21, 15, 21);
}

} // namespace mc::levelgen::structure::piece

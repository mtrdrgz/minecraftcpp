#pragma once

// 1:1 C++ port of the PURE static bounding-box probe nested in the REAL
// decompiled 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.structures.MineshaftPieces
//     -> public static class MineShaftStairs
//          public static @Nullable BoundingBox findStairs(
//              StructurePieceAccessor, RandomSource, int footX, int footY,
//              int footZ, Direction)                    [MineshaftPieces.java:1290-1306]
//
// findStairs is the geometry probe the mineshaft generator runs when it wants to
// splice a descending stair piece off the foot of an existing corridor: it forms
// the fixed stair box in the requested `direction`, slides it to the foot
// position, and returns it ONLY if nothing already occupies that space. It is
// PURE control flow over integer geometry:
//   * It NEVER reads `random` (the param is part of the shared piece-factory
//     signature shared by every MineShaft* findX helper; findStairs ignores it)
//     -> NO RandomSource generation.
//   * It performs NO world writes, NO registry/datapack access.
//   * Its ONLY contact with mutable structure state is a single
//     `accessor.findCollisionPiece(box)` lookup, and it uses ONLY the NULLNESS of
//     the returned piece (`!= null ? null : box`) -- it never reads the collided
//     piece's box. So the result is a deterministic pure function of
//     (footX, footY, footZ, direction) and the single boolean "was something there".
//
// We therefore model the accessor exactly as the harness does: the caller hands in
// a `bool collisionPresent` (true == findCollisionPiece returned a non-null piece).
// The parity gate drives the REAL MineShaftStairs.findStairs with an inline
// StructurePieceAccessor that returns a real piece (or null), so the boolean we
// accept here is byte-for-byte the Java `collisionPiece != null` branch input.
//
// EXACT Java translated (MineshaftPieces.java:1290-1306):
//   public static BoundingBox findStairs(acc, random, footX, footY, footZ, dir){
//      BoundingBox box = switch (dir) {
//         default -> new BoundingBox(0, -5, -8, 2, 2, 0);   // NORTH (+ DOWN/UP, unused)
//         case SOUTH -> new BoundingBox(0, -5, 0, 2, 2, 8);
//         case WEST  -> new BoundingBox(-8, -5, 0, 0, 2, 2);
//         case EAST  -> new BoundingBox(0, -5, 0, 8, 2, 2);
//      };
//      box.move(footX, footY, footZ);
//      return acc.findCollisionPiece(box) != null ? null : box;
//   }
//
// 1:1 TRAPS this gate locks down:
//   * findStairs returns the box ONLY when there is NO collision (the ternary is
//     `!= null ? null : box` -- present-collision yields null). This is the same
//     "empty = ok" contract as the corridors but is the INVERSE of the sibling
//     stronghold FillerCorridor.findPieceBox, which returns null when EMPTY. A port
//     that copies the stronghold logic here would be exactly backwards.
//   * The box is built with the LITERAL `new BoundingBox(...)` constructor (NOT
//     orientBox), so the per-direction constants are baked corner pairs, and the
//     BoundingBox ctor's inverted-axis normalization (BoundingBox.java:48-64) is the
//     relevant fixup -- though for these constants no axis is inverted, the +move is
//     applied AFTER construction, exactly as Java does (move never re-normalizes).
//   * The per-direction stair shape is asymmetric: NORTH extends -8 in Z (a box of
//     Z-span 9, Y from -5..2, X 0..2); SOUTH mirrors it to +8 Z; WEST extends -8 in
//     X; EAST +8 X. The -5 floor offset (stairs descend) and the 2x2 cross-section
//     are copied verbatim, never re-derived.
//   * move(footX,footY,footZ) wraps two's-complement (Java int overflow) -- reused
//     from BoundingBox.h (certified byte-exact by bounding_box_parity).
//   * `default` in the Java switch IS the NORTH branch (and would also catch the
//     vertical DOWN/UP, which the generator never passes); we map Direction::NORTH
//     to that constant set so the four horizontals are exhaustively covered.
//
// Certified byte-exact by mineshaft_stairs_box_parity
// (tools/MineshaftStairsBoxParity.java), which drives the REAL
// MineshaftPieces.MineShaftStairs.findStairs.

#include "../BoundingBox.h"

#include <optional>

namespace mc::levelgen::structure::structures {

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;

// MineshaftPieces.MineShaftStairs.findStairs -- MineshaftPieces.java:1290-1306.
//
// `collisionPresent` is true when StructurePieceAccessor.findCollisionPiece(box)
// returns a non-null piece (whose box findStairs never inspects), false when it
// returns null. The `random` of the Java signature is dead w.r.t. the result and
// is intentionally omitted.
//
// Returns the computed stair box, or std::nullopt for the Java `null`.
inline std::optional<BoundingBox> findStairs(bool collisionPresent,
                                             int32_t footX, int32_t footY, int32_t footZ,
                                             Direction direction) noexcept {
    // BoundingBox box = switch (direction) { ... };
    BoundingBox box;
    switch (direction) {
        case Direction::SOUTH:
            box = BoundingBox(0, -5, 0, 2, 2, 8);
            break;
        case Direction::WEST:
            box = BoundingBox(-8, -5, 0, 0, 2, 2);
            break;
        case Direction::EAST:
            box = BoundingBox(0, -5, 0, 8, 2, 2);
            break;
        case Direction::NORTH:
        default: // `default ->` is the NORTH branch in the Java switch.
            box = BoundingBox(0, -5, -8, 2, 2, 0);
            break;
    }

    // box.move(footX, footY, footZ);
    box.move(footX, footY, footZ);

    // return accessor.findCollisionPiece(box) != null ? null : box;
    if (collisionPresent) {
        return std::nullopt;
    }
    return box;
}

} // namespace mc::levelgen::structure::structures

#pragma once

// 1:1 C++ port of the PURE static bounding-box helper nested in the REAL
// decompiled 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces
//     -> public static class FillerCorridor
//          public static @Nullable BoundingBox findPieceBox(
//              StructurePieceAccessor, RandomSource, int footX, int footY,
//              int footZ, Direction)                    [StrongholdPieces.java:314-339]
//
// findPieceBox is the geometry probe the stronghold generator runs when it wants
// to splice a *filler corridor* between an existing piece and a new junction: it
// asks how deep a 5x5 box can extend in `direction` before it would overlap the
// piece already sitting at the foot position. It is PURE control flow over
// integer geometry:
//   * It NEVER reads `random` (the param is part of the shared piece-factory
//     signature; findPieceBox ignores it) -> NO RandomSource generation.
//   * It performs NO world writes, NO registry/datapack access.
//   * Its ONLY contact with mutable structure state is a single
//     `accessor.findCollisionPiece(box0)` lookup; everything after that reads the
//     returned piece's (immutable) BoundingBox. So GIVEN the box of the piece the
//     accessor returns (or "none"), the result is a deterministic pure function.
//
// We therefore model the accessor exactly as the harness does: the caller hands
// in the collision piece's box as std::optional<BoundingBox> `collisionBox`
// (std::nullopt == findCollisionPiece returned null). The parity gate drives the
// REAL FillerCorridor.findPieceBox with an inline StructurePieceAccessor that
// returns a real FillerCorridor carrying `collisionBox` (or null), so the
// optional we accept here is byte-for-byte the Java `collisionPiece` branch input.
//
// EXACT Java translated (StrongholdPieces.java:314-339):
//   public static BoundingBox findPieceBox(acc, random, footX, footY, footZ, dir){
//      int maxLength = 3;                                   // unused dead local
//      BoundingBox box = BoundingBox.orientBox(footX,footY,footZ,-1,-1,0,5,5,4,dir);
//      StructurePiece collisionPiece = acc.findCollisionPiece(box);
//      if (collisionPiece == null) return null;
//      if (collisionPiece.getBoundingBox().minY() == box.minY()) {
//         for (int depth = 2; depth >= 1; depth--) {
//            box = BoundingBox.orientBox(footX,footY,footZ,-1,-1,0,5,5,depth,dir);
//            if (!collisionPiece.getBoundingBox().intersects(box))
//               return BoundingBox.orientBox(footX,footY,footZ,-1,-1,0,5,5,depth+1,dir);
//         }
//      }
//      return null;
//   }
//
// 1:1 TRAPS this gate locks down:
//   * The function RETURNS null when there is NO collision piece (an empty/open
//     foot must NOT yield a box) -- the inverse of the usual "no collision = ok".
//     A naive port that returns the depth-4 box on no-collision is wrong.
//   * The gating `collisionPiece.getBoundingBox().minY() == box.minY()` compares
//     the collision box's minY against the DEPTH-4 probe box's minY (the first
//     orientBox), NOT against the candidate boxes inside the loop. If the existing
//     piece sits at a different floor, findPieceBox bails to null.
//   * The loop walks depth = 2 then 1 (DESCENDING) and returns on the FIRST depth
//     whose box does NOT intersect the collision piece -- returning orientBox at
//     `depth + 1` (the deepest box that still *touched*, i.e. one step longer than
//     the first gap). The +1 and the descending order are both load-bearing.
//   * If BOTH depth=2 and depth=1 still intersect, the loop falls through and the
//     method returns null (no room even for the shortest corridor).
//   * orientBox/intersects/minY are reused verbatim from BoundingBox.h (each
//     certified byte-exact by bounding_box_parity); the offsets (-1,-1,0) and the
//     fixed 5x5 cross-section, plus widths {4,3,2} & {return depth+1}, are copied
//     exactly from the Java -- never re-derived.
//
// Certified byte-exact by stronghold_piece_box_parity
// (tools/StrongholdPieceBoxParity.java), which drives the REAL
// StrongholdPieces.FillerCorridor.findPieceBox.

#include "../BoundingBox.h"

#include <optional>

namespace mc::levelgen::structure::structures {

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;

// StrongholdPieces.FillerCorridor.findPieceBox -- StrongholdPieces.java:314-339.
//
// `collisionBox` is the BoundingBox of the piece that
// StructurePieceAccessor.findCollisionPiece(<depth-4 probe box>) returns, or
// std::nullopt when it returns null. The `random`/`maxLength` of the Java
// signature are dead w.r.t. the result and are intentionally omitted.
//
// Returns the computed corridor box, or std::nullopt for the Java `null`.
inline std::optional<BoundingBox> findPieceBox(const std::optional<BoundingBox>& collisionBox,
                                               int32_t footX, int32_t footY, int32_t footZ,
                                               Direction direction) noexcept {
    // BoundingBox box = orientBox(footX,footY,footZ, -1,-1,0, 5,5,4, direction);
    BoundingBox box = BoundingBox::orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, 4, direction);

    // StructurePiece collisionPiece = accessor.findCollisionPiece(box);
    // if (collisionPiece == null) return null;
    if (!collisionBox.has_value()) {
        return std::nullopt;
    }
    const BoundingBox& collision = *collisionBox;

    // if (collisionPiece.getBoundingBox().minY() == box.minY()) { ... }
    if (collision.minY == box.minY) {
        // for (int depth = 2; depth >= 1; depth--)
        for (int32_t depth = 2; depth >= 1; --depth) {
            box = BoundingBox::orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, depth, direction);
            // if (!collisionPiece.getBoundingBox().intersects(box))
            //     return orientBox(..., depth + 1, direction);
            if (!collision.intersects(box)) {
                return BoundingBox::orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, depth + 1, direction);
            }
        }
    }

    // return null;
    return std::nullopt;
}

} // namespace mc::levelgen::structure::structures

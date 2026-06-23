#pragma once

// 1:1 port of the PURE list/bbox aggregation helpers in
//   net.minecraft.world.level.levelgen.structure.StructurePiece (26.1.2)
// plus the multi-box fold they delegate to in
//   net.minecraft.world.level.levelgen.structure.BoundingBox.
//
// These are the bounds/collision helpers that StructurePiecesBuilder (and through
// it every StructureStart) leans on:
//   StructurePiecesBuilder.getBoundingBox()  == StructurePiece.createBoundingBox(stream)
//   StructurePiecesBuilder.findCollisionPiece == StructurePiece.findCollisionPiece(list, box)
//
// The math is pure: it folds over an ordered sequence of the pieces' bounding
// boxes. There are NO world writes, NO RandomSource generation, NO registry or
// datapack access. The actual StructurePiece objects matter only through their
// BoundingBox, so the helpers below operate on the boxes directly — exactly what
// `pieces.map(StructurePiece::getBoundingBox)` and `piece.getBoundingBox()`
// produce in Java.
//
// Ported members (verbatim from the decompiled 26.1.2 sources):
//   BoundingBox.encapsulatingBoxes(Iterable<BoundingBox>)  [BoundingBox.java:138-148]
//   StructurePiece.createBoundingBox(Stream<StructurePiece>) [StructurePiece.java:514-517]
//   StructurePiece.findCollisionPiece(List, BoundingBox)     [StructurePiece.java:519-527]
//
// BoundingBox (int fields, inverting ctor, encapsulate, intersects) is reused
// from BoundingBox.h — itself certified byte-exact by bounding_box_parity.
//
// 1:1 fidelity notes:
//   * encapsulatingBoxes copies the FIRST box's raw min/max into a fresh, mutable
//     BoundingBox (NOT via fromCorners/orientBox), then folds the deprecated
//     mutating encapsulate(box) over the rest in iterator order. Because the seed
//     is already a valid (non-inverted) box, the copy is faithful; we reproduce
//     it with a direct field copy, not the inverting ctor.
//   * Empty input -> Optional.empty(); createBoundingBox then throws
//     IllegalStateException("Unable to calculate boundingbox without pieces").
//     We model the optional with std::optional and the throw with a std::logic_error
//     carrying the identical message (callers in the engine never hit it, but the
//     parity gate exercises both branches).
//   * findCollisionPiece returns the FIRST box (by list order) that intersects;
//     iteration order and the short-circuit on first match are load-bearing.
//   * encapsulate / intersects use Math.min/Math.max and pure comparisons — no int
//     overflow is possible (min/max never wrap), so no uint dance is needed here;
//     the BoundingBox.h primitives already match Java bit-for-bit.
//
// Certified byte-exact by structure_piece_collection_parity
// (tools/StructurePieceCollectionParity.java).

#include "BoundingBox.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

namespace mc::levelgen::structure::piece {

// NOTE: deliberately NOT `using mc::levelgen::structure::BoundingBox;` here.
// StructurePieceMath.h (a sibling header) defines its OWN piece::BoundingBox
// struct, and the `using` directive would conflict with it when both are
// included in the same TU (e.g. StructureGen.cpp pulls in both via
// StructurePieceBase.h -> StructurePieceMath.h and via MineshaftAssembly.h ->
// StructurePieceCollection.h). We use the fully-qualified
// mc::levelgen::structure::BoundingBox instead.

// BoundingBox.encapsulatingBoxes(Iterable<BoundingBox>) — BoundingBox.java:138-148.
// Returns empty when the sequence is empty; otherwise seeds from the first box's
// raw fields and folds the mutating encapsulate over the remainder in order.
inline std::optional<mc::levelgen::structure::BoundingBox> encapsulatingBoxes(
        const std::vector<mc::levelgen::structure::BoundingBox>& boxes) {
    if (boxes.empty()) {
        return std::nullopt;
    }
    const mc::levelgen::structure::BoundingBox& first = boxes.front();
    // new BoundingBox(first.minX, first.minY, first.minZ, first.maxX, first.maxY, first.maxZ)
    // — a faithful field copy of an already-valid box (no inversion can occur).
    mc::levelgen::structure::BoundingBox result;
    result.minX = first.minX; result.minY = first.minY; result.minZ = first.minZ;
    result.maxX = first.maxX; result.maxY = first.maxY; result.maxZ = first.maxZ;
    for (std::size_t i = 1; i < boxes.size(); ++i) {
        result.encapsulate(boxes[i]); // BoundingBox.encapsulate(box) — mutating fold.
    }
    return result;
}

// StructurePiece.createBoundingBox(Stream<StructurePiece>) — StructurePiece.java:514-517.
//   encapsulatingBoxes(pieces.map(getBoundingBox)).orElseThrow(IllegalStateException)
// The Java message is reproduced verbatim.
inline mc::levelgen::structure::BoundingBox createBoundingBox(
        const std::vector<mc::levelgen::structure::BoundingBox>& pieceBoxes) {
    auto bb = encapsulatingBoxes(pieceBoxes);
    if (!bb.has_value()) {
        throw std::logic_error("Unable to calculate boundingbox without pieces");
    }
    return *bb;
}

// StructurePiece.findCollisionPiece(List<StructurePiece>, BoundingBox)
// — StructurePiece.java:519-527. Returns the index of the first piece (by list
// order) whose box intersects `box`; -1 if none (Java returns @Nullable null).
inline int findCollisionPieceIndex(
        const std::vector<mc::levelgen::structure::BoundingBox>& pieceBoxes,
        const mc::levelgen::structure::BoundingBox& box) {
    for (std::size_t i = 0; i < pieceBoxes.size(); ++i) {
        if (pieceBoxes[i].intersects(box)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace mc::levelgen::structure::piece

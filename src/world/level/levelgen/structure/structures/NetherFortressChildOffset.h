#pragma once

// 1:1 C++ port of the PURE child-piece foot-position + facing geometry of
//   net.minecraft.world.level.levelgen.structure.structures.NetherFortressPieces
//       .NetherBridgePiece.generateChildForward(...)   (NetherFortressPieces.java:1244-1307)
//       .NetherBridgePiece.generateChildLeft(...)       (NetherFortressPieces.java:1309-1372)
//       .NetherBridgePiece.generateChildRight(...)      (NetherFortressPieces.java:1374-1437)
//   (decompiled Minecraft 26.1.2).
//
// Every nether-fortress piece, when it spawns its neighbours, calls one of these
// three NetherBridgePiece base helpers. Each one maps the PARENT piece's
// (boundingBox, orientation) plus the per-call (xOff/yOff/zOff) into:
//     * the CHILD foot position (footX, footY, footZ), and
//     * the CHILD facing Direction,
// then forwards them to generateAndAddPiece(...) -> generatePiece(...) ->
// the chosen piece's createPiece(...), which builds the child box via
// BoundingBox.orientBox(footX,footY,footZ, off.., dim.., childDir). The
// (boundingBox, orientation, offsets) -> (footX, footY, footZ, childDir) mapping
// ported here is self-contained two's-complement integer arithmetic + a switch
// on the parent's orientation; there are NO world writes, NO RandomSource draws,
// NO registry/datapack access in this mapping.
//
// What is DELIBERATELY NOT ported here (and is genuinely separate from this pure
// mapping):
//   * generateAndAddPiece's 112-block distance gate, the weighted piece RNG
//     selection in generatePiece, and the accessor.addPiece bookkeeping — all
//     stateful / RNG-driven / world-bound.
//   * the orientBox box-build itself (already gated by NetherFortressPieceBox.h /
//     bounding_box_parity) and isOkBox (NetherFortressPieceBox.h).
// Those layers are gated elsewhere; this header is ONLY the per-direction foot
// position + facing derivation that decides WHERE the next piece is anchored.
//
// 1:1 TRAPS faithfully reproduced:
//   * Java int arithmetic WRAPS (two's complement); the +/-1 / +xOff / +zOff are
//     routed through uint32_t (iadd/isub) to avoid C++ signed-overflow UB at -O2.
//   * generateChildForward keeps the SAME facing as the parent (orientation),
//     whereas generateChildLeft always faces WEST(N/S parent)/NORTH(W/E parent)
//     and generateChildRight always faces EAST(N/S parent)/SOUTH(W/E parent).
//     The NORTH/SOUTH and WEST/EAST branches are pairwise identical in Left/Right.
//   * Forward uses boundingBox.minZ()-1 (NORTH) / maxZ()+1 (SOUTH) / minX()-1
//     (WEST) / maxX()+1 (EAST) for the advancing axis; Left/Right mirror this on
//     the perpendicular axis. These exact min/max picks are load-bearing.
//   * If the parent has no orientation (Java orientation == null), all three
//     return null (no child) — modelled by hasOrientation=false -> present=false.
//     Real fortress pieces always have a horizontal orientation.
//
// Certified byte-exact by nether_fortress_child_offset_parity
// (tools/NetherFortressChildOffsetParity.java), which drives the REAL
// NetherBridgePiece.generateChild{Forward,Left,Right} via reflection (with the
// piece-weight lists emptied so the chosen child is always the RNG-free
// BridgeEndFiller) and reads back the produced child box + null-ness.

#include "../BoundingBox.h"

#include <cstdint>

namespace mc::levelgen::structure::structures {

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;
using mc::levelgen::structure::iadd;
using mc::levelgen::structure::isub;

// The pure result of one generateChild{Forward,Left,Right} call: the child piece
// foot anchor + its facing. `present` is false iff the parent had no orientation
// (Java returns null in that case — no child position is produced).
struct NetherFortressChildOffset {
    bool present = false;          // false == Java returned null (no orientation)
    int32_t footX = 0, footY = 0, footZ = 0;
    Direction direction = Direction::NORTH;
    constexpr bool operator==(const NetherFortressChildOffset&) const = default;
};

// NetherFortressPieces.NetherBridgePiece.generateChildForward(...,xOff,yOff,...)
// — NetherFortressPieces.java:1244-1307. The advancing piece keeps the parent's
// facing; the foot anchor sits one block past the parent on the facing axis.
inline constexpr NetherFortressChildOffset netherFortressChildForward(
    const BoundingBox& parentBox, bool hasOrientation, Direction orientation,
    int32_t xOff, int32_t yOff) noexcept {
    if (!hasOrientation) return {};
    switch (orientation) {
        case Direction::NORTH:
            return {true, iadd(parentBox.minX, xOff), iadd(parentBox.minY, yOff),
                    isub(parentBox.minZ, 1), orientation};
        case Direction::SOUTH:
            return {true, iadd(parentBox.minX, xOff), iadd(parentBox.minY, yOff),
                    iadd(parentBox.maxZ, 1), orientation};
        case Direction::WEST:
            return {true, isub(parentBox.minX, 1), iadd(parentBox.minY, yOff),
                    iadd(parentBox.minZ, xOff), orientation};
        case Direction::EAST:
            return {true, iadd(parentBox.maxX, 1), iadd(parentBox.minY, yOff),
                    iadd(parentBox.minZ, xOff), orientation};
        default:  // DOWN/UP never occur for a fortress piece orientation
            return {};
    }
}

// NetherFortressPieces.NetherBridgePiece.generateChildLeft(...,yOff,zOff,...)
// — NetherFortressPieces.java:1309-1372. Turns to the parent's LEFT: a N/S parent
// emits a WEST-facing child, a W/E parent emits a NORTH-facing child.
inline constexpr NetherFortressChildOffset netherFortressChildLeft(
    const BoundingBox& parentBox, bool hasOrientation, Direction orientation,
    int32_t yOff, int32_t zOff) noexcept {
    if (!hasOrientation) return {};
    switch (orientation) {
        case Direction::NORTH:
        case Direction::SOUTH:
            return {true, isub(parentBox.minX, 1), iadd(parentBox.minY, yOff),
                    iadd(parentBox.minZ, zOff), Direction::WEST};
        case Direction::WEST:
        case Direction::EAST:
            return {true, iadd(parentBox.minX, zOff), iadd(parentBox.minY, yOff),
                    isub(parentBox.minZ, 1), Direction::NORTH};
        default:
            return {};
    }
}

// NetherFortressPieces.NetherBridgePiece.generateChildRight(...,yOff,zOff,...)
// — NetherFortressPieces.java:1374-1437. Turns to the parent's RIGHT: a N/S parent
// emits an EAST-facing child, a W/E parent emits a SOUTH-facing child.
inline constexpr NetherFortressChildOffset netherFortressChildRight(
    const BoundingBox& parentBox, bool hasOrientation, Direction orientation,
    int32_t yOff, int32_t zOff) noexcept {
    if (!hasOrientation) return {};
    switch (orientation) {
        case Direction::NORTH:
        case Direction::SOUTH:
            return {true, iadd(parentBox.maxX, 1), iadd(parentBox.minY, yOff),
                    iadd(parentBox.minZ, zOff), Direction::EAST};
        case Direction::WEST:
        case Direction::EAST:
            return {true, iadd(parentBox.minX, zOff), iadd(parentBox.minY, yOff),
                    iadd(parentBox.maxZ, 1), Direction::SOUTH};
        default:
            return {};
    }
}

} // namespace mc::levelgen::structure::structures

#pragma once

// 1:1 C++ port of the PURE box-construction + validity layer of every
//   net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces.<X>
//   .createPiece(...) static factory (decompiled Minecraft 26.1.2).
//
// Each concrete stronghold piece type builds its candidate BoundingBox the SAME
// way the nether-fortress pieces do, but with its OWN per-type orientBox
// constants:
//
//     BoundingBox box = BoundingBox.orientBox(footX, footY, footZ,
//                                             <offX,offY,offZ>, <width,height,depth>,
//                                             direction);
//     return isOkBox(box) && accessor.findCollisionPiece(box) == null
//          ? new <X>(...) : null;
//
// The per-type constants {offX,offY,offZ,width,height,depth} are the load-bearing
// data — they fix each corridor/room/stairs footprint and clearance. This header
// ports exactly that pure layer:
//   * the box geometry = BoundingBox.orientBox(...) (reused from BoundingBox.h),
//   * the validity gate = StrongholdPieces.StrongholdPiece.isOkBox(box)
//        == `box.minY() > 10`                       (StrongholdPieces.java:1736-1738),
// for every concrete stronghold piece type, indexed by the exact orientBox(...)
// args each REAL createPiece passes.
//
// LIBRARY is special (StrongholdPieces.java:601-607): it first tries the TALL box
//   orientBox(-4,-1,0, 14,11,15) and, only if that fails isOkBox OR collides,
//   falls back to the SHORT box orientBox(-4,-1,0, 14,6,15). Because the fallback
//   selection is partly a `findCollisionPiece` decision (stateful), the two Library
//   boxes are exposed here as TWO separate, independently-pure entries
//   (LibraryTall / LibraryShort); the gate certifies both. The runtime selection
//   between them lives with the (excluded) accessor lookup.
//
// What is DELIBERATELY NOT ported here (and why the rest is still pure to gate):
// the trailing `accessor.findCollisionPiece(box) == null` is a stateful lookup
// against the in-progress piece list, and the `new <X>(...)` allocation is a
// world-bound object — neither is part of this geometry layer. The `RandomSource
// random` that every createPiece overload (except PortalRoom, which takes none)
// accepts is NEVER consumed before the box is built — it is only forwarded into
// the piece constructor — so the box + isOkBox result is a pure function of
// (footX, footY, footZ, direction) and the piece type. NO world writes, NO
// RandomSource draws, NO registry/datapack.
//
// NOT a duplicate of stronghold_piece_box (StrongholdPieceBox.h): that gate covers
// FillerCorridor.findPieceBox (a multi-probe corridor-splice search). This gate
// covers the per-type createPiece box+isOkBox factory layer, which findPieceBox is
// not part of.
//
// 1:1 TRAPS faithfully reproduced (all inherited from BoundingBox.orientBox / the
// BoundingBox ctor, reused verbatim from BoundingBox.h):
//   * orientBox corner arithmetic wraps two's-complement (Java int overflow);
//     BoundingBox.h routes every +/- through uint32_t to avoid C++ signed-overflow
//     UB at -O2 near Integer.MIN/MAX_VALUE.
//   * orientBox switches on the FOUR horizontals only; the SOUTH branch is the
//     `default` (DOWN/UP, were they ever passed, fall through to SOUTH). Real
//     stronghold generation only ever supplies a horizontal Direction.
//   * the BoundingBox ctor SWAPS inverted axes — for negative-offset pieces the
//     raw min/max can invert and Java silently re-sorts them; we keep that swap.
//   * isOkBox compares the FINAL (post-swap, post-orient) minY strictly `> 10`.
//
// Certified byte-exact by stronghold_piece_type_box_parity
// (tools/StrongholdPieceTypeBoxParity.java), which drives the REAL
// BoundingBox.orientBox with each type's constants + the REAL (reflected) isOkBox.

#include "../BoundingBox.h"

#include <cstdint>

namespace mc::levelgen::structure::structures {

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;

// The concrete StrongholdPieces piece types whose createPiece(...) builds a box
// via orientBox. Ordered as they appear in StrongholdPieces.java. FillerCorridor
// is intentionally absent (its box layer is the separate findPieceBox probe,
// gated by stronghold_piece_box).
enum class StrongholdPiece : int32_t {
    ChestCorridor = 0,        // .java:258  orientBox(-1,-1,0,  5, 5, 7)
    FiveCrossing,             // .java:448  orientBox(-4,-3,0, 10, 9,11)
    LeftTurn,                 // .java:542  orientBox(-1,-1,0,  5, 5, 5)
    LibraryTall,              // .java:601  orientBox(-4,-1,0, 14,11,15)
    LibraryShort,            // .java:603  orientBox(-4,-1,0, 14, 6,15)  (fallback box)
    PortalRoom,              // .java:785  orientBox(-4,-1,0, 11, 8,16)
    PrisonHall,              // .java:914  orientBox(-1,-1,0,  9, 5,11)
    RightTurn,               // .java:999  orientBox(-1,-1,0,  5, 5, 5)
    RoomCrossing,            // .java:1066 orientBox(-4,-1,0, 11, 7,11)
    StairsDown,              // .java:1242 orientBox(-1,-7,0,  5,11, 5)
    Straight,                // .java:1349 orientBox(-1,-1,0,  5, 5, 7)
    StraightStairsDown       // .java:1411 orientBox(-1,-7,0,  5,11, 8)
};

// The orientBox(...) arguments (offX,offY,offZ,width,height,depth) each concrete
// createPiece passes — copied verbatim from StrongholdPieces.java.
struct StrongholdBoxSpec {
    int32_t offX, offY, offZ;
    int32_t width, height, depth;
};

// StrongholdPieces.<type>.createPiece orientBox constants — verbatim.
inline constexpr StrongholdBoxSpec strongholdBoxSpec(StrongholdPiece type) noexcept {
    switch (type) {
        case StrongholdPiece::ChestCorridor:      return {-1, -1, 0,  5,  5,  7}; // .java:258
        case StrongholdPiece::FiveCrossing:       return {-4, -3, 0, 10,  9, 11}; // .java:448
        case StrongholdPiece::LeftTurn:           return {-1, -1, 0,  5,  5,  5}; // .java:542
        case StrongholdPiece::LibraryTall:        return {-4, -1, 0, 14, 11, 15}; // .java:601
        case StrongholdPiece::LibraryShort:       return {-4, -1, 0, 14,  6, 15}; // .java:603
        case StrongholdPiece::PortalRoom:         return {-4, -1, 0, 11,  8, 16}; // .java:785
        case StrongholdPiece::PrisonHall:         return {-1, -1, 0,  9,  5, 11}; // .java:914
        case StrongholdPiece::RightTurn:          return {-1, -1, 0,  5,  5,  5}; // .java:999
        case StrongholdPiece::RoomCrossing:       return {-4, -1, 0, 11,  7, 11}; // .java:1066
        case StrongholdPiece::StairsDown:         return {-1, -7, 0,  5, 11,  5}; // .java:1242
        case StrongholdPiece::Straight:           return {-1, -1, 0,  5,  5,  7}; // .java:1349
        case StrongholdPiece::StraightStairsDown: return {-1, -7, 0,  5, 11,  8}; // .java:1411
    }
    return {0, 0, 0, 0, 0, 0}; // unreachable; keeps the compiler happy
}

// BoundingBox built by StrongholdPieces.<type>.createPiece for the given foot
// position + facing. Pure: identical to the REAL
//   BoundingBox.orientBox(footX, footY, footZ, off.., dim.., direction).
inline constexpr BoundingBox strongholdPieceBox(StrongholdPiece type, int32_t footX, int32_t footY,
                                                int32_t footZ, Direction direction) noexcept {
    StrongholdBoxSpec s = strongholdBoxSpec(type);
    return BoundingBox::orientBox(footX, footY, footZ, s.offX, s.offY, s.offZ, s.width, s.height, s.depth,
                                  direction);
}

// StrongholdPieces.StrongholdPiece.isOkBox(BoundingBox) —
// StrongholdPieces.java:1736-1738: `return box.minY() > 10;`. (Post-swap minY,
// i.e. the actual minimum-Y field of the constructed box.)
inline constexpr bool strongholdIsOkBox(const BoundingBox& box) noexcept { return box.minY > 10; }

// Convenience: the geometric portion of createPiece — the candidate box and
// whether it passes isOkBox. (The `findCollisionPiece(box) == null` and the piece
// allocation, both stateful/world-bound, are intentionally excluded.)
struct StrongholdPieceResult {
    BoundingBox box{};
    bool okBox = false;
    constexpr bool operator==(const StrongholdPieceResult&) const = default;
};

inline constexpr StrongholdPieceResult strongholdCreatePieceGeometry(
    StrongholdPiece type, int32_t footX, int32_t footY, int32_t footZ, Direction direction) noexcept {
    BoundingBox box = strongholdPieceBox(type, footX, footY, footZ, direction);
    return {box, strongholdIsOkBox(box)};
}

} // namespace mc::levelgen::structure::structures

#pragma once

// 1:1 C++ port of the PURE box-construction + validity layer of
//   net.minecraft.world.level.levelgen.structure.structures.NetherFortressPieces
//   (decompiled Minecraft 26.1.2).
//
// Every NetherFortressPieces.<X>.createPiece(...) static factory builds its
// candidate BoundingBox the SAME way:
//
//     BoundingBox box = BoundingBox.orientBox(footX, footY, footZ,
//                                             <offX,offY,offZ>, <width,height,depth>,
//                                             direction);
//     return isOkBox(box) && accessor.findCollisionPiece(box) == null
//          ? new <X>(...) : null;
//
// The per-piece-type constants {offX,offY,offZ,width,height,depth} are the
// load-bearing data — they decide the footprint and clearance of each fortress
// piece. This header ports exactly that pure layer:
//   * the box geometry  = BoundingBox.orientBox(...) (reused from BoundingBox.h),
//   * the validity gate  = NetherFortressPieces.NetherBridgePiece.isOkBox(box)
//        == `box.minY() > 10`                       (NetherFortressPieces.java:1439-1441),
// for every concrete piece type, indexed by the exact orientBox(...) args each
// REAL createPiece passes.
//
// What is DELIBERATELY NOT ported here (and why it is still pure to gate the
// rest): the trailing `accessor.findCollisionPiece(box) == null` is a stateful
// lookup against the in-progress piece list (a StructurePieceAccessor) and the
// `new <X>(...)` allocation is a world-bound object — neither is part of this
// geometry layer. The `RandomSource random` that several createPiece overloads
// accept (BridgeStraight/BridgeCrossing-start/BridgeEndFiller/CastleEntrance/
// CastleSmallCorridor*) is NEVER consumed before the box is built — it is only
// forwarded into the piece constructor — so the box + isOkBox result is a pure
// function of (footX, footY, footZ, direction) and the piece type. NO world
// writes, NO RandomSource draws, NO registry/datapack.
//
// 1:1 TRAPS faithfully reproduced (all inherited from BoundingBox.orientBox /
// the BoundingBox ctor, which this header reuses verbatim from BoundingBox.h):
//   * orientBox corner arithmetic wraps two's-complement (Java int overflow);
//     BoundingBox.h routes every +/- through uint32_t to avoid C++ signed-overflow
//     UB at -O2 near Integer.MIN/MAX_VALUE.
//   * orientBox switches on the FOUR horizontals only; the SOUTH branch is the
//     `default` (so DOWN/UP, were they ever passed, fall through to SOUTH). Real
//     fortress generation only ever supplies a horizontal Direction.
//   * the BoundingBox ctor SWAPS inverted axes — for negative-offset pieces the
//     raw min/max can invert and Java silently re-sorts them; we keep that swap.
//   * isOkBox compares the FINAL (post-swap, post-orient) minY strictly `> 10`.
//
// Certified byte-exact by nether_fortress_piece_box_parity
// (tools/NetherFortressPieceBoxParity.java), which drives the REAL
// BoundingBox.orientBox + NetherFortressPieces.*.isOkBox via reflection.

#include "../BoundingBox.h"

#include <cstdint>

namespace mc::levelgen::structure::structures {

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;

// The concrete NetherFortressPieces piece types whose createPiece(...) builds a
// box via orientBox. Ordered as they appear in NetherFortressPieces.java.
enum class NetherFortressPiece : int32_t {
    BridgeCrossing = 0,                  // .java:131  orientBox(-8,-3,0, 19,10,19)
    BridgeEndFiller,                     // .java:211  orientBox(-1,-3,0,  5,10, 8)
    BridgeStraight,                      // .java:289  orientBox(-1,-3,0,  5,10,19)
    CastleCorridorStairsPiece,           // .java:357  orientBox(-1,-7,0,  5,14,10)
    CastleCorridorTBalconyPiece,         // .java:436  orientBox(-3, 0,0,  9, 7, 9)
    CastleEntrance,                      // .java:516  orientBox(-5,-3,0, 13,14,13)
    CastleSmallCorridorCrossingPiece,    // .java:638  orientBox(-1, 0,0,  5, 7, 5)
    CastleSmallCorridorLeftTurnPiece,    // .java:707  orientBox(-1, 0,0,  5, 7, 5)
    CastleSmallCorridorPiece,            // .java:771  orientBox(-1, 0,0,  5, 7, 5)
    CastleSmallCorridorRightTurnPiece,   // .java:843  orientBox(-1, 0,0,  5, 7, 5)
    CastleStalkRoom,                     // .java:908  orientBox(-5,-3,0, 13,14,13)
    MonsterThrone,                       // .java:1070 orientBox(-2, 0,0,  7, 8, 9)
    RoomCrossing,                        // .java:1497 orientBox(-2, 0,0,  7, 9, 7)
    StairsRoom                           // .java:1562 orientBox(-2, 0,0,  7,11, 7)
};

// The orientBox(...) arguments (offX,offY,offZ,width,height,depth) each concrete
// createPiece passes — copied verbatim from NetherFortressPieces.java.
struct NetherFortressBoxSpec {
    int32_t offX, offY, offZ;
    int32_t width, height, depth;
};

// NetherFortressPieces.<type>.createPiece orientBox constants — verbatim.
inline constexpr NetherFortressBoxSpec netherFortressBoxSpec(NetherFortressPiece type) noexcept {
    switch (type) {
        case NetherFortressPiece::BridgeCrossing:                return {-8, -3, 0, 19, 10, 19}; // .java:131
        case NetherFortressPiece::BridgeEndFiller:               return {-1, -3, 0,  5, 10,  8}; // .java:211
        case NetherFortressPiece::BridgeStraight:                return {-1, -3, 0,  5, 10, 19}; // .java:289
        case NetherFortressPiece::CastleCorridorStairsPiece:     return {-1, -7, 0,  5, 14, 10}; // .java:357
        case NetherFortressPiece::CastleCorridorTBalconyPiece:   return {-3,  0, 0,  9,  7,  9}; // .java:436
        case NetherFortressPiece::CastleEntrance:                return {-5, -3, 0, 13, 14, 13}; // .java:516
        case NetherFortressPiece::CastleSmallCorridorCrossingPiece: return {-1, 0, 0, 5, 7, 5};  // .java:638
        case NetherFortressPiece::CastleSmallCorridorLeftTurnPiece:  return {-1, 0, 0, 5, 7, 5};  // .java:707
        case NetherFortressPiece::CastleSmallCorridorPiece:         return {-1, 0, 0, 5, 7, 5};  // .java:771
        case NetherFortressPiece::CastleSmallCorridorRightTurnPiece: return {-1, 0, 0, 5, 7, 5};  // .java:843
        case NetherFortressPiece::CastleStalkRoom:               return {-5, -3, 0, 13, 14, 13}; // .java:908
        case NetherFortressPiece::MonsterThrone:                 return {-2,  0, 0,  7,  8,  9}; // .java:1070
        case NetherFortressPiece::RoomCrossing:                  return {-2,  0, 0,  7,  9,  7}; // .java:1497
        case NetherFortressPiece::StairsRoom:                    return {-2,  0, 0,  7, 11,  7}; // .java:1562
    }
    return {0, 0, 0, 0, 0, 0}; // unreachable; keeps the compiler happy
}

// BoundingBox built by NetherFortressPieces.<type>.createPiece for the given
// foot position + facing. Pure: identical to the REAL
//   BoundingBox.orientBox(footX, footY, footZ, off.., dim.., direction).
inline constexpr BoundingBox netherFortressPieceBox(NetherFortressPiece type, int32_t footX, int32_t footY,
                                                    int32_t footZ, Direction direction) noexcept {
    NetherFortressBoxSpec s = netherFortressBoxSpec(type);
    return BoundingBox::orientBox(footX, footY, footZ, s.offX, s.offY, s.offZ, s.width, s.height, s.depth,
                                  direction);
}

// NetherFortressPieces.NetherBridgePiece.isOkBox(BoundingBox) —
// NetherFortressPieces.java:1439-1441: `return box.minY() > 10;`. (Post-swap
// minY, i.e. the actual minimum-Y field of the constructed box.)
inline constexpr bool netherFortressIsOkBox(const BoundingBox& box) noexcept { return box.minY > 10; }

// Convenience: the geometric portion of createPiece — the candidate box and
// whether it passes isOkBox. (The `findCollisionPiece(box) == null` and the
// piece allocation, both stateful/world-bound, are intentionally excluded.)
struct NetherFortressPieceResult {
    BoundingBox box{};
    bool okBox = false;
    constexpr bool operator==(const NetherFortressPieceResult&) const = default;
};

inline constexpr NetherFortressPieceResult netherFortressCreatePieceGeometry(
    NetherFortressPiece type, int32_t footX, int32_t footY, int32_t footZ, Direction direction) noexcept {
    BoundingBox box = netherFortressPieceBox(type, footX, footY, footZ, direction);
    return {box, netherFortressIsOkBox(box)};
}

} // namespace mc::levelgen::structure::structures

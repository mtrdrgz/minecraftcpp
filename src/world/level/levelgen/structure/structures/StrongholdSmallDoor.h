#pragma once

// 1:1 port of the PURE, RNG-driven small-door-type selector in
//   net.minecraft.world.level.levelgen.structure.structures
//     .StrongholdPieces.StrongholdPiece.randomSmallDoor(RandomSource)
//   [StrongholdPieces.java:1540-1554].
//
// Every concrete Stronghold piece (Straight, RoomCrossing, FiveCrossing,
// PrisonHall, LeftTurn/RightTurn, StairsDown, FillerCorridor's neighbours, ...)
// chooses its entry doorway by calling this one shared instance method in its
// constructor:
//
//   protected StrongholdPiece.SmallDoorType randomSmallDoor(RandomSource random) {
//      int selection = random.nextInt(5);
//      switch (selection) {
//         case 0:
//         case 1:
//         default:  return SmallDoorType.OPENING;
//         case 2:   return SmallDoorType.WOOD_DOOR;
//         case 3:   return SmallDoorType.GRATES;
//         case 4:   return SmallDoorType.IRON_DOOR;
//      }
//   }
//
// It is a pure function of the RandomSource draw sequence: ONE nextInt(5) draw,
// mapped to a SmallDoorType by the switch. NO world reads/writes, NO registries,
// NO datapacks, NO BoundingBox — just the draw and the branch.
//
// This is DISTINCT from the HARD-AVOIDed Stronghold layer
// (findPieceBox / createPiece / SmoothStoneSelector): randomSmallDoor decides a
// doorway STYLE, not a piece footprint, and never touches the piece accessor.
//
// 1:1 TRAP faithfully reproduced — the Java switch FALL-THROUGH: labels `case 0`,
// `case 1` and `default` all share the OPENING arm, so selection==0, 1, and any
// out-of-range value (which nextInt(5) never produces, but the default arm still
// makes the mapping total) ALL map to OPENING. Only 2/3/4 diverge. A naive
// `selection-1`-style table would mis-handle 0 vs 1; we keep the exact mapping.
//
// SmallDoorType is declared (StrongholdPieces.java:1740-1744) as:
//   OPENING=0, WOOD_DOOR=1, GRATES=2, IRON_DOOR=3   (enum declaration order).
// We expose those exact ordinals so the emitted code is genuine vanilla.
//
// Reuses the already-certified, header-only RandomSource.h (LegacyRandomSource,
// the production type behind RandomSource.create(seed) used by every Stronghold
// piece constructor) for nextInt(int).
//
// Certified byte-exact by stronghold_small_door_parity
// (tools/StrongholdSmallDoorParity.java drives the REAL
//  StrongholdPiece.randomSmallDoor via reflection on an Unsafe-allocated
//  concrete piece instance).

#include <cstdint>

#include "world/level/levelgen/RandomSource.h"

namespace mc::levelgen::structure::structures {

// StrongholdPieces.StrongholdPiece.SmallDoorType — enum declaration order
// (StrongholdPieces.java:1740-1744). Ordinals are load-bearing: they are the
// values persisted by addAdditionalSaveData and re-read by the (CompoundTag)
// constructors, so the C++ port mirrors them exactly.
enum class SmallDoorType : int32_t {
    OPENING = 0,
    WOOD_DOOR = 1,
    GRATES = 2,
    IRON_DOOR = 3
};

// StrongholdPiece.randomSmallDoor(RandomSource) — StrongholdPieces.java:1540-1554.
//
// Draws ONE nextInt(5) and maps it via the exact switch (with the 0/1/default
// fall-through to OPENING). `random` is mutated by the single draw, exactly as
// in Java, so callers that draw further values stay in lock-step.
inline SmallDoorType randomSmallDoor(mc::levelgen::RandomSource& random) {
    int32_t selection = random.nextInt(5);
    switch (selection) {
        case 2:
            return SmallDoorType::WOOD_DOOR;
        case 3:
            return SmallDoorType::GRATES;
        case 4:
            return SmallDoorType::IRON_DOOR;
        case 0:
        case 1:
        default:
            return SmallDoorType::OPENING;
    }
}

} // namespace mc::levelgen::structure::structures

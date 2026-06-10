#pragma once
#include <cstdint>
#include <cmath>
#include "world/phys/Direction.h"

// ---------------------------------------------------------------------------
// Port of the horizontal / yaw (2D) helpers of net/minecraft/core/Direction.java
// (Minecraft Java Edition 26.1.2). The 3D enum, axis helpers, getOpposite,
// getApproximateNearest etc. already live in world/phys/Direction.h; this header
// adds ONLY the 2D-data / yaw surface and reuses mc::Direction from there.
//
// Java enum constants carry a `data2d` field (Direction.java:33-38, the 3rd ctor
// arg): DOWN=-1, UP=-1, NORTH=2, SOUTH=0, WEST=1, EAST=3.
//
// BY_2D_DATA (Direction.java:61-64) = the horizontal directions (axis X or Z)
// sorted ascending by data2d  ->  [SOUTH(0), WEST(1), NORTH(2), EAST(3)].
//
// Ported:
//   get2DDataValue()          Direction.java:159-161
//   from2DDataValue(int)      Direction.java:287-289
//   getYRot(Direction)        Direction.java:134-142
//   fromYRot(double)          Direction.java:291-293
//   toYRot()                  Direction.java:303-305
//   2D getOpposite            Direction.java:175-177 (horizontal subset)
//   BY_2D_DATA ordering       Direction.java:61-64
// ---------------------------------------------------------------------------

namespace mc {

// Java: Direction.data2d (3rd ctor arg, Direction.java:33-38).
//   DOWN=-1, UP=-1, NORTH=2, SOUTH=0, WEST=1, EAST=3.
constexpr int direction2DGet2DDataValue(Direction d) noexcept {
    switch (d) {
        case Direction::SOUTH: return 0;
        case Direction::WEST:  return 1;
        case Direction::NORTH: return 2;
        case Direction::EAST:  return 3;
        default:               return -1; // DOWN, UP (vertical, data2d=-1)
    }
}

// Java: Direction.BY_2D_DATA (Direction.java:61-64) — horizontal directions
// sorted by data2d: SOUTH(0), WEST(1), NORTH(2), EAST(3).
inline constexpr Direction DIRECTION_BY_2D_DATA[4] = {
    Direction::SOUTH, Direction::WEST, Direction::NORTH, Direction::EAST,
};

// Java: Mth.floor(double) = (int)Math.floor(v)  (Mth.java:65-67).
constexpr int direction2DMthFloor(double v) noexcept {
    return static_cast<int>(std::floor(v));
}

// Java: Mth.abs(int) = Math.abs(v)  (Mth.java:77-79).
constexpr int direction2DMthAbs(int v) noexcept {
    return v < 0 ? -v : v;
}

// Java: Direction.from2DDataValue(int) = BY_2D_DATA[Mth.abs(data % 4)]
// (Direction.java:287-289). BY_2D_DATA.length == 4.
constexpr Direction direction2DFrom2DDataValue(int data) noexcept {
    // Java % is truncating-toward-zero, same as C++ % for int.
    return DIRECTION_BY_2D_DATA[direction2DMthAbs(data % 4)];
}

// Java: Direction.getYRot(Direction) (Direction.java:134-142).
//   NORTH -> 180, SOUTH -> 0, WEST -> 90, EAST -> -90;
//   vertical (DOWN/UP) throws IllegalStateException. We mirror the throw as a
//   hard no-op: callers must only pass horizontal directions. Returns the float
//   exactly as Java's literals (180.0F / 0.0F / 90.0F / -90.0F).
constexpr float direction2DGetYRot(Direction direction) noexcept {
    switch (direction) {
        case Direction::NORTH: return 180.0f;
        case Direction::SOUTH: return 0.0f;
        case Direction::WEST:  return 90.0f;
        case Direction::EAST:  return -90.0f;
        default:               return 0.0f; // DOWN/UP — Java throws; never valid input
    }
}

// Java: Direction.fromYRot(double) = from2DDataValue(Mth.floor(yRot/90.0+0.5) & 3)
// (Direction.java:291-293).
constexpr Direction direction2DFromYRot(double yRot) noexcept {
    return direction2DFrom2DDataValue(direction2DMthFloor(yRot / 90.0 + 0.5) & 3);
}

// Java: Direction.toYRot() = (data2d & 3) * 90  (Direction.java:303-305).
// Returns a float (Java widens the int product to float on return).
constexpr float direction2DToYRot(Direction d) noexcept {
    return static_cast<float>((direction2DGet2DDataValue(d) & 3) * 90);
}

// Java: Direction.getOpposite() for the horizontal subset
// (from3DDataValue(oppositeIndex), Direction.java:175-177):
//   SOUTH<->NORTH, WEST<->EAST. Reuses the certified 3D opposite for parity.
constexpr Direction direction2DGetOpposite(Direction d) noexcept {
    return directionOpposite(d);
}

} // namespace mc

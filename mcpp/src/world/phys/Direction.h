#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Port of the subset of net/minecraft/core/Direction.java and
// net/minecraft/core/AxisCycle.java (Minecraft Java Edition 26.1.2) needed by
// the world/phys voxel-shape + collision port. Lives under world/phys/ until
// the full net.minecraft.core port lands.
//
// Enum ordinals MATCH JAVA EXACTLY (they are exchanged with parity TSVs):
//   Direction: DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5  (Direction.java:33-38)
//   Direction.Axis: X=0, Y=1, Z=2                              (Direction.java:402-480)
//   AxisCycle: NONE=0, FORWARD=1, BACKWARD=2                   (AxisCycle.java:3-66)
//
// Ported here:
//   Direction.Axis.choose(int/double x, y, z)            Direction.java:539-541
//   Direction.AxisDirection.getStep()                    Direction.java:546-560
//   Direction.getAxis()/getAxisDirection()/getOpposite() Direction.java:33-38,175-177
//   Direction.fromAxisAndDirection(axis, direction)      Direction.java:295-301
//   Direction.getApproximateNearest(double dx, dy, dz)   Direction.java:311-328
//   AxisCycle.cycle(int x, int y, int z, axis)           AxisCycle.java:6-8,27-29,48-50
//   AxisCycle.cycle(axis) / inverse() / between(from,to) AxisCycle.java:16-23,37-44,58-65,79-81
//
// NOT ported (unneeded by shapes; hard-absent, not stubbed): codecs, 2D data
// values, rotations (getClockWise etc.), orderedByNearest, getNearest,
// getUnitVec3 and the rest of the Direction surface.
// ---------------------------------------------------------------------------

namespace mc {

// Java: Direction.Axis (ordinals X=0, Y=1, Z=2 — Direction.java:402-480).
enum class Axis : int32_t { X = 0, Y = 1, Z = 2 };

inline constexpr Axis AXIS_VALUES[3] = {Axis::X, Axis::Y, Axis::Z}; // Direction.Axis.VALUES

// Java: Direction.Axis.choose(x, y, z) — Direction.java:405-417 (X), 431-438 (Y), 457-464 (Z).
template <typename T>
constexpr T axisChoose(Axis axis, T x, T y, T z) noexcept {
    return axis == Axis::X ? x : (axis == Axis::Y ? y : z);
}

// Java: Direction.AxisDirection — Direction.java:546-560.
enum class AxisDirection : int32_t { POSITIVE = 0, NEGATIVE = 1 };

constexpr int axisDirectionStep(AxisDirection d) noexcept {
    return d == AxisDirection::POSITIVE ? 1 : -1; // Direction.java:547-548 (step field)
}

// Java: Direction — ordinal order DOWN, UP, NORTH, SOUTH, WEST, EAST (Direction.java:33-38).
enum class Direction : int32_t { DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5 };

inline constexpr Direction DIRECTION_VALUES[6] = {
    Direction::DOWN, Direction::UP, Direction::NORTH,
    Direction::SOUTH, Direction::WEST, Direction::EAST,
};

// Per-direction unit normals — the Vec3i ctor args at Direction.java:33-38.
inline constexpr int DIRECTION_NORMAL[6][3] = {
    {0, -1, 0},  // DOWN
    {0, 1, 0},   // UP
    {0, 0, -1},  // NORTH
    {0, 0, 1},   // SOUTH
    {-1, 0, 0},  // WEST
    {1, 0, 0},   // EAST
};

// Java: Direction.getAxis() — axis field per constant (Direction.java:33-38).
constexpr Axis directionAxis(Direction d) noexcept {
    switch (d) {
        case Direction::DOWN:
        case Direction::UP: return Axis::Y;
        case Direction::NORTH:
        case Direction::SOUTH: return Axis::Z;
        default: return Axis::X; // WEST, EAST
    }
}

// Java: Direction.getAxisDirection() — axisDirection field per constant (Direction.java:33-38).
constexpr AxisDirection directionAxisDirection(Direction d) noexcept {
    return (d == Direction::UP || d == Direction::SOUTH || d == Direction::EAST)
               ? AxisDirection::POSITIVE
               : AxisDirection::NEGATIVE;
}

// Java: Direction.getOpposite() — from3DDataValue(oppositeIndex) (Direction.java:175-177);
// oppositeIndex pairs from the constants: DOWN<->UP, NORTH<->SOUTH, WEST<->EAST.
constexpr Direction directionOpposite(Direction d) noexcept {
    switch (d) {
        case Direction::DOWN: return Direction::UP;
        case Direction::UP: return Direction::DOWN;
        case Direction::NORTH: return Direction::SOUTH;
        case Direction::SOUTH: return Direction::NORTH;
        case Direction::WEST: return Direction::EAST;
        default: return Direction::WEST; // EAST
    }
}

// Java: Direction.fromAxisAndDirection(axis, direction) — Direction.java:295-301.
constexpr Direction directionFromAxisAndDirection(Axis axis, AxisDirection direction) noexcept {
    switch (axis) {
        case Axis::X: return direction == AxisDirection::POSITIVE ? Direction::EAST : Direction::WEST;
        case Axis::Y: return direction == AxisDirection::POSITIVE ? Direction::UP : Direction::DOWN;
        default: return direction == AxisDirection::POSITIVE ? Direction::SOUTH : Direction::NORTH;
    }
}

// Java: Direction.getApproximateNearest(double, double, double) — Direction.java:311-313
// casts the doubles to FLOAT and delegates to the float overload (Direction.java:315-328).
// CRITICAL Java detail: `float highestDot = Float.MIN_VALUE;` — Float.MIN_VALUE is the
// smallest POSITIVE float (1.4e-45, i.e. denorm_min), NOT a "most negative" sentinel.
// A zero vector therefore beats nothing and the NORTH initial value is returned.
inline Direction directionGetApproximateNearest(double dxd, double dyd, double dzd) noexcept {
    const float dx = static_cast<float>(dxd);
    const float dy = static_cast<float>(dyd);
    const float dz = static_cast<float>(dzd);
    Direction result = Direction::NORTH;
    float highestDot = 1.401298464324817e-45f; // Java Float.MIN_VALUE (denorm_min)
    for (int i = 0; i < 6; ++i) {
        const Direction direction = DIRECTION_VALUES[i];
        // Java: dx * normal.getX() + dy * normal.getY() + dz * normal.getZ() (float math)
        const float dot = dx * static_cast<float>(DIRECTION_NORMAL[i][0])
                        + dy * static_cast<float>(DIRECTION_NORMAL[i][1])
                        + dz * static_cast<float>(DIRECTION_NORMAL[i][2]);
        if (dot > highestDot) {
            highestDot = dot;
            result = direction;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Java: net/minecraft/core/AxisCycle.java
// ---------------------------------------------------------------------------

enum class AxisCycle : int32_t { NONE = 0, FORWARD = 1, BACKWARD = 2 };

// Java: Math.floorMod(int, int) (used by AxisCycle.java:38,59,80).
constexpr int javaFloorModInt(int x, int y) noexcept {
    int mod = x % y;
    if ((mod ^ y) < 0 && mod != 0) mod += y;
    return mod;
}

// Java: AxisCycle.cycle(int x, int y, int z, Direction.Axis axis) —
// NONE: choose(x,y,z) (AxisCycle.java:6-8); FORWARD: choose(z,x,y) (27-29);
// BACKWARD: choose(y,z,x) (48-50). Same for the double overload.
template <typename T>
constexpr T axisCycleChoose(AxisCycle cycle, T x, T y, T z, Axis axis) noexcept {
    switch (cycle) {
        case AxisCycle::NONE: return axisChoose(axis, x, y, z);
        case AxisCycle::FORWARD: return axisChoose(axis, z, x, y);
        default: return axisChoose(axis, y, z, x); // BACKWARD
    }
}

// Java: AxisCycle.cycle(Direction.Axis) — AxisCycle.java:16-18 (NONE),
// 37-39 (FORWARD: AXIS_VALUES[floorMod(ordinal+1, 3)]), 58-60 (BACKWARD: -1).
constexpr Axis axisCycleAxis(AxisCycle cycle, Axis axis) noexcept {
    switch (cycle) {
        case AxisCycle::NONE: return axis;
        case AxisCycle::FORWARD:
            return AXIS_VALUES[javaFloorModInt(static_cast<int>(axis) + 1, 3)];
        default:
            return AXIS_VALUES[javaFloorModInt(static_cast<int>(axis) - 1, 3)];
    }
}

// Java: AxisCycle.inverse() — AxisCycle.java:21-23,42-44,63-65.
constexpr AxisCycle axisCycleInverse(AxisCycle cycle) noexcept {
    switch (cycle) {
        case AxisCycle::NONE: return AxisCycle::NONE;
        case AxisCycle::FORWARD: return AxisCycle::BACKWARD;
        default: return AxisCycle::FORWARD;
    }
}

// Java: AxisCycle.between(from, to) = VALUES[floorMod(to.ordinal() - from.ordinal(), 3)]
// (AxisCycle.java:79-81).
constexpr AxisCycle axisCycleBetween(Axis from, Axis to) noexcept {
    return static_cast<AxisCycle>(javaFloorModInt(static_cast<int>(to) - static_cast<int>(from), 3));
}

} // namespace mc

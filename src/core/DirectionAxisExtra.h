#pragma once
#include <cstdint>
#include <cstdlib>

// ---------------------------------------------------------------------------
// NEW, self-contained port of the still-unported "extra" surface of
// net/minecraft/core/Direction.java (Minecraft Java Edition 26.1.2):
//
//   Direction.Axis.choose(int/double x, y, z)          Direction.java:405-417 (X),
//                                                       431-438 (Y), 457-464 (Z)
//   Direction.Axis.isHorizontal()                       Direction.java:502-504
//   Direction.Axis.isVertical()                         Direction.java:498-500
//   Direction.Axis.getPositive()/getNegative()          Direction.java:419-426 (X),
//                                                       445-452 (Y), 471-478 (Z)
//   Direction.getAxisDirection()                        Direction.java:163-165, 33-38
//   Direction.Plane.HORIZONTAL/VERTICAL faces+axes      Direction.java:576-616
//   Direction.Plane.iterator()/length()                 Direction.java:600-615
//   Direction.getNearest(int x, int y, int z, orElse)   Direction.java:334-348
//
// This header is intentionally NEW and standalone (the existing read-only
// world/phys/Direction.h is NOT edited); it lives in its own namespace
// (mc::core_dirextra) so the two ports never collide. Enum ordinals MATCH
// JAVA EXACTLY (exchanged with the parity TSV):
//   Direction: DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5  (Direction.java:33-38)
//   Direction.Axis: X=0, Y=1, Z=2                              (Direction.java:402-480)
//   Direction.AxisDirection: POSITIVE, NEGATIVE                (Direction.java:546-548)
//
// "orElse == null" in getNearest is encoded as the sentinel ordinal -1.
// ---------------------------------------------------------------------------

namespace mc::core_dirextra {

// Java: Direction (ordinal order DOWN, UP, NORTH, SOUTH, WEST, EAST — Direction.java:33-38).
enum class Direction : int32_t { DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5 };

// Java: Direction.Axis (ordinals X=0, Y=1, Z=2 — Direction.java:402-480).
enum class Axis : int32_t { X = 0, Y = 1, Z = 2 };

// Java: Direction.AxisDirection (POSITIVE, NEGATIVE — Direction.java:546-548).
enum class AxisDirection : int32_t { POSITIVE = 0, NEGATIVE = 1 };

// Java: Direction.Plane (HORIZONTAL, VERTICAL — Direction.java:576-578).
enum class Plane : int32_t { HORIZONTAL = 0, VERTICAL = 1 };

// ---- Direction.Axis.choose(x, y, z) ---------------------------------------
// X returns x (Direction.java:405-417), Y returns y (431-438), Z returns z (457-464).
template <typename T>
constexpr T axisChoose(Axis axis, T x, T y, T z) noexcept {
    switch (axis) {
        case Axis::X: return x;
        case Axis::Y: return y;
        default:      return z; // Z
    }
}

// ---- Direction.Axis.isVertical()/isHorizontal() ---------------------------
// Direction.java:498-500: isVertical() == (this == Y).
constexpr bool axisIsVertical(Axis axis) noexcept { return axis == Axis::Y; }
// Direction.java:502-504: isHorizontal() == (this == X || this == Z).
constexpr bool axisIsHorizontal(Axis axis) noexcept { return axis == Axis::X || axis == Axis::Z; }

// ---- Direction.Axis.getPositive()/getNegative() ---------------------------
// X -> EAST/WEST (Direction.java:419-426); Y -> UP/DOWN (445-452); Z -> SOUTH/NORTH (471-478).
constexpr Direction axisGetPositive(Axis axis) noexcept {
    switch (axis) {
        case Axis::X: return Direction::EAST;
        case Axis::Y: return Direction::UP;
        default:      return Direction::SOUTH; // Z
    }
}
constexpr Direction axisGetNegative(Axis axis) noexcept {
    switch (axis) {
        case Axis::X: return Direction::WEST;
        case Axis::Y: return Direction::DOWN;
        default:      return Direction::NORTH; // Z
    }
}

// Java: Direction.Axis.getDirections() = {getPositive(), getNegative()} (Direction.java:510-512).
// (length 2 array; index 0 = positive, 1 = negative.)
constexpr Direction axisGetDirection(Axis axis, int index) noexcept {
    return index == 0 ? axisGetPositive(axis) : axisGetNegative(axis);
}

// ---- Direction.getAxis() — axis field per constant (Direction.java:33-38) --
constexpr Axis directionAxis(Direction d) noexcept {
    switch (d) {
        case Direction::DOWN:
        case Direction::UP:    return Axis::Y;
        case Direction::NORTH:
        case Direction::SOUTH: return Axis::Z;
        default:               return Axis::X; // WEST, EAST
    }
}

// ---- Direction.getAxisDirection() — axisDirection field (Direction.java:163-165, 33-38) ----
// UP/SOUTH/EAST are POSITIVE; DOWN/NORTH/WEST are NEGATIVE (per the constant declarations).
constexpr AxisDirection directionAxisDirection(Direction d) noexcept {
    return (d == Direction::UP || d == Direction::SOUTH || d == Direction::EAST)
               ? AxisDirection::POSITIVE
               : AxisDirection::NEGATIVE;
}

// ---- Direction.Axis.getPlane() — Direction.java:527-532 -------------------
// X,Z -> HORIZONTAL; Y -> VERTICAL.
constexpr Plane axisGetPlane(Axis axis) noexcept {
    return axis == Axis::Y ? Plane::VERTICAL : Plane::HORIZONTAL;
}

// ---- Direction.Plane — faces + axes arrays (Direction.java:577-578) -------
// HORIZONTAL faces = {NORTH, EAST, SOUTH, WEST}; axes = {X, Z}.
// VERTICAL   faces = {UP, DOWN};                 axes = {Y}.
inline constexpr Direction PLANE_HORIZONTAL_FACES[4] = {
    Direction::NORTH, Direction::EAST, Direction::SOUTH, Direction::WEST};
inline constexpr Direction PLANE_VERTICAL_FACES[2] = {Direction::UP, Direction::DOWN};
inline constexpr Axis PLANE_HORIZONTAL_AXES[2] = {Axis::X, Axis::Z};
inline constexpr Axis PLANE_VERTICAL_AXES[1] = {Axis::Y};

// Java: Direction.Plane.length() = faces.length (Direction.java:613-615).
constexpr int planeLength(Plane plane) noexcept {
    return plane == Plane::HORIZONTAL ? 4 : 2;
}

// Java: Direction.Plane.iterator() iterates faces in declaration order
// (Direction.java:600-603 — Iterators.forArray(this.faces)). Returns the i-th face.
constexpr Direction planeFace(Plane plane, int i) noexcept {
    if (plane == Plane::HORIZONTAL) return PLANE_HORIZONTAL_FACES[i];
    return PLANE_VERTICAL_FACES[i];
}

// Java: Direction.Plane.test(input) = input != null && input.getAxis().getPlane() == this
// (Direction.java:596-598). Caller encodes "null" by simply not calling test.
constexpr bool planeTest(Plane plane, Direction d) noexcept {
    return axisGetPlane(directionAxis(d)) == plane;
}

// Number of axes in this plane's axis[] array (Direction.java:577-578).
constexpr int planeAxisCount(Plane plane) noexcept {
    return plane == Plane::HORIZONTAL ? 2 : 1;
}
constexpr Axis planeAxis(Plane plane, int i) noexcept {
    if (plane == Plane::HORIZONTAL) return PLANE_HORIZONTAL_AXES[i];
    return PLANE_VERTICAL_AXES[i];
}

// ---- Direction.getNearest(int x, int y, int z, orElse) — Direction.java:334-348 ----
// absX/absY/absZ via Math.abs(int) (NOTE: Math.abs(INT_MIN) == INT_MIN; we mirror
// java.lang.Math.abs which simply returns the input when it is INT_MIN — same as
// std::abs is UB there, so we replicate Java's bit behaviour explicitly). The
// orElse fallback is passed in (use the sentinel -1 for Java null).
constexpr int javaAbsInt(int a) noexcept {
    // java.lang.Math.abs(int): return (a < 0) ? -a : a;  (-INT_MIN wraps to INT_MIN, matching Java)
    return a < 0 ? static_cast<int>(0u - static_cast<unsigned>(a)) : a;
}

// orElse is an int ordinal in [0,5], or -1 for "null". Returns the same encoding.
constexpr int directionGetNearest(int x, int y, int z, int orElse) noexcept {
    const int absX = javaAbsInt(x);
    const int absY = javaAbsInt(y);
    const int absZ = javaAbsInt(z);
    if (absX > absZ && absX > absY) {
        return static_cast<int>(x < 0 ? Direction::WEST : Direction::EAST);
    } else if (absZ > absX && absZ > absY) {
        return static_cast<int>(z < 0 ? Direction::NORTH : Direction::SOUTH);
    } else if (absY > absX && absY > absZ) {
        return static_cast<int>(y < 0 ? Direction::DOWN : Direction::UP);
    } else {
        return orElse;
    }
}

} // namespace mc::core_dirextra

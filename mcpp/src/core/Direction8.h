#pragma once
#include <array>
#include <cstdint>

#include "world/phys/Direction.h" // certified mc::Direction (+ DIRECTION_NORMAL)

// ---------------------------------------------------------------------------
// Port of net/minecraft/core/Direction8.java (Minecraft Java Edition 26.1.2).
//
// Direction8 is an 8-way (cardinal + diagonal) horizontal enum. Each constant
// is built from one or two net.minecraft.core.Direction cardinals; its `step`
// Vec3i is the COMPONENT-WISE SUM of those cardinals' normals, and getStepX()/
// getStepZ() expose that sum (Direction8.java:20-39). getDirections() returns
// the (immutable enum) Set of contributing cardinals (Direction8.java:29-31).
//
// Java declaration order (Direction8.java:8-15) — ordinals MUST match:
//   NORTH=0, NORTH_EAST=1, EAST=2, SOUTH_EAST=3,
//   SOUTH=4, SOUTH_WEST=5, WEST=6, NORTH_WEST=7
//
// The contributing cardinals (Direction8.java:8-15), using the certified
// mc::Direction ordinals (DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5):
//   NORTH      -> {NORTH}
//   NORTH_EAST -> {NORTH, EAST}
//   EAST       -> {EAST}
//   SOUTH_EAST -> {SOUTH, EAST}
//   SOUTH      -> {SOUTH}
//   SOUTH_WEST -> {SOUTH, WEST}
//   WEST       -> {WEST}
//   NORTH_WEST -> {NORTH, WEST}
//
// The `step` is accumulated in declaration order over the ctor varargs
// (Direction8.java:24-26). All contributing cardinals are horizontal, so the
// Y component of every step is 0; Java only exposes getStepX()/getStepZ()
// (there is no getStepY on Direction8). We expose the full Vec3i sum plus the
// two accessors Java offers, for completeness.
// ---------------------------------------------------------------------------

namespace mc {

// Java: Direction8 — declaration order (Direction8.java:8-15).
enum class Direction8 : int32_t {
    NORTH = 0,
    NORTH_EAST = 1,
    EAST = 2,
    SOUTH_EAST = 3,
    SOUTH = 4,
    SOUTH_WEST = 5,
    WEST = 6,
    NORTH_WEST = 7,
};

inline constexpr Direction8 DIRECTION8_VALUES[8] = {
    Direction8::NORTH,      Direction8::NORTH_EAST, Direction8::EAST,
    Direction8::SOUTH_EAST, Direction8::SOUTH,      Direction8::SOUTH_WEST,
    Direction8::WEST,       Direction8::NORTH_WEST,
};

// The contributing cardinals per Direction8 constant (Direction8.java:8-15).
// First entry is the primary cardinal; for diagonals the second entry is the
// secondary cardinal. Entries are listed in the SAME order as the Java ctor
// varargs so the step sum is accumulated identically.
inline constexpr Direction DIRECTION8_PARTS[8][2] = {
    {Direction::NORTH, Direction::NORTH}, // NORTH       -> {NORTH}
    {Direction::NORTH, Direction::EAST},  // NORTH_EAST  -> {NORTH, EAST}
    {Direction::EAST, Direction::EAST},   // EAST        -> {EAST}
    {Direction::SOUTH, Direction::EAST},  // SOUTH_EAST  -> {SOUTH, EAST}
    {Direction::SOUTH, Direction::SOUTH}, // SOUTH       -> {SOUTH}
    {Direction::SOUTH, Direction::WEST},  // SOUTH_WEST  -> {SOUTH, WEST}
    {Direction::WEST, Direction::WEST},   // WEST        -> {WEST}
    {Direction::NORTH, Direction::WEST},  // NORTH_WEST  -> {NORTH, WEST}
};

// Number of distinct cardinals contributing to a Direction8 (1 for cardinals,
// 2 for diagonals). Mirrors the size of the immutable enum set Direction8 holds.
constexpr int direction8PartCount(Direction8 d) noexcept {
    // Diagonals are the odd ordinals (NORTH_EAST=1, SOUTH_EAST=3, SOUTH_WEST=5,
    // NORTH_WEST=7); cardinals are the even ones.
    return (static_cast<int>(d) & 1) == 1 ? 2 : 1;
}

// Java: Direction8.getDirections() — true iff `cardinal` is in this Direction8's
// set (Direction8.java:29-31 returns the set; membership is what callers test).
constexpr bool direction8HasDirection(Direction8 d, Direction cardinal) noexcept {
    const int n = direction8PartCount(d);
    const int idx = static_cast<int>(d);
    for (int i = 0; i < n; ++i) {
        if (DIRECTION8_PARTS[idx][i] == cardinal) return true;
    }
    return false;
}

// Java: Direction8 ctor step accumulation (Direction8.java:22-26): start at
// (0,0,0) and add each contributing cardinal's normal component-wise.
constexpr int direction8Step(Direction8 d, int component) noexcept {
    // component: 0=x, 1=y, 2=z
    const int n = direction8PartCount(d);
    const int idx = static_cast<int>(d);
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        const int cardinalOrd = static_cast<int>(DIRECTION8_PARTS[idx][i]);
        sum += DIRECTION_NORMAL[cardinalOrd][component]; // certified normal table
    }
    return sum;
}

// Java: Direction8.getStepX() — step.getX() (Direction8.java:33-35).
constexpr int direction8GetStepX(Direction8 d) noexcept { return direction8Step(d, 0); }

// step.getY(): always 0 (only horizontal cardinals contribute). Java has no
// public getStepY on Direction8, but the underlying Vec3i carries it.
constexpr int direction8GetStepY(Direction8 d) noexcept { return direction8Step(d, 1); }

// Java: Direction8.getStepZ() — step.getZ() (Direction8.java:37-39).
constexpr int direction8GetStepZ(Direction8 d) noexcept { return direction8Step(d, 2); }

} // namespace mc

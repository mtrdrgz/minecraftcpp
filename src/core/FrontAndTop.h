#pragma once
#include <array>
#include <cstdint>
#include "world/phys/Direction.h"

// ---------------------------------------------------------------------------
// Port of net/minecraft/core/FrontAndTop.java (Minecraft Java Edition 26.1.2).
//
// An enum of (front, top) Direction combinations used by jigsaw/structure blocks.
// The Java enum declares exactly TWELVE constants (NOT 24 — only the perpendicular
// front/top pairs that actually occur are enumerated). Ordinals MATCH the Java
// declaration order exactly (FrontAndTop.java:7-18):
//   DOWN_EAST=0,  DOWN_NORTH=1, DOWN_SOUTH=2, DOWN_WEST=3,
//   UP_EAST=4,    UP_NORTH=5,   UP_SOUTH=6,   UP_WEST=7,
//   WEST_UP=8,    EAST_UP=9,    NORTH_UP=10,  SOUTH_UP=11
//
// Ported here (verbatim from the Java):
//   FrontAndTop.lookupKey(front, top)          FrontAndTop.java:30-32
//   FrontAndTop.fromFrontAndTop(front, top)    FrontAndTop.java:45-47 (via BY_TOP_FRONT)
//   FrontAndTop.front()                        FrontAndTop.java:49-51
//   FrontAndTop.top()                          FrontAndTop.java:53-55
//   getSerializedName()                        FrontAndTop.java:40-43 (the name field)
//
// NOT ported (no such members exist in this class): there is no data2D / value-by-id
// table on FrontAndTop. The StringRepresentable CODEC/keyable surface is not ported
// here (hard-absent), only the raw serialized-name string is exposed.
//
// Reuses world/phys/Direction.h for mc::Direction (DOWN=0..EAST=5).
// ---------------------------------------------------------------------------

namespace mc {

// Java: FrontAndTop enum constants, in declaration order (ordinals 0..11).
// (FrontAndTop.java:7-18)
enum class FrontAndTop : int32_t {
    DOWN_EAST = 0,
    DOWN_NORTH = 1,
    DOWN_SOUTH = 2,
    DOWN_WEST = 3,
    UP_EAST = 4,
    UP_NORTH = 5,
    UP_SOUTH = 6,
    UP_WEST = 7,
    WEST_UP = 8,
    EAST_UP = 9,
    NORTH_UP = 10,
    SOUTH_UP = 11,
};

// Java: FrontAndTop.NUM_DIRECTIONS = Direction.values().length (FrontAndTop.java:20).
inline constexpr int FRONT_AND_TOP_NUM_DIRECTIONS = 6;

inline constexpr int FRONT_AND_TOP_COUNT = 12;

// Per-constant (front, top) pair — the ctor args at FrontAndTop.java:7-18.
// Indexed by ordinal.
inline constexpr Direction FRONT_AND_TOP_FRONT[FRONT_AND_TOP_COUNT] = {
    Direction::DOWN,  // DOWN_EAST
    Direction::DOWN,  // DOWN_NORTH
    Direction::DOWN,  // DOWN_SOUTH
    Direction::DOWN,  // DOWN_WEST
    Direction::UP,    // UP_EAST
    Direction::UP,    // UP_NORTH
    Direction::UP,    // UP_SOUTH
    Direction::UP,    // UP_WEST
    Direction::WEST,  // WEST_UP
    Direction::EAST,  // EAST_UP
    Direction::NORTH, // NORTH_UP
    Direction::SOUTH, // SOUTH_UP
};

inline constexpr Direction FRONT_AND_TOP_TOP[FRONT_AND_TOP_COUNT] = {
    Direction::EAST,  // DOWN_EAST
    Direction::NORTH, // DOWN_NORTH
    Direction::SOUTH, // DOWN_SOUTH
    Direction::WEST,  // DOWN_WEST
    Direction::EAST,  // UP_EAST
    Direction::NORTH, // UP_NORTH
    Direction::SOUTH, // UP_SOUTH
    Direction::WEST,  // UP_WEST
    Direction::UP,    // WEST_UP
    Direction::UP,    // EAST_UP
    Direction::UP,    // NORTH_UP
    Direction::UP,    // SOUTH_UP
};

// Per-constant serialized name — the name field at FrontAndTop.java:7-18.
inline constexpr const char* FRONT_AND_TOP_NAME[FRONT_AND_TOP_COUNT] = {
    "down_east", "down_north", "down_south", "down_west",
    "up_east",   "up_north",   "up_south",   "up_west",
    "west_up",   "east_up",    "north_up",   "south_up",
};

// Java: FrontAndTop.front() — FrontAndTop.java:49-51.
constexpr Direction frontAndTopFront(FrontAndTop v) noexcept {
    return FRONT_AND_TOP_FRONT[static_cast<int>(v)];
}

// Java: FrontAndTop.top() — FrontAndTop.java:53-55.
constexpr Direction frontAndTopTop(FrontAndTop v) noexcept {
    return FRONT_AND_TOP_TOP[static_cast<int>(v)];
}

// Java: FrontAndTop.getSerializedName() — FrontAndTop.java:40-43.
constexpr const char* frontAndTopSerializedName(FrontAndTop v) noexcept {
    return FRONT_AND_TOP_NAME[static_cast<int>(v)];
}

// Java: FrontAndTop.lookupKey(front, top) = front.ordinal() * NUM_DIRECTIONS + top.ordinal()
// (FrontAndTop.java:30-32).
constexpr int frontAndTopLookupKey(Direction front, Direction top) noexcept {
    return static_cast<int>(front) * FRONT_AND_TOP_NUM_DIRECTIONS + static_cast<int>(top);
}

// Java: FrontAndTop.BY_TOP_FRONT — a NUM_DIRECTIONS*NUM_DIRECTIONS (=36) element array,
// default-filled with null, then populated for each enum constant at its lookupKey
// (FrontAndTop.java:21-25). We mirror it as a 36-element table of ordinals, where -1
// stands in for Java's null (front/top pairs not enumerated, e.g. front==top or a
// horizontal front with a horizontal top). Built verbatim: for each value,
// BY_TOP_FRONT[lookupKey(value.front, value.top)] = value.ordinal().
inline constexpr auto FRONT_AND_TOP_BY_TOP_FRONT = []() constexpr {
    std::array<int, 36> m{};
    for (int& e : m) e = -1; // Java null
    for (int i = 0; i < FRONT_AND_TOP_COUNT; ++i) {
        int key = static_cast<int>(FRONT_AND_TOP_FRONT[i]) * FRONT_AND_TOP_NUM_DIRECTIONS
                + static_cast<int>(FRONT_AND_TOP_TOP[i]);
        m[key] = i;
    }
    return m;
}();

// Java: FrontAndTop.fromFrontAndTop(front, top) — FrontAndTop.java:45-47.
// Returns the FrontAndTop ordinal as int, or -1 if Java would return null (NPE source
// in vanilla for invalid pairs; here we surface the absence rather than fabricate).
constexpr int frontAndTopFromFrontAndTopOrdinal(Direction front, Direction top) noexcept {
    return FRONT_AND_TOP_BY_TOP_FRONT[frontAndTopLookupKey(front, top)];
}

} // namespace mc

#pragma once

// 1:1 port of net.minecraft.world.level.pathfinder.PathType (26.1.2).
//
// PathType is a bounded Java enum: 27 constants, each carrying a `float malus`
// default cost passed to its constructor (PathType.java:4-30), plus the pure
// accessor getMalus() (PathType.java:38-40). There is NO registry/world/network
// coupling — every value below is transcribed VERBATIM from the Java source.
//
// The enum ORDER (and therefore each constant's ordinal()) and its name() are
// part of the contract: ordinal() is used as an index into malus arrays and
// name() is used for serialization elsewhere, so both are reproduced exactly.

#include <array>
#include <cstddef>
#include <string_view>

namespace mc::pathfinder {

// Declaration order == Java declaration order == ordinal(). DO NOT reorder.
enum class PathType : int {
    BLOCKED = 0,
    OPEN,
    WALKABLE,
    WALKABLE_DOOR,
    TRAPDOOR,
    POWDER_SNOW,
    ON_TOP_OF_POWDER_SNOW,
    FENCE,
    LAVA,
    WATER,
    WATER_BORDER,
    RAIL,
    UNPASSABLE_RAIL,
    FIRE_IN_NEIGHBOR,
    FIRE,
    DAMAGING_IN_NEIGHBOR,
    DAMAGING,
    DOOR_OPEN,
    DOOR_WOOD_CLOSED,
    DOOR_IRON_CLOSED,
    BREACH,
    LEAVES,
    STICKY_HONEY,
    COCOA,
    DAMAGE_CAUTIOUS,
    ON_TOP_OF_TRAPDOOR,
    BIG_MOBS_CLOSE_TO_DANGER,
};

// Number of enum constants (PathType.values().length).
inline constexpr std::size_t PATH_TYPE_COUNT = 27;

namespace detail {

// Per-constant default malus passed to PathType(final float defaultCost),
// indexed by ordinal — PathType.java:4-30. VERBATIM.
inline constexpr std::array<float, PATH_TYPE_COUNT> kPathTypeMalus = {
    -1.0F,  // BLOCKED
    0.0F,   // OPEN
    0.0F,   // WALKABLE
    0.0F,   // WALKABLE_DOOR
    0.0F,   // TRAPDOOR
    -1.0F,  // POWDER_SNOW
    0.0F,   // ON_TOP_OF_POWDER_SNOW
    -1.0F,  // FENCE
    -1.0F,  // LAVA
    8.0F,   // WATER
    8.0F,   // WATER_BORDER
    0.0F,   // RAIL
    -1.0F,  // UNPASSABLE_RAIL
    8.0F,   // FIRE_IN_NEIGHBOR
    16.0F,  // FIRE
    8.0F,   // DAMAGING_IN_NEIGHBOR
    -1.0F,  // DAMAGING
    0.0F,   // DOOR_OPEN
    -1.0F,  // DOOR_WOOD_CLOSED
    -1.0F,  // DOOR_IRON_CLOSED
    4.0F,   // BREACH
    -1.0F,  // LEAVES
    8.0F,   // STICKY_HONEY
    0.0F,   // COCOA
    0.0F,   // DAMAGE_CAUTIOUS
    0.0F,   // ON_TOP_OF_TRAPDOOR
    4.0F,   // BIG_MOBS_CLOSE_TO_DANGER
};

// Constant name() strings, indexed by ordinal — PathType.java:4-30. VERBATIM.
inline constexpr std::array<std::string_view, PATH_TYPE_COUNT> kPathTypeName = {
    "BLOCKED",
    "OPEN",
    "WALKABLE",
    "WALKABLE_DOOR",
    "TRAPDOOR",
    "POWDER_SNOW",
    "ON_TOP_OF_POWDER_SNOW",
    "FENCE",
    "LAVA",
    "WATER",
    "WATER_BORDER",
    "RAIL",
    "UNPASSABLE_RAIL",
    "FIRE_IN_NEIGHBOR",
    "FIRE",
    "DAMAGING_IN_NEIGHBOR",
    "DAMAGING",
    "DOOR_OPEN",
    "DOOR_WOOD_CLOSED",
    "DOOR_IRON_CLOSED",
    "BREACH",
    "LEAVES",
    "STICKY_HONEY",
    "COCOA",
    "DAMAGE_CAUTIOUS",
    "ON_TOP_OF_TRAPDOOR",
    "BIG_MOBS_CLOSE_TO_DANGER",
};

}  // namespace detail

// ordinal() — the underlying int value, which equals Java's enum ordinal.
inline constexpr int ordinal(PathType t) { return static_cast<int>(t); }

// getMalus() — PathType.java:38-40: returns this.malus (the constructor default).
inline constexpr float getMalus(PathType t) {
    return detail::kPathTypeMalus[static_cast<std::size_t>(t)];
}

// name() — Enum.name(): the constant's identifier string.
inline constexpr std::string_view name(PathType t) {
    return detail::kPathTypeName[static_cast<std::size_t>(t)];
}

}  // namespace mc::pathfinder

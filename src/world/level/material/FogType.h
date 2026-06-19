#pragma once

// 1:1 port of net.minecraft.world.level.material.FogType (Minecraft 26.1.2).
//
// Java source (26.1.2/src/net/minecraft/world/level/material/FogType.java):
//   public enum FogType {
//      LAVA,        // ordinal 0
//      WATER,       // ordinal 1
//      POWDER_SNOW, // ordinal 2
//      ATMOSPHERIC, // ordinal 3
//      NONE;        // ordinal 4
//   }
//
// A bare enum with no fields, constructors or methods. The only observable
// behaviour is the constant set, their declaration order (ordinals) and their
// names (Enum.name() / values()[i]). All of that is reproduced verbatim below.

#include <array>
#include <cstddef>
#include <string_view>

namespace mc::material {

// Underlying ordinal values match Java's enum declaration order exactly.
enum class FogType : int {
    LAVA = 0,
    WATER = 1,
    POWDER_SNOW = 2,
    ATMOSPHERIC = 3,
    NONE = 4,
};

// Number of constants in the enum (FogType.values().length).
inline constexpr int FOG_TYPE_COUNT = 5;

// FogType.values() — constants in declaration (ordinal) order.
inline constexpr std::array<FogType, FOG_TYPE_COUNT> FOG_TYPE_VALUES = {
    FogType::LAVA,
    FogType::WATER,
    FogType::POWDER_SNOW,
    FogType::ATMOSPHERIC,
    FogType::NONE,
};

// FogType ordinal (Enum.ordinal()).
inline constexpr int fogTypeOrdinal(FogType t) { return static_cast<int>(t); }

// FogType.values()[ordinal].
inline constexpr FogType fogTypeFromOrdinal(int ordinal) {
    return FOG_TYPE_VALUES[static_cast<std::size_t>(ordinal)];
}

// Enum.name() — the constant's declared identifier, verbatim.
inline constexpr std::string_view fogTypeName(FogType t) {
    switch (t) {
        case FogType::LAVA: return "LAVA";
        case FogType::WATER: return "WATER";
        case FogType::POWDER_SNOW: return "POWDER_SNOW";
        case FogType::ATMOSPHERIC: return "ATMOSPHERIC";
        case FogType::NONE: return "NONE";
    }
    return {};
}

}  // namespace mc::material

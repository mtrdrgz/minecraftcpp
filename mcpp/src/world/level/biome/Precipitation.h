#pragma once

// Port of the nested enum net.minecraft.world.level.biome.Biome.Precipitation
// (26.1.2). Verbatim from 26.1.2/src/net/minecraft/world/level/biome/Biome.java
// lines 370-386:
//
//   public enum Precipitation implements StringRepresentable {
//      NONE("none"),
//      RAIN("rain"),
//      SNOW("snow");
//      private final String name;
//      Precipitation(final String name) { this.name = name; }
//      @Override public String getSerializedName() { return this.name; }
//   }
//
// The existing engine header world/level/biome/Biome.h models the Biome data
// record but does NOT declare this nested enum, so it lives here on its own.
//
// ordinal() is the declaration order (NONE=0, RAIN=1, SNOW=2); name() is the
// constant identifier; getSerializedName() is the lowercase string passed to the
// constructor.

#include <cstddef>

namespace mc::biome {

// Declaration order matches Java enum ordinal() exactly.
enum class Precipitation : int {
    NONE = 0,
    RAIN = 1,
    SNOW = 2,
};

// Every declared constant, in ordinal order (mirrors Precipitation.values()).
inline constexpr Precipitation kPrecipitationValues[] = {
    Precipitation::NONE,
    Precipitation::RAIN,
    Precipitation::SNOW,
};
inline constexpr std::size_t kPrecipitationCount = 3;

// Enum.ordinal()
inline constexpr int precipitationOrdinal(Precipitation p) {
    return static_cast<int>(p);
}

// Enum.name() — the Java constant identifier.
inline constexpr const char* precipitationName(Precipitation p) {
    switch (p) {
        case Precipitation::NONE: return "NONE";
        case Precipitation::RAIN: return "RAIN";
        case Precipitation::SNOW: return "SNOW";
    }
    return "";
}

// StringRepresentable.getSerializedName() — the lowercase ctor argument.
inline constexpr const char* precipitationSerializedName(Precipitation p) {
    switch (p) {
        case Precipitation::NONE: return "none";
        case Precipitation::RAIN: return "rain";
        case Precipitation::SNOW: return "snow";
    }
    return "";
}

} // namespace mc::biome

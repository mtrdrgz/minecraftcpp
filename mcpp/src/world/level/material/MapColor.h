#pragma once

// 1:1 port of net.minecraft.world.level.material.MapColor (Minecraft 26.1.2) —
// the 62-entry map-color (col/id) table + the nested Brightness enum, plus the
// pure color math: calculateARGBColor / getColorFromPackedId / getPackedId.
//
// The color math itself (ARGB.opaque + the *int* overload of ARGB.scaleRGB) is the
// already-certified port in util/ARGB.h (mc::argb::), reused here verbatim — this
// header only adds the data table, the Brightness modifiers, and the dispatch.
//
// MapColor.java:84-86  calculateARGBColor(brightness):
//     return this == NONE ? 0 : ARGB.scaleRGB(ARGB.opaque(this.col), brightness.modifier);
//   brightness.modifier is an *int* (180/220/255/135), so the int overload of
//   ARGB.scaleRGB is selected (ARGB.java:117 — (long)channel*scale/255L, clamped 0..255).
//
// MapColor.java:98-105:
//     getColorFromPackedId(packedId): val = packedId & 0xFF;
//         return byIdUnsafe(val >> 2).calculateARGBColor(Brightness.byIdUnsafe(val & 3));
//     getPackedId(brightness): return (byte)(this.id << 2 | brightness.id & 3);
//
// Certified by map_color_parity (ground truth: tools/MapColorParity.java vs the real
// net.minecraft.world.level.material.MapColor).
//
// NOTE: the live-registry plumbing (byId's Preconditions.checkPositionIndex, the
// MATERIAL_COLORS[] side-effect during static init) is irrelevant to the pure value
// math — we model the table as a flat array indexed by id, with the byIdUnsafe
// "null -> NONE" fallback for unused ids (62, 63), exactly as the Java does.

#include "../../../util/ARGB.h"

#include <array>
#include <cstdint>

namespace mc::material {

// ── MapColor.Brightness enum — MapColor.java:107-130 ─────────────────────────
// LOW(0,180) NORMAL(1,220) HIGH(2,255) LOWEST(3,135); VALUES[] is in enum order.
struct MapColorBrightness {
    int id;
    int modifier;
};

// VALUES = {LOW, NORMAL, HIGH, LOWEST}; byIdUnsafe(id) = VALUES[id].
inline constexpr std::array<MapColorBrightness, 4> MAP_COLOR_BRIGHTNESS_VALUES{{
    {0, 180},  // LOW
    {1, 220},  // NORMAL
    {2, 255},  // HIGH
    {3, 135},  // LOWEST
}};

inline constexpr MapColorBrightness mapColorBrightnessByIdUnsafe(int id) {
    return MAP_COLOR_BRIGHTNESS_VALUES[static_cast<std::size_t>(id)];
}

// ── MapColor instance — MapColor.java:71-72 (public final int col, id) ───────
struct MapColor {
    int id;
    int col;
};

// The 62 declared MapColors (ids 0..61), MapColor.java:9-70, indexed by id.
// NONE is id 0 with col 0 — calculateARGBColor short-circuits to 0 for it.
inline constexpr std::array<MapColor, 62> MAP_COLORS{{
    {0, 0},          // NONE
    {1, 8368696},    // GRASS
    {2, 16247203},   // SAND
    {3, 13092807},   // WOOL
    {4, 16711680},   // FIRE
    {5, 10526975},   // ICE
    {6, 10987431},   // METAL
    {7, 31744},      // PLANT
    {8, 16777215},   // SNOW
    {9, 10791096},   // CLAY
    {10, 9923917},   // DIRT
    {11, 7368816},   // STONE
    {12, 4210943},   // WATER
    {13, 9402184},   // WOOD
    {14, 16776437},  // QUARTZ
    {15, 14188339},  // COLOR_ORANGE
    {16, 11685080},  // COLOR_MAGENTA
    {17, 6724056},   // COLOR_LIGHT_BLUE
    {18, 15066419},  // COLOR_YELLOW
    {19, 8375321},   // COLOR_LIGHT_GREEN
    {20, 15892389},  // COLOR_PINK
    {21, 5000268},   // COLOR_GRAY
    {22, 10066329},  // COLOR_LIGHT_GRAY
    {23, 5013401},   // COLOR_CYAN
    {24, 8339378},   // COLOR_PURPLE
    {25, 3361970},   // COLOR_BLUE
    {26, 6704179},   // COLOR_BROWN
    {27, 6717235},   // COLOR_GREEN
    {28, 10040115},  // COLOR_RED
    {29, 1644825},   // COLOR_BLACK
    {30, 16445005},  // GOLD
    {31, 6085589},   // DIAMOND
    {32, 4882687},   // LAPIS
    {33, 55610},     // EMERALD
    {34, 8476209},   // PODZOL
    {35, 7340544},   // NETHER
    {36, 13742497},  // TERRACOTTA_WHITE
    {37, 10441252},  // TERRACOTTA_ORANGE
    {38, 9787244},   // TERRACOTTA_MAGENTA
    {39, 7367818},   // TERRACOTTA_LIGHT_BLUE
    {40, 12223780},  // TERRACOTTA_YELLOW
    {41, 6780213},   // TERRACOTTA_LIGHT_GREEN
    {42, 10505550},  // TERRACOTTA_PINK
    {43, 3746083},   // TERRACOTTA_GRAY
    {44, 8874850},   // TERRACOTTA_LIGHT_GRAY
    {45, 5725276},   // TERRACOTTA_CYAN
    {46, 8014168},   // TERRACOTTA_PURPLE
    {47, 4996700},   // TERRACOTTA_BLUE
    {48, 4993571},   // TERRACOTTA_BROWN
    {49, 5001770},   // TERRACOTTA_GREEN
    {50, 9321518},   // TERRACOTTA_RED
    {51, 2430480},   // TERRACOTTA_BLACK
    {52, 12398641},  // CRIMSON_NYLIUM
    {53, 9715553},   // CRIMSON_STEM
    {54, 6035741},   // CRIMSON_HYPHAE
    {55, 1474182},   // WARPED_NYLIUM
    {56, 3837580},   // WARPED_STEM
    {57, 5647422},   // WARPED_HYPHAE
    {58, 1356933},   // WARPED_WART_BLOCK
    {59, 6579300},   // DEEPSLATE
    {60, 14200723},  // RAW_IRON
    {61, 8365974},   // GLOW_LICHEN
}};

// NONE is the canonical id-0 instance. calculateARGBColor compares `this == NONE`
// by identity; for our value model that is equivalent to id == 0 (NONE is the only
// instance with id 0, and the byIdUnsafe null->NONE fallback also yields id 0).
inline constexpr MapColor MAP_COLOR_NONE = MAP_COLORS[0];

// byIdUnsafe(id) — MapColor.java:93-96: MATERIAL_COLORS[id] != null ? it : NONE.
// ids 0..61 are populated; 62 and 63 are unused -> NONE. (byId's bounds check
// [0..64) is registry plumbing; we model only ids the table can hold, 0..63.)
inline constexpr MapColor mapColorByIdUnsafe(int id) {
    if (id >= 0 && id < 62) {
        return MAP_COLORS[static_cast<std::size_t>(id)];
    }
    return MAP_COLOR_NONE;  // ids 62, 63 (and any null slot) -> NONE
}

// calculateARGBColor(brightness) — MapColor.java:84-86.
inline int calculateARGBColor(const MapColor& self, const MapColorBrightness& brightness) {
    if (self.id == 0) {  // this == NONE
        return 0;
    }
    return mc::argb::scaleRGB(mc::argb::opaque(self.col), brightness.modifier);
}

// getColorFromPackedId(packedId) — MapColor.java:98-101.
inline int getColorFromPackedId(int packedId) {
    int val = packedId & 0xFF;
    return calculateARGBColor(mapColorByIdUnsafe(val >> 2), mapColorBrightnessByIdUnsafe(val & 3));
}

// getPackedId(brightness) — MapColor.java:103-105: (byte)(id << 2 | brightness.id & 3).
// Java returns a (signed) byte; mirror with int8_t so the sign bit matches for id>=32.
inline std::int8_t getPackedId(const MapColor& self, const MapColorBrightness& brightness) {
    return static_cast<std::int8_t>((self.id << 2) | (brightness.id & 3));
}

}  // namespace mc::material

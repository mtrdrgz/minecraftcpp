#pragma once

// 1:1 port of the two pure equine "variant" enums of Minecraft 26.1.2:
//   * net.minecraft.world.entity.animal.equine.Variant   (horse coat color)
//   * net.minecraft.world.entity.animal.equine.Markings   (horse markings)
//
// Both expose the same shape: a stable integer id (getId), a getSerializedName
// (Variant only), and a static byId(int) that resolves through
//   ByIdMap.continuous(<idGetter>, values(), ByIdMap.OutOfBoundsStrategy.WRAP)
//
// The WRAP strategy is the load-bearing 1:1 trap here. ByIdMap.continuous builds
// a dense array sorted by id [0, length) and, for WRAP, returns
//   sortedValues[Mth.positiveModulo(id, length)]
// where Mth.positiveModulo(int,int) == java.lang.Math.floorMod (ByIdMap.java:75,
// Mth.java:165-167). floorMod is NOT the C/C++ `%` operator for negative inputs:
//   Math.floorMod(-1, 7) == 6   but   (-1) % 7 == -1 in C++.
// A naive `id % length` port silently mis-resolves every negative id (and id =
// INT_MIN, where `-INT_MIN` overflows), so we route through the certified
// mc::levelgen::mth::positiveModulo (== floorMod, Mth.h:117-128).
//
// values() ordering == declaration order, and every entry's id equals its
// declaration index, so the sorted array is just the declaration order. We mirror
// that exactly. Pure, world-free, deterministic. Certified by equine_variants_parity.

#include <cstdint>
#include <string_view>

#include "world/level/levelgen/Mth.h"

namespace mc::world::entity::animal::equine {

// ── Variant ──────────────────────────────────────────────────────────────────
// Variant.java: WHITE(0,"white") .. DARK_BROWN(6,"dark_brown"). Declaration order
// == id order, so values()[ordinal].getId() == ordinal for all entries.
enum class Variant : int {
    WHITE = 0,
    CREAMY = 1,
    CHESTNUT = 2,
    BROWN = 3,
    BLACK = 4,
    GRAY = 5,
    DARK_BROWN = 6,
};

inline constexpr int VARIANT_COUNT = 7;

// Variant.getId() — Variant.java:31-33. The enum value IS the id.
inline constexpr int variantGetId(Variant v) { return static_cast<int>(v); }

// Sorted-by-id values() array (Variant.java declaration order, ids 0..6).
inline constexpr Variant VARIANT_BY_ORDINAL[VARIANT_COUNT] = {
    Variant::WHITE, Variant::CREAMY, Variant::CHESTNUT, Variant::BROWN,
    Variant::BLACK, Variant::GRAY,   Variant::DARK_BROWN,
};

// Variant.byId(int) — Variant.java:35-37 via ByIdMap.continuous(...WRAP).
inline Variant variantById(int id) {
    return VARIANT_BY_ORDINAL[mc::levelgen::mth::positiveModulo(id, VARIANT_COUNT)];
}

// Variant.getSerializedName() — Variant.java:39-42.
inline constexpr std::string_view variantSerializedName(Variant v) {
    switch (v) {
        case Variant::WHITE:      return "white";
        case Variant::CREAMY:     return "creamy";
        case Variant::CHESTNUT:   return "chestnut";
        case Variant::BROWN:      return "brown";
        case Variant::BLACK:      return "black";
        case Variant::GRAY:       return "gray";
        case Variant::DARK_BROWN: return "dark_brown";
    }
    return {};  // unreachable for the 7 defined values
}

// ── Markings ─────────────────────────────────────────────────────────────────
// Markings.java: NONE(0) .. BLACK_DOTS(4). Declaration order == id order.
enum class Markings : int {
    NONE = 0,
    WHITE = 1,
    WHITE_FIELD = 2,
    WHITE_DOTS = 3,
    BLACK_DOTS = 4,
};

inline constexpr int MARKINGS_COUNT = 5;

// Markings.getId() — Markings.java:20-22.
inline constexpr int markingsGetId(Markings m) { return static_cast<int>(m); }

inline constexpr Markings MARKINGS_BY_ORDINAL[MARKINGS_COUNT] = {
    Markings::NONE, Markings::WHITE, Markings::WHITE_FIELD,
    Markings::WHITE_DOTS, Markings::BLACK_DOTS,
};

// Markings.byId(int) — Markings.java:24-26 via ByIdMap.continuous(...WRAP).
inline Markings markingsById(int id) {
    return MARKINGS_BY_ORDINAL[mc::levelgen::mth::positiveModulo(id, MARKINGS_COUNT)];
}

}  // namespace mc::world::entity::animal::equine

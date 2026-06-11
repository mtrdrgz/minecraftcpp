// 1:1 C++ port of the pure static variant math of
// net.minecraft.world.entity.animal.fish.TropicalFish (Minecraft 26.1.2).
//
// Java source: 26.1.2/src/net/minecraft/world/entity/animal/fish/TropicalFish.java
//
// A TropicalFish's appearance is a single signed 32-bit "packed variant":
//
//   packVariant(pattern, baseColor, patternColor) =
//        pattern.getPackedId() & 65535
//      | (baseColor.getId()    & 0xFF) << 16
//      | (patternColor.getId() & 0xFF) << 24
//
// and the inverse accessors:
//
//   getBaseColor(packed)    = DyeColor.byId(packed >> 16 & 0xFF)
//   getPatternColor(packed) = DyeColor.byId(packed >> 24 & 0xFF)
//   getPattern(packed)      = Pattern.byId(packed & 65535)
//
// The Pattern enum carries `packedId = base.id | index << 8`, where
// Base.SMALL.id = 0, Base.LARGE.id = 1, and index runs 0..5 within each base.
// Pattern.byId is `ByIdMap.sparse(Pattern::getPackedId, values(), KOB)`, i.e. a
// hash lookup that returns the matching Pattern or, for any unknown key, KOB.
//
// Everything here is pure integer / bit arithmetic over Java `int` (signed
// 32-bit) plus the (already-certified) DyeColor.byId / DyeColor.getId tables and
// the ByIdMap.sparse semantics. No world, entity, registry, or datapack state.
//
// 1:1 traps captured verbatim:
//   * Java `>>` is an ARITHMETIC (sign-propagating) shift — getBaseColor /
//     getPatternColor shift first, then mask with 0xFF, so the sign bits that the
//     arithmetic shift smears in are discarded by the mask. We mirror that exact
//     order on a signed int.
//   * `& 0xFF` / `& 65535` keep the int signed in Java; the masked results here are
//     non-negative so they round-trip through DyeColor.byId / Pattern.byId cleanly.
//   * `<< 16` / `<< 24` can flip the sign bit of the resulting int (patternColor
//     ids >= 0x80 set bit 31). We compute the OR in uint32 and bit_cast back to a
//     signed int32 so there is no C++ signed-overflow UB at -O2 while preserving
//     the exact Java two's-complement bit pattern.
//
// All constants/formulas are verbatim from the Java; nothing is invented.

#ifndef MCPP_WORLD_ENTITY_ANIMAL_FISH_TROPICALFISHVARIANT_H
#define MCPP_WORLD_ENTITY_ANIMAL_FISH_TROPICALFISHVARIANT_H

#include <array>
#include <bit>
#include <cstdint>

#include "world/item/DyeColor.h"

namespace mc::world::entity::animal::fish {

// net.minecraft.world.entity.animal.fish.TropicalFish.Base
// (declaration order == enum ordinal; `id` is the explicit ctor arg).
enum class Base : std::int32_t {
    SMALL = 0,  // id 0
    LARGE = 1,  // id 1
};

inline constexpr std::int32_t baseId(Base b) { return static_cast<std::int32_t>(b); }

// net.minecraft.world.entity.animal.fish.TropicalFish.Pattern
// Ordinal order is the Java declaration order below; that order is irrelevant to
// the math (Pattern.byId is a sparse hash on packedId, not an ordinal lookup).
enum class Pattern : std::int32_t {
    KOB = 0,
    SUNSTREAK,
    SNOOPER,
    DASHER,
    BRINELY,
    SPOTTY,
    FLOPPER,
    STRIPEY,
    GLITTER,
    BLOCKFISH,
    BETTY,
    CLAYFISH,
};

inline constexpr std::int32_t PATTERN_COUNT = 12;

struct PatternData {
    Pattern pattern;
    Base base;
    std::int32_t index;  // 0..5 within the base
};

// Pattern enum-constant table, verbatim from TropicalFish.Pattern (name, base,
// index). Names are kept as serialized names for completeness.
inline constexpr std::array<PatternData, PATTERN_COUNT> PATTERNS{{
    {Pattern::KOB, Base::SMALL, 0},
    {Pattern::SUNSTREAK, Base::SMALL, 1},
    {Pattern::SNOOPER, Base::SMALL, 2},
    {Pattern::DASHER, Base::SMALL, 3},
    {Pattern::BRINELY, Base::SMALL, 4},
    {Pattern::SPOTTY, Base::SMALL, 5},
    {Pattern::FLOPPER, Base::LARGE, 0},
    {Pattern::STRIPEY, Base::LARGE, 1},
    {Pattern::GLITTER, Base::LARGE, 2},
    {Pattern::BLOCKFISH, Base::LARGE, 3},
    {Pattern::BETTY, Base::LARGE, 4},
    {Pattern::CLAYFISH, Base::LARGE, 5},
}};

inline constexpr const PatternData& patternData(Pattern p) {
    return PATTERNS[static_cast<std::size_t>(static_cast<std::int32_t>(p))];
}

inline constexpr Base patternBase(Pattern p) { return patternData(p).base; }

// Pattern.packedId = base.id | index << 8.
inline constexpr std::int32_t getPackedId(Pattern p) {
    const PatternData& d = patternData(p);
    return baseId(d.base) | (d.index << 8);
}

// Pattern.byId = ByIdMap.sparse(Pattern::getPackedId, values(), KOB):
//   look up `packedId` in the (packedId -> Pattern) map; absent -> KOB.
inline constexpr Pattern patternById(std::int32_t packedId) {
    for (const PatternData& d : PATTERNS) {
        if (getPackedId(d.pattern) == packedId) {
            return d.pattern;
        }
    }
    return Pattern::KOB;  // ByIdMap.sparse default
}

// TropicalFish.packVariant(pattern, baseColor, patternColor) =
//   pattern.getPackedId() & 65535
//   | (baseColor.getId()    & 0xFF) << 16
//   | (patternColor.getId() & 0xFF) << 24
//
// baseColorId / patternColorId are DyeColor ids in [0, 15]; we take them as ints
// to faithfully reproduce the `& 0xFF` masking on a signed Java int.
inline constexpr std::int32_t packVariant(Pattern pattern, std::int32_t baseColorId,
                                          std::int32_t patternColorId) {
    const std::uint32_t patternBits =
        static_cast<std::uint32_t>(getPackedId(pattern)) & 65535u;
    const std::uint32_t baseBits =
        (static_cast<std::uint32_t>(baseColorId) & 0xFFu) << 16;
    const std::uint32_t patternColorBits =
        (static_cast<std::uint32_t>(patternColorId) & 0xFFu) << 24;
    return std::bit_cast<std::int32_t>(patternBits | baseBits | patternColorBits);
}

// TropicalFish.getBaseColor(packed) = DyeColor.byId(packed >> 16 & 0xFF).
// `>>` is Java's arithmetic shift on a signed int; the subsequent `& 0xFF` keeps
// only the low byte, so the sign extension is irrelevant to the result but the
// shift must be performed on the SIGNED value first.
inline constexpr std::int32_t getBaseColorId(std::int32_t packed) {
    const std::int32_t id = (packed >> 16) & 0xFF;
    return mc::world::item::byId(id).id;  // DyeColor.byId(...).getId()
}

// TropicalFish.getPatternColor(packed) = DyeColor.byId(packed >> 24 & 0xFF).
inline constexpr std::int32_t getPatternColorId(std::int32_t packed) {
    const std::int32_t id = (packed >> 24) & 0xFF;
    return mc::world::item::byId(id).id;  // DyeColor.byId(...).getId()
}

// TropicalFish.getPattern(packed) = Pattern.byId(packed & 65535).
inline constexpr Pattern getPattern(std::int32_t packed) {
    return patternById(packed & 65535);
}

}  // namespace mc::world::entity::animal::fish

#endif  // MCPP_WORLD_ENTITY_ANIMAL_FISH_TROPICALFISHVARIANT_H

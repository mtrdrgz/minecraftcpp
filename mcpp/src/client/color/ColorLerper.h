#pragma once

// 1:1 port of net.minecraft.client.color.ColorLerper (26.1.2) — the render-time
// "rainbow" color cycling used by sheep (DyeColor.values(), 25-tick steps, 0.75
// brightness) and the note-block / music note particle (a 12-color palette,
// 30-tick steps, 1.25 brightness). Pure integer/float math: integer division +
// modulo over the tick, Mth.frac for the sub-tick, then ARGB.srgbLerp between the
// two adjacent precomputed colors.
//
// GL-free / GPU-free. Depends only on:
//   * mc::levelgen::mth::{floor, frac}                 (Mth.java)
//   * mc::argb::{srgbLerp, color, red, green, blue}    (ARGB.java, already gated)
//   * the baked DyeColor.getTextureDiffuseColor() table (DyeColor.java).
//
// The per-Type color tables are precomputed exactly as the Java enum ctor does it:
// getModifiedColor(dye, brightness), with WHITE special-cased to 0xFFE6E6DA
// (-1644826) regardless of brightness. Certified bit-exact by colorlerper_parity.

#include "../../world/level/levelgen/Mth.h"
#include "../../util/ARGB.h"

#include <array>
#include <cstdint>

namespace mc::client::color {

namespace mth  = mc::levelgen::mth;
namespace argb = mc::argb;

// ── net.minecraft.world.item.DyeColor ────────────────────────────────────────
// Ordinal order WHITE..BLACK (id == ordinal). getTextureDiffuseColor() returns
// ARGB.opaque(textureDiffuseColor) == 0xFF000000 | rgb, baked here.
enum class DyeColor : int {
    WHITE = 0, ORANGE, MAGENTA, LIGHT_BLUE, YELLOW, LIME, PINK, GRAY,
    LIGHT_GRAY, CYAN, PURPLE, BLUE, BROWN, GREEN, RED, BLACK
};

// DyeColor.getTextureDiffuseColor() per ordinal — DyeColor.java:30-46,
// textureDiffuseColor field = ARGB.opaque(<table value>).
inline constexpr int DYE_TEXTURE_DIFFUSE_COLOR[16] = {
    static_cast<int>(0xFF000000u | 16383998u), // WHITE
    static_cast<int>(0xFF000000u | 16351261u), // ORANGE
    static_cast<int>(0xFF000000u | 13061821u), // MAGENTA
    static_cast<int>(0xFF000000u |  3847130u), // LIGHT_BLUE
    static_cast<int>(0xFF000000u | 16701501u), // YELLOW
    static_cast<int>(0xFF000000u |  8439583u), // LIME
    static_cast<int>(0xFF000000u | 15961002u), // PINK
    static_cast<int>(0xFF000000u |  4673362u), // GRAY
    static_cast<int>(0xFF000000u | 10329495u), // LIGHT_GRAY
    static_cast<int>(0xFF000000u |  1481884u), // CYAN
    static_cast<int>(0xFF000000u |  8991416u), // PURPLE
    static_cast<int>(0xFF000000u |  3949738u), // BLUE
    static_cast<int>(0xFF000000u |  8606770u), // BROWN
    static_cast<int>(0xFF000000u |  6192150u), // GREEN
    static_cast<int>(0xFF000000u | 11546150u), // RED
    static_cast<int>(0xFF000000u |  1908001u), // BLACK
};

inline int getTextureDiffuseColor(DyeColor c) {
    return DYE_TEXTURE_DIFFUSE_COLOR[static_cast<int>(c)];
}

// ── ColorLerper.getModifiedColor — ColorLerper.java:41-48 ────────────────────
// WHITE is hard-coded to -1644826 (0xFFE6E6DA); everything else scales each
// channel by `brightness` with Mth.floor((float) channel * brightness) and
// re-opaques via ARGB.color(255, r, g, b).
inline int getModifiedColor(DyeColor color, float brightness) {
    if (color == DyeColor::WHITE) {
        return -1644826;
    }
    int src = getTextureDiffuseColor(color);
    return argb::color(255,
        mth::floor(static_cast<float>(argb::red(src))   * brightness),
        mth::floor(static_cast<float>(argb::green(src)) * brightness),
        mth::floor(static_cast<float>(argb::blue(src))  * brightness));
}

// ── ColorLerper.Type ─────────────────────────────────────────────────────────
// SHEEP(25, DyeColor.values(), 0.75F), MUSIC_NOTE(30, MUSIC_NOTE_COLORS, 1.25F).
// Each Type precomputes its modified colors in declaration order; getLerpedColor
// indexes that array directly (colorByDye in Java preserves no order, but the
// `colors` array drives indexing, so we store the resolved color array).
struct Type {
    int colorDuration;
    int colorCount;
    std::array<int, 16> colors; // precomputed getModifiedColor per palette entry

    int getColorAt(int index) const { return colors[index]; }
};

// ColorLerper.MUSIC_NOTE_COLORS — ColorLerper.java:13-26 (12 entries).
inline constexpr DyeColor MUSIC_NOTE_DYES[12] = {
    DyeColor::WHITE, DyeColor::LIGHT_GRAY, DyeColor::LIGHT_BLUE, DyeColor::BLUE,
    DyeColor::CYAN,  DyeColor::GREEN,      DyeColor::LIME,       DyeColor::YELLOW,
    DyeColor::ORANGE, DyeColor::PINK,      DyeColor::RED,        DyeColor::MAGENTA
};

// DyeColor.values() in ordinal order (the SHEEP palette).
inline constexpr DyeColor ALL_DYES[16] = {
    DyeColor::WHITE, DyeColor::ORANGE, DyeColor::MAGENTA, DyeColor::LIGHT_BLUE,
    DyeColor::YELLOW, DyeColor::LIME, DyeColor::PINK, DyeColor::GRAY,
    DyeColor::LIGHT_GRAY, DyeColor::CYAN, DyeColor::PURPLE, DyeColor::BLUE,
    DyeColor::BROWN, DyeColor::GREEN, DyeColor::RED, DyeColor::BLACK
};

inline Type makeSheep() {
    Type t{}; t.colorDuration = 25; t.colorCount = 16;
    for (int i = 0; i < 16; ++i) t.colors[i] = getModifiedColor(ALL_DYES[i], 0.75F);
    return t;
}

inline Type makeMusicNote() {
    Type t{}; t.colorDuration = 30; t.colorCount = 12;
    for (int i = 0; i < 12; ++i) t.colors[i] = getModifiedColor(MUSIC_NOTE_DYES[i], 1.25F);
    return t;
}

inline const Type& SHEEP()      { static const Type t = makeSheep();     return t; }
inline const Type& MUSIC_NOTE() { static const Type t = makeMusicNote(); return t; }

// ── ColorLerper.getLerpedColor — ColorLerper.java:30-39 ──────────────────────
//   int tickCount = Mth.floor(tick);
//   int value = tickCount / type.colorDuration;          // int division
//   int colorCount = type.colors.length;
//   int c1 = value % colorCount;
//   int c2 = (value + 1) % colorCount;
//   float subStep = (tickCount % type.colorDuration + Mth.frac(tick)) / type.colorDuration;
//   return ARGB.srgbLerp(subStep, type.getColor(colors[c1]), type.getColor(colors[c2]));
inline int getLerpedColor(const Type& type, float tick) {
    int tickCount  = mth::floor(tick);
    int value      = tickCount / type.colorDuration;
    int colorCount = type.colorCount;
    int c1 = value % colorCount;
    int c2 = (value + 1) % colorCount;
    float subStep = static_cast<float>(tickCount % type.colorDuration + mth::frac(tick))
                    / static_cast<float>(type.colorDuration);
    int color1 = type.getColorAt(c1);
    int color2 = type.getColorAt(c2);
    return argb::srgbLerp(subStep, color1, color2);
}

} // namespace mc::client::color

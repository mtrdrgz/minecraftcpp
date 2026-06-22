#pragma once

// 1:1 port of net.minecraft.util.ARGB (26.1.2) — the packed-ARGB color math used
// across rendering, GUI, particles, map colors, tinting. Pure integer/float
// arithmetic. The sRGB<->linear LUTs are built in Java from Math.pow/Math.round, so
// they are reflection-dumped and embedded (ARGBTables.inc) to stay bit-identical
// regardless of the host libm. Certified by argb_parity.

#include "../world/level/levelgen/Mth.h"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace mc::argb {

namespace mth = mc::levelgen::mth;

// SRGB_TO_LINEAR (short[256], 0..1023) and LINEAR_TO_SRGB (byte[1024] -> &0xFF, 0..255).
#include "ARGBTables.inc"

// java.lang.Math.round(float) — the modern (JDK-8010430) impl that avoids the
// +0.5f double-rounding. Used by ARGB.setBrightness and the LUT builders.
inline int javaRoundF(float a) {
    int32_t intBits = std::bit_cast<int32_t>(a);
    int biasedExp = (intBits & 0x7F800000) >> 23;
    int shift = (24 - 2 + 127) - biasedExp;            // 149 - biasedExp
    if ((shift & -32) == 0) {
        int r = (intBits & 0x007FFFFF) | 0x00800000;
        if (intBits < 0) r = -r;
        return ((r >> shift) + 1) >> 1;
    }
    return static_cast<int>(a);
}

// ── channel pack/unpack — ARGB.java:54-77 ────────────────────────────────────
inline int alpha(int color) { return static_cast<int>(static_cast<uint32_t>(color) >> 24); }
inline int red(int color)   { return (static_cast<uint32_t>(color) >> 16) & 0xFF; }
inline int green(int color) { return (static_cast<uint32_t>(color) >> 8) & 0xFF; }
inline int blue(int color)  { return color & 0xFF; }
inline int color(int a, int r, int g, int b) {
    return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}
inline int color(int r, int g, int b) { return color(255, r, g, b); }
inline int colorAlphaRgb(int a, int rgb) { return (a << 24) | (rgb & 0x00FFFFFF); }     // color(int,int)
inline int as8BitChannel(float v) { return mth::floor(v * 255.0F); }
inline int colorAlphaFRgb(float a, int rgb) { return (as8BitChannel(a) << 24) | (rgb & 0x00FFFFFF); } // color(float,int)

// ── float channels — ARGB.java:227-245 ───────────────────────────────────────
inline float from8BitChannel(int v) { return v / 255.0F; }
inline float alphaFloat(int color_) { return from8BitChannel(alpha(color_)); }
inline float redFloat(int color_)   { return from8BitChannel(red(color_)); }
inline float greenFloat(int color_) { return from8BitChannel(green(color_)); }
inline float blueFloat(int color_)  { return from8BitChannel(blue(color_)); }

// ── sRGB <-> linear — ARGB.java:30-51 ─────────────────────────────────────────
inline float srgbToLinearChannel(int srgb) { return ARGB_SRGB_TO_LINEAR[srgb] / 1023.0F; }
inline int   linearToSrgbChannel(float linear) { return ARGB_LINEAR_TO_SRGB[mth::floor(linear * 1023.0F)] & 0xFF; }
inline int linearChannelMean(int c1, int c2, int c3, int c4) {
    int linear = (ARGB_SRGB_TO_LINEAR[c1] + ARGB_SRGB_TO_LINEAR[c2] + ARGB_SRGB_TO_LINEAR[c3] + ARGB_SRGB_TO_LINEAR[c4]) / 4;
    return ARGB_LINEAR_TO_SRGB[linear] & 0xFF;
}
inline int meanLinear(int s1, int s2, int s3, int s4) {
    return color((alpha(s1) + alpha(s2) + alpha(s3) + alpha(s4)) / 4,
                 linearChannelMean(red(s1), red(s2), red(s3), red(s4)),
                 linearChannelMean(green(s1), green(s2), green(s3), green(s4)),
                 linearChannelMean(blue(s1), blue(s2), blue(s3), blue(s4)));
}

// ── blends / scales — ARGB.java:80-165 ────────────────────────────────────────
inline int multiply(int lhs, int rhs) {
    if (lhs == -1) return rhs;
    if (rhs == -1) return lhs;
    return color(alpha(lhs) * alpha(rhs) / 255, red(lhs) * red(rhs) / 255,
                 green(lhs) * green(rhs) / 255, blue(lhs) * blue(rhs) / 255);
}
inline int addRgb(int lhs, int rhs) {
    return color(alpha(lhs), std::min(red(lhs) + red(rhs), 255), std::min(green(lhs) + green(rhs), 255), std::min(blue(lhs) + blue(rhs), 255));
}
inline int subtractRgb(int lhs, int rhs) {
    return color(alpha(lhs), std::max(red(lhs) - red(rhs), 0), std::max(green(lhs) - green(rhs), 0), std::max(blue(lhs) - blue(rhs), 0));
}
inline int multiplyAlpha(int color_, float alphaMultiplier) {
    if (color_ == 0 || alphaMultiplier <= 0.0F) return 0;
    if (alphaMultiplier >= 1.0F) return color_;
    return colorAlphaFRgb(alphaFloat(color_) * alphaMultiplier, color_);
}
inline int scaleRGB(int color_, float sR, float sG, float sB) {
    return color(alpha(color_),
                 mth::clamp(static_cast<int>(red(color_) * sR), 0, 255),
                 mth::clamp(static_cast<int>(green(color_) * sG), 0, 255),
                 mth::clamp(static_cast<int>(blue(color_) * sB), 0, 255));
}
inline int scaleRGB(int color_, float scale) { return scaleRGB(color_, scale, scale, scale); }
inline int scaleRGB(int color_, int scale) {
    return color(alpha(color_),
                 static_cast<int>(mth::clamp(static_cast<int64_t>(red(color_)) * scale / 255LL, static_cast<int64_t>(0), static_cast<int64_t>(255))),
                 static_cast<int>(mth::clamp(static_cast<int64_t>(green(color_)) * scale / 255LL, static_cast<int64_t>(0), static_cast<int64_t>(255))),
                 static_cast<int>(mth::clamp(static_cast<int64_t>(blue(color_)) * scale / 255LL, static_cast<int64_t>(0), static_cast<int64_t>(255))));
}
inline int greyscale(int color_) {
    int g = static_cast<int>(red(color_) * 0.3F + green(color_) * 0.59F + blue(color_) * 0.11F);
    return color(alpha(color_), g, g, g);
}
inline int alphaBlendChannel(int resultAlpha, int sourceAlpha, int destination, int source) {
    return (source * sourceAlpha + destination * (resultAlpha - sourceAlpha)) / resultAlpha;
}
inline int alphaBlend(int destination, int source) {
    int dA = alpha(destination), sA = alpha(source);
    if (sA == 255) return source;
    if (sA == 0) return destination;
    int a = sA + dA * (255 - sA) / 255;
    return color(a,
                 alphaBlendChannel(a, sA, red(destination), red(source)),
                 alphaBlendChannel(a, sA, green(destination), green(source)),
                 alphaBlendChannel(a, sA, blue(destination), blue(source)));
}

// ── lerps — ARGB.java:167-186 ─────────────────────────────────────────────────
inline int srgbLerp(float a, int p0, int p1) {
    return color(mth::lerpInt(a, alpha(p0), alpha(p1)), mth::lerpInt(a, red(p0), red(p1)),
                 mth::lerpInt(a, green(p0), green(p1)), mth::lerpInt(a, blue(p0), blue(p1)));
}
inline int linearLerp(float a, int p0, int p1) {
    return color(mth::lerpInt(a, alpha(p0), alpha(p1)),
                 ARGB_LINEAR_TO_SRGB[mth::lerpInt(a, ARGB_SRGB_TO_LINEAR[red(p0)], ARGB_SRGB_TO_LINEAR[red(p1)])] & 0xFF,
                 ARGB_LINEAR_TO_SRGB[mth::lerpInt(a, ARGB_SRGB_TO_LINEAR[green(p0)], ARGB_SRGB_TO_LINEAR[green(p1)])] & 0xFF,
                 ARGB_LINEAR_TO_SRGB[mth::lerpInt(a, ARGB_SRGB_TO_LINEAR[blue(p0)], ARGB_SRGB_TO_LINEAR[blue(p1)])] & 0xFF);
}

// ── misc — ARGB.java:188-256 ──────────────────────────────────────────────────
inline int opaque(int color_)      { return color_ | static_cast<int>(0xFF000000); }
inline int transparent(int color_) { return color_ & 0x00FFFFFF; }
inline int white(float a) { return (as8BitChannel(a) << 24) | 0x00FFFFFF; }
inline int white(int a)   { return (a << 24) | 0x00FFFFFF; }
inline int black(float a) { return as8BitChannel(a) << 24; }
inline int black(int a)   { return a << 24; }
inline int gray(float brightness) { int c = as8BitChannel(brightness); return color(c, c, c); }
inline int colorFromFloat(float a, float r, float g, float b) {
    return color(as8BitChannel(a), as8BitChannel(r), as8BitChannel(g), as8BitChannel(b));
}
inline int average(int lhs, int rhs) {
    return color((alpha(lhs) + alpha(rhs)) / 2, (red(lhs) + red(rhs)) / 2, (green(lhs) + green(rhs)) / 2, (blue(lhs) + blue(rhs)) / 2);
}
inline int toABGR(int color_) {
    return (color_ & static_cast<int>(0xFF00FF00)) | ((color_ & 0x00FF0000) >> 16) | ((color_ & 0x000000FF) << 16);
}
inline int fromABGR(int color_) { return toABGR(color_); }

// ARGB.setBrightness — ARGB.java:258-336 (HSV brightness reset).
inline int setBrightness(int color_, float brightness) {
    int r = red(color_), g = green(color_), b = blue(color_), a = alpha(color_);
    int rgbMax = std::max(std::max(r, g), b);
    int rgbMin = std::min(std::min(r, g), b);
    float rgbConstantRange = static_cast<float>(rgbMax - rgbMin);
    float saturation = (rgbMax != 0) ? rgbConstantRange / rgbMax : 0.0F;
    float hue;
    if (saturation == 0.0F) {
        hue = 0.0F;
    } else {
        float cR = (rgbMax - r) / rgbConstantRange;
        float cG = (rgbMax - g) / rgbConstantRange;
        float cB = (rgbMax - b) / rgbConstantRange;
        if (r == rgbMax)      hue = cB - cG;
        else if (g == rgbMax) hue = 2.0F + cR - cB;
        else                  hue = 4.0F + cG - cR;
        hue /= 6.0F;
        if (hue < 0.0F) hue++;
    }
    if (saturation == 0.0F) {
        r = g = b = javaRoundF(brightness * 255.0F);
        return color(a, r, g, b);
    }
    float seg = (hue - static_cast<float>(std::floor(hue))) * 6.0F;
    float off = seg - static_cast<float>(std::floor(seg));
    float primary = brightness * (1.0F - saturation);
    float secondary = brightness * (1.0F - saturation * off);
    float tertiary = brightness * (1.0F - saturation * (1.0F - off));
    switch (static_cast<int>(seg)) {
        case 0: r = javaRoundF(brightness * 255.0F); g = javaRoundF(tertiary * 255.0F); b = javaRoundF(primary * 255.0F); break;
        case 1: r = javaRoundF(secondary * 255.0F); g = javaRoundF(brightness * 255.0F); b = javaRoundF(primary * 255.0F); break;
        case 2: r = javaRoundF(primary * 255.0F); g = javaRoundF(brightness * 255.0F); b = javaRoundF(tertiary * 255.0F); break;
        case 3: r = javaRoundF(primary * 255.0F); g = javaRoundF(secondary * 255.0F); b = javaRoundF(brightness * 255.0F); break;
        case 4: r = javaRoundF(tertiary * 255.0F); g = javaRoundF(primary * 255.0F); b = javaRoundF(brightness * 255.0F); break;
        case 5: r = javaRoundF(brightness * 255.0F); g = javaRoundF(primary * 255.0F); b = javaRoundF(secondary * 255.0F); break;
    }
    return color(a, r, g, b);
}

} // namespace mc::argb

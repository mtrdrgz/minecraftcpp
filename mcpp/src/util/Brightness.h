#pragma once

// 1:1 port of net.minecraft.util.Brightness + net.minecraft.util.LightCoordsUtil
// (Minecraft 26.1.2). These are the packed block/sky light-coordinate helpers used
// by the lightmap / vertex lighting path.
//
//   Brightness is a `record Brightness(int block, int sky)`; its pack()/unpack()
//   delegate to LightCoordsUtil.pack/block/sky — so the real bit math lives in
//   LightCoordsUtil. Both are ported here.
//
// All methods are pure 32-bit integer / float bit ops (plus one Mth.clamp + (int)
// truncation in addSmoothBlockEmission and an (int) truncation in
// smoothWeightedBlend). Java `int` == int32_t; `<<`/`>>` are signed 32-bit shifts
// (arithmetic right shift), matching C++ int32_t exactly. Certified by
// brightness_parity (ground truth: tools/BrightnessParity.java vs the real classes).

#include "../world/level/levelgen/Mth.h"

#include <algorithm>
#include <cstdint>

namespace mc::util {

namespace mth = mc::levelgen::mth;

// Java's `(int)float` narrowing (JLS 5.1.3): NaN -> 0; values outside int range or
// +/-Inf saturate to Integer.MAX_VALUE / MIN_VALUE; otherwise round toward zero. A
// plain C++ static_cast<int32_t>(NaN/Inf) is undefined, so any spot where the Java
// source casts a float to int must go through this to stay bit-exact on NaN/Inf
// inputs (smoothWeightedBlend / addSmoothBlockEmission can receive them).
inline int32_t javaF2I(float v) {
    if (v != v) return 0;                 // NaN
    if (v >= 2147483647.0f) return 2147483647;            // >= Integer.MAX_VALUE (incl +Inf)
    if (v <= -2147483648.0f) return -2147483647 - 1;      // <= Integer.MIN_VALUE (incl -Inf)
    return static_cast<int32_t>(v);       // in-range: round toward zero
}

// ---- net.minecraft.util.LightCoordsUtil -----------------------------------
namespace lightcoords {

// public static final int FULL_BRIGHT = 15728880;
inline constexpr int32_t FULL_BRIGHT = 15728880;
// public static final int FULL_SKY = 15728640;
inline constexpr int32_t FULL_SKY = 15728640;
// private static final int MAX_SMOOTH_LIGHT_LEVEL = 240;
inline constexpr int32_t MAX_SMOOTH_LIGHT_LEVEL = 240;

// return block << 4 | sky << 20;
inline constexpr int32_t pack(int32_t block, int32_t sky) {
    return (block << 4) | (sky << 20);
}

// return packed >> 4 & 15;
inline constexpr int32_t block(int32_t packed) {
    return (packed >> 4) & 15;
}

// return packed >> 20 & 15;
inline constexpr int32_t sky(int32_t packed) {
    return (packed >> 20) & 15;
}

// return coords & 0xFF0000 | block << 4;
inline constexpr int32_t withBlock(int32_t coords, int32_t block) {
    return (coords & 0xFF0000) | (block << 4);
}

// return block & 0xFF | (sky & 0xFF) << 16;
inline constexpr int32_t smoothPack(int32_t block, int32_t sky) {
    return (block & 0xFF) | ((sky & 0xFF) << 16);
}

// return packed & 0xFF;
inline constexpr int32_t smoothBlock(int32_t packed) {
    return packed & 0xFF;
}

// return packed >> 16 & 0xFF;
inline constexpr int32_t smoothSky(int32_t packed) {
    return (packed >> 16) & 0xFF;
}

// blockLightEmission = Mth.clamp(blockLightEmission, 0.0F, 1.0F);
// int emittedBlock = (int)(Mth.clamp(blockLightEmission, 0.0F, 1.0F) * 240.0F);
// int block = Math.min(smoothBlock(lightCoords) + emittedBlock, 240);
// return smoothPack(block, smoothSky(lightCoords));
inline int32_t addSmoothBlockEmission(int32_t lightCoords, float blockLightEmission) {
    blockLightEmission = mth::clamp(blockLightEmission, 0.0F, 1.0F);
    int32_t emittedBlock = mc::util::javaF2I(mth::clamp(blockLightEmission, 0.0F, 1.0F) * 240.0F);
    int32_t b = std::min(smoothBlock(lightCoords) + emittedBlock, 240);
    return smoothPack(b, smoothSky(lightCoords));
}

// int block1 = block(coords1); ... return pack(Math.max(block1,block2), Math.max(sky1,sky2));
inline constexpr int32_t max(int32_t coords1, int32_t coords2) {
    int32_t block1 = block(coords1);
    int32_t block2 = block(coords2);
    int32_t sky1 = sky(coords1);
    int32_t sky2 = sky(coords2);
    return pack(std::max(block1, block2), std::max(sky1, sky2));
}

// if (emission == 0) return lightCoords;
// int sky = Math.max(sky(lightCoords), emission);
// int block = Math.max(block(lightCoords), emission);
// return pack(block, sky);
inline constexpr int32_t lightCoordsWithEmission(int32_t lightCoords, int32_t emission) {
    if (emission == 0) {
        return lightCoords;
    }
    int32_t s = std::max(sky(lightCoords), emission);
    int32_t b = std::max(block(lightCoords), emission);
    return pack(b, s);
}

// smoothBlend(neighbor1, neighbor2, neighbor3, center) — note neighbor1..3 are passed
// by value in Java (locals reassigned). center is final.
inline constexpr int32_t smoothBlend(int32_t neighbor1, int32_t neighbor2, int32_t neighbor3, int32_t center) {
    if (sky(center) > 2 || block(center) > 2) {
        if (sky(neighbor1) == 0) {
            neighbor1 |= center & 0xFF0000;
        }
        if (block(neighbor1) == 0) {
            neighbor1 |= center & 0xFF;
        }
        if (sky(neighbor2) == 0) {
            neighbor2 |= center & 0xFF0000;
        }
        if (block(neighbor2) == 0) {
            neighbor2 |= center & 0xFF;
        }
        if (sky(neighbor3) == 0) {
            neighbor3 |= center & 0xFF0000;
        }
        if (block(neighbor3) == 0) {
            neighbor3 |= center & 0xFF;
        }
    }
    return ((neighbor1 + neighbor2 + neighbor3 + center) >> 2) & 16711935;
}

// int sky = (int)(smoothSky(c1)*w1 + smoothSky(c2)*w2 + smoothSky(c3)*w3 + smoothSky(c4)*w4);
// int block = (int)(smoothBlock(c1)*w1 + ... );
// return smoothPack(block, sky);
//
// Java: smoothSky(...) is int, weight is float -> int*float promotes to float; the
// four float products are summed in float, then narrowed (int). Mirror that exactly
// with float intermediates (NOT double) so the truncation matches bit-for-bit.
inline int32_t smoothWeightedBlend(int32_t coords1, int32_t coords2, int32_t coords3, int32_t coords4,
                                   float weight1, float weight2, float weight3, float weight4) {
    int32_t s = mc::util::javaF2I(
        static_cast<float>(smoothSky(coords1)) * weight1 + static_cast<float>(smoothSky(coords2)) * weight2 +
        static_cast<float>(smoothSky(coords3)) * weight3 + static_cast<float>(smoothSky(coords4)) * weight4);
    int32_t b = mc::util::javaF2I(
        static_cast<float>(smoothBlock(coords1)) * weight1 + static_cast<float>(smoothBlock(coords2)) * weight2 +
        static_cast<float>(smoothBlock(coords3)) * weight3 + static_cast<float>(smoothBlock(coords4)) * weight4);
    return smoothPack(b, s);
}

}  // namespace lightcoords

// ---- net.minecraft.util.Brightness (record Brightness(int block, int sky)) ----
struct Brightness {
    int32_t block;
    int32_t sky;

    // public int pack() { return LightCoordsUtil.pack(this.block, this.sky); }
    constexpr int32_t pack() const { return lightcoords::pack(block, sky); }

    // public static Brightness unpack(final int packed) {
    //    return new Brightness(LightCoordsUtil.block(packed), LightCoordsUtil.sky(packed)); }
    static constexpr Brightness unpack(int32_t packed) {
        return Brightness{lightcoords::block(packed), lightcoords::sky(packed)};
    }
};

// public static final Brightness FULL_BRIGHT = new Brightness(15, 15);
inline constexpr Brightness BRIGHTNESS_FULL_BRIGHT{15, 15};

}  // namespace mc::util

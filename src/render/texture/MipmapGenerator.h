#pragma once

// 1:1 port of the pure color math in
// net.minecraft.client.renderer.texture.MipmapGenerator (26.1.2):
//   * darkenedAlphaBlend  — sRGB->linear average of 4 texels, ignoring fully
//                           transparent ones, then linear->sRGB (DARK_CUTOUT mips).
//   * alphaTestCoverage   — 4x4 bilinear sub-sampling of each 2x2 texel quad,
//                           counting the fraction whose interpolated alpha (scaled
//                           and clamped) clears alphaRef.
//   * scaleAlphaToCoverage— 5-step bisection search for the alpha scale whose
//                           coverage best matches a target, then rewrites alpha.
//
// These are GL-free: in Java they read/write a NativeImage, which is just a
// MemoryUtil-backed packed-ARGB pixel buffer (no GpuTexture / RenderSystem). Here
// the image is a plain row-major int32 array (ARGB, row y * width + x), exactly the
// values NativeImage.getPixel/setPixel return/accept. All channel ops reuse the
// certified mc::argb port (sRGB LUTs, clamp, floor). Certified byte-exact by
// mipmap_generator_parity against the real class driven via reflection.
//
// 1:1 traps respected here:
//   * java.lang.Math.clamp(float,float,float) / Math.min / Math.max float
//     semantics (NOT Mth.clamp) — reproduced in javaMathMin/Max/Clamp below.
//   * java.lang.Math.abs(float) — plain magnitude.
//   * (subsample + 0.5F) / 4.0F and the bilinear FMA-free a*b+c chain evaluated in
//     float, left-to-right, with -ffp-contract=off (project default).
//   * ARGB.srgbToLinearChannel / linearToSrgbChannel go through the embedded LUTs,
//     so no host libm pow() enters the picture.

#include "../../util/ARGB.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace mc::render::mipmap {

namespace argb = mc::argb;

// ── java.lang.Math float min/max/clamp (NOT Mth.*) ───────────────────────────
// Math.max(float,float): NaN-propagating, with -0.0F < +0.0F. (JDK Math.java)
inline float javaMathMaxF(float a, float b) {
    if (a != a) return a;                       // a is NaN
    if (a == 0.0F && b == 0.0F) {
        // sum trick distinguishes -0.0/+0.0: -0.0 + -0.0 = -0.0 < +0.0.
        return std::bit_cast<int32_t>(a) & 0x80000000 ? b : a;
    }
    return (a >= b) ? a : b;
}
inline float javaMathMinF(float a, float b) {
    if (a != a) return a;                       // a is NaN
    if (a == 0.0F && b == 0.0F) {
        return std::bit_cast<int32_t>(a) & 0x80000000 ? a : b;
    }
    return (a <= b) ? a : b;
}
// Math.clamp(value,min,max) with min<max on every call site here.
inline float javaMathClampF(float value, float min, float max) {
    return javaMathMinF(max, javaMathMaxF(value, min));
}

// ── MipmapGenerator.darkenedAlphaBlend — MipmapGenerator.java:160-198 ─────────
inline int darkenedAlphaBlend(int color1, int color2, int color3, int color4) {
    float aTotal = 0.0F, rTotal = 0.0F, gTotal = 0.0F, bTotal = 0.0F;
    auto accum = [&](int c) {
        if (argb::alpha(c) != 0) {
            aTotal += argb::srgbToLinearChannel(argb::alpha(c));
            rTotal += argb::srgbToLinearChannel(argb::red(c));
            gTotal += argb::srgbToLinearChannel(argb::green(c));
            bTotal += argb::srgbToLinearChannel(argb::blue(c));
        }
    };
    accum(color1);
    accum(color2);
    accum(color3);
    accum(color4);
    aTotal /= 4.0F;
    rTotal /= 4.0F;
    gTotal /= 4.0F;
    bTotal /= 4.0F;
    return argb::color(argb::linearToSrgbChannel(aTotal), argb::linearToSrgbChannel(rTotal),
                       argb::linearToSrgbChannel(gTotal), argb::linearToSrgbChannel(bTotal));
}

// A NativeImage stand-in: row-major packed-ARGB pixels, image[y * width + x].
struct Image {
    int width = 0;
    int height = 0;
    std::vector<int> pixels;  // size width*height

    Image() = default;
    Image(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * h, 0) {}
    int getPixel(int x, int y) const { return pixels[static_cast<size_t>(y) * width + x]; }
    void setPixel(int x, int y, int p) { pixels[static_cast<size_t>(y) * width + x] = p; }
};

// ── MipmapGenerator.alphaTestCoverage — MipmapGenerator.java:17-48 ────────────
inline float alphaTestCoverage(const Image& image, float alphaRef, float alphaScale) {
    int width = image.width;
    int height = image.height;
    float coverage = 0.0F;

    for (int y = 0; y < height - 1; y++) {
        for (int x = 0; x < width - 1; x++) {
            float alpha00 = javaMathClampF(argb::alphaFloat(image.getPixel(x, y)) * alphaScale, 0.0F, 1.0F);
            float alpha10 = javaMathClampF(argb::alphaFloat(image.getPixel(x + 1, y)) * alphaScale, 0.0F, 1.0F);
            float alpha01 = javaMathClampF(argb::alphaFloat(image.getPixel(x, y + 1)) * alphaScale, 0.0F, 1.0F);
            float alpha11 = javaMathClampF(argb::alphaFloat(image.getPixel(x + 1, y + 1)) * alphaScale, 0.0F, 1.0F);
            float texelCoverage = 0.0F;

            for (int subsample_y = 0; subsample_y < 4; subsample_y++) {
                float fy = (subsample_y + 0.5F) / 4.0F;
                for (int subsample_x = 0; subsample_x < 4; subsample_x++) {
                    float fx = (subsample_x + 0.5F) / 4.0F;
                    float alpha = alpha00 * (1.0F - fx) * (1.0F - fy) + alpha10 * fx * (1.0F - fy)
                                + alpha01 * (1.0F - fx) * fy + alpha11 * fx * fy;
                    if (alpha > alphaRef) {
                        texelCoverage++;
                    }
                }
            }

            coverage += texelCoverage / 16.0F;
        }
    }

    return coverage / ((width - 1) * (height - 1));
}

// ── MipmapGenerator.scaleAlphaToCoverage — MipmapGenerator.java:50-89 ─────────
// Mutates `image` (alpha channel rewritten), matching the Java side which mutates
// the NativeImage in place.
inline void scaleAlphaToCoverage(Image& image, float desiredCoverage, float alphaRef, float alphaCutoffBias) {
    float minAlphaScale = 0.0F;
    float maxAlphaScale = 4.0F;
    float alphaScale = 1.0F;
    float bestAlphaScale = 1.0F;
    float bestError = std::numeric_limits<float>::max();   // Float.MAX_VALUE
    int width = image.width;
    int height = image.height;

    for (int i = 0; i < 5; i++) {
        float currentCoverage = alphaTestCoverage(image, alphaRef, alphaScale);
        float error = std::fabs(currentCoverage - desiredCoverage);   // Math.abs(float)
        if (error < bestError) {
            bestError = error;
            bestAlphaScale = alphaScale;
        }

        if (currentCoverage < desiredCoverage) {
            minAlphaScale = alphaScale;
        } else {
            if (!(currentCoverage > desiredCoverage)) {
                break;
            }
            maxAlphaScale = alphaScale;
        }

        alphaScale = (minAlphaScale + maxAlphaScale) * 0.5F;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel = image.getPixel(x, y);
            float alpha = argb::alphaFloat(pixel);
            alpha = alpha * bestAlphaScale + alphaCutoffBias + 0.025F;
            alpha = javaMathClampF(alpha, 0.0F, 1.0F);
            image.setPixel(x, y, argb::colorAlphaFRgb(alpha, pixel));
        }
    }
}

// ── mip downscale step (MipmapGenerator.java:127-149) for non-DARK strategies ─
// Produces the next mip level from `last`, halving each dimension. `dark` selects
// darkenedAlphaBlend (DARK_CUTOUT) vs ARGB.meanLinear (MEAN/CUTOUT/STRICT_CUTOUT).
// (The solidify/fill empty-area pre-pass lives in TextureUtil — GL-importing class,
// not ported here; documented as the only un-ported branch of generateMipLevels.)
inline Image downscaleMip(const Image& last, bool dark) {
    Image data(last.width >> 1, last.height >> 1);
    int width = data.width;
    int height = data.height;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            int color1 = last.getPixel(x * 2 + 0, y * 2 + 0);
            int color2 = last.getPixel(x * 2 + 1, y * 2 + 0);
            int color3 = last.getPixel(x * 2 + 0, y * 2 + 1);
            int color4 = last.getPixel(x * 2 + 1, y * 2 + 1);
            int color = dark ? darkenedAlphaBlend(color1, color2, color3, color4)
                             : argb::meanLinear(color1, color2, color3, color4);
            data.setPixel(x, y, color);
        }
    }
    return data;
}

}  // namespace mc::render::mipmap

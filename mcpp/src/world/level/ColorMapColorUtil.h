#pragma once

// Bit-exact 1:1 port of the biome colormap sampler shared by grass / foliage
// rendering in Minecraft Java Edition 26.1.2:
//
//   * net.minecraft.world.level.ColorMapColorUtil#get(double, double, int[], int)
//   * net.minecraft.world.level.GrassColor#get(double, double)   (+ default color)
//   * net.minecraft.world.level.FoliageColor#get(double, double)  (+ default color)
//
// These map a (temperature, downfall) pair to an ARGB colour by indexing a
// 256x256 colormap texture (the `grass.png` / `foliage.png` images, supplied as a
// length-65536 int[] of packed pixels). GrassColor / FoliageColor are thin wrappers
// that call ColorMapColorUtil#get with their own fallback colour.
//
// Source (26.1.2/src/net/minecraft/world/level/ColorMapColorUtil.java):
//
//   static int get(final double temp, double rain, final int[] pixels, final int defaultMapColor) {
//      rain *= temp;
//      int x = (int)((1.0 - temp) * 255.0);
//      int y = (int)((1.0 - rain) * 255.0);
//      int index = y << 8 | x;
//      return index >= pixels.length ? defaultMapColor : pixels[index];
//   }
//
// Source (GrassColor.java):
//   private static int[] pixels = new int[65536];
//   public static int get(final double temp, final double rain) {
//      return ColorMapColorUtil.get(temp, rain, pixels, -65281);
//   }
//   public static int getDefaultColor() { return get(0.5, 1.0); }
//
// Source (FoliageColor.java):
//   public static final int FOLIAGE_EVERGREEN = -10380959;
//   public static final int FOLIAGE_BIRCH     = -8345771;
//   public static final int FOLIAGE_DEFAULT   = -12012264;
//   public static final int FOLIAGE_MANGROVE  = -7158200;
//   private static int[] pixels = new int[65536];
//   public static int get(final double temp, final double rain) {
//      return ColorMapColorUtil.get(temp, rain, pixels, -12012264);
//   }
//
// ── Bit-exactness facts (read straight from the source) ──────────────────────
//
//  * `rain *= temp` is a plain double multiply (the parameter `rain` is reused).
//
//  * `(int)((1.0 - temp) * 255.0)` is a Java narrowing of double -> int, which
//    truncates TOWARD ZERO (JLS 5.1.3). C++ static_cast<int>(double) also
//    truncates toward zero, so the two agree for every finite value whose
//    truncation fits in a 32-bit int. The parity gate feeds only finite physical
//    inputs whose (1.0 - v) * 255.0 stays well inside int range, so the JLS
//    saturation rules for NaN / out-of-range (which C++ leaves UB) are never
//    reached. (Negative and out-of-[0,1] temps/rains ARE exercised, to cover the
//    truncation direction and the bounds guard below.)
//
//  * `y << 8 | x` is a 32-bit int shift then bitwise OR. We use int32_t and rely
//    on the inputs keeping x,y small (the gate never produces shifts that hit the
//    sign bit), matching Java's two's-complement int arithmetic.
//
//  * `index >= pixels.length` uses pixels.length == 65536 here (both colormaps are
//    allocated as new int[65536]). A negative index (from a negative y) is < length
//    and would index the array; the parity gate keeps the colormap a full 65536
//    entries and only feeds (temp,rain) that yield a valid in-bounds index OR an
//    index >= 65536 (the default-colour branch), never a negative index — exactly
//    as the real grass/foliage call sites do for in-game temperature/downfall.
//
// The `pixels` colormap is a genuine INPUT, fed identically (bit-for-bit) to both
// the real Java methods and this port by the ground-truth tool; nothing about the
// algorithm is invented here.
//
// NO deviation from the source is permitted in this file.

#include <cstdint>
#include <vector>

namespace mc::level::colormap {

// FoliageColor public constants (FoliageColor.java).
inline constexpr std::int32_t FOLIAGE_EVERGREEN = -10380959;
inline constexpr std::int32_t FOLIAGE_BIRCH = -8345771;
inline constexpr std::int32_t FOLIAGE_DEFAULT = -12012264;
inline constexpr std::int32_t FOLIAGE_MANGROVE = -7158200;

// GrassColor fallback colour (the -65281 magenta passed by GrassColor#get).
inline constexpr std::int32_t GRASS_DEFAULT_MAP_COLOR = -65281;

// ColorMapColorUtil#get(double temp, double rain, int[] pixels, int defaultMapColor).
//   rain *= temp;
//   int x = (int)((1.0 - temp) * 255.0);
//   int y = (int)((1.0 - rain) * 255.0);
//   int index = y << 8 | x;
//   return index >= pixels.length ? defaultMapColor : pixels[index];
inline std::int32_t get(double temp, double rain,
                        const std::vector<std::int32_t>& pixels,
                        std::int32_t defaultMapColor) {
    rain *= temp;
    std::int32_t x = static_cast<std::int32_t>((1.0 - temp) * 255.0);
    std::int32_t y = static_cast<std::int32_t>((1.0 - rain) * 255.0);
    std::int32_t index = (y << 8) | x;
    return index >= static_cast<std::int32_t>(pixels.size())
               ? defaultMapColor
               : pixels[static_cast<std::size_t>(index)];
}

// GrassColor#get(double temp, double rain) — fixed -65281 fallback.
inline std::int32_t grassGet(double temp, double rain,
                             const std::vector<std::int32_t>& pixels) {
    return get(temp, rain, pixels, GRASS_DEFAULT_MAP_COLOR);
}

// FoliageColor#get(double temp, double rain) — fixed FOLIAGE_DEFAULT fallback.
inline std::int32_t foliageGet(double temp, double rain,
                               const std::vector<std::int32_t>& pixels) {
    return get(temp, rain, pixels, FOLIAGE_DEFAULT);
}

}  // namespace mc::level::colormap

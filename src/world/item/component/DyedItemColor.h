#pragma once
#include <cstdint>
#include <optional>
#include <vector>

// ---------------------------------------------------------------------------
// 1:1 C++ port of the pure dye-mixing helper of
//   net.minecraft.world.item.component.DyedItemColor (Minecraft Java Edition
//   26.1.2)
//
//   DyedItemColor.applyDyes(@Nullable DyedItemColor currentDye,
//                           List<DyeColor> dyes)            DyedItemColor.java:40-79
//
// This is the leather-armour / cauldron dye blending math: given an item's
// current dye colour (or none) and a list of dyes being applied, it averages
// the per-channel RGB across all participating colours, then rescales the
// result so the output's peak channel equals the *average* per-colour peak
// channel ("intensity"). It is a pure, deterministic, world-free static helper:
// NO Level / registry / RandomSource / ItemStack involvement for this overload —
// it takes a nullable colour + a sequence of DyeColor enum constants and returns
// a new colour. The ItemStack-facing applyDyes(ItemStack,...) simply unwraps the
// DYED_COLOR component and calls THIS overload, so this is the entire arithmetic.
//
// Java source (DyedItemColor.java:40-79):
//   public static DyedItemColor applyDyes(@Nullable DyedItemColor currentDye, List<DyeColor> dyes) {
//      int redTotal = 0; int greenTotal = 0; int blueTotal = 0;
//      int intensityTotal = 0; int colorCount = 0;
//      if (currentDye != null) {
//         int red = ARGB.red(currentDye.rgb());
//         int green = ARGB.green(currentDye.rgb());
//         int blue = ARGB.blue(currentDye.rgb());
//         intensityTotal += Math.max(red, Math.max(green, blue));
//         redTotal += red; greenTotal += green; blueTotal += blue; colorCount++;
//      }
//      for (DyeColor dye : dyes) {
//         int color = dye.getTextureDiffuseColor();
//         int red = ARGB.red(color); int green = ARGB.green(color); int blue = ARGB.blue(color);
//         intensityTotal += Math.max(red, Math.max(green, blue));
//         redTotal += red; greenTotal += green; blueTotal += blue; colorCount++;
//      }
//      int red = redTotal / colorCount;
//      int green = greenTotal / colorCount;
//      int blue = blueTotal / colorCount;
//      float averageIntensity = (float)intensityTotal / colorCount;
//      float resultIntensity = Math.max(red, Math.max(green, blue));
//      red = (int)(red * averageIntensity / resultIntensity);
//      green = (int)(green * averageIntensity / resultIntensity);
//      blue = (int)(blue * averageIntensity / resultIntensity);
//      int rgb = ARGB.color(0, red, green, blue);
//      return new DyedItemColor(rgb);
//   }
//
// 1:1 TRAPS reproduced exactly:
//   * ARGB.red/green/blue (ARGB.java:56-66): red = color>>16 & 0xFF,
//     green = color>>8 & 0xFF, blue = color & 0xFF. These are signed >> on a
//     Java int, but each is immediately masked with & 0xFF so the result is a
//     clean 0..255 byte — sign of the shift is irrelevant after the mask.
//   * intensity per colour = Math.max(red, Math.max(green, blue)) — INTEGER max
//     of the three byte channels; accumulated in an int (intensityTotal).
//   * red/green/blue averages are INTEGER division (redTotal / colorCount), i.e.
//     truncation toward zero (all operands non-negative here, so floor).
//   * averageIntensity = (float)intensityTotal / colorCount : intensityTotal is
//     cast to float FIRST, then divided by colorCount (int promoted to float) —
//     a FLOAT division. Order matters; do not do the division in int.
//   * resultIntensity is declared float but assigned Math.max(int,int,int) — the
//     int max is computed, THEN widened to float.
//   * red = (int)(red * averageIntensity / resultIntensity):
//       - red * averageIntensity : int promoted to float, FLOAT multiply.
//       - / resultIntensity      : FLOAT divide.
//       - (int) narrowing of a float (JLS 5.1.3): truncates toward zero; NaN->0;
//         out-of-range saturates to INT_MIN/INT_MAX. (Inputs here keep it in
//         0..255, but the cast semantics are honoured.)
//     Must be done in float with NO contraction (compile -ffp-contract=off) and
//     in this exact operand order.
//   * ARGB.color(int alpha, int red, int green, int blue) (ARGB.java:68-70):
//     (alpha&0xFF)<<24 | (red&0xFF)<<16 | (green&0xFF)<<8 | (blue&0xFF). Here
//     alpha is 0, so the result has a zero alpha byte (NOT opaque) — the caller
//     stores exactly this 32-bit value as DyedItemColor.rgb.
//   * DyeColor.getTextureDiffuseColor() returns ARGB.opaque(rawLiteral) =
//     rawLiteral | 0xFF000000, but only red/green/blue (the low 24 bits) are
//     read, so the raw 24-bit literals below are the exact channel source.
//
// NOTE: division by colorCount is undefined when colorCount == 0 in Java too
// (ArithmeticException for the int divides). The real method is only ever called
// with at least one dye, or a non-null currentDye, so colorCount >= 1. The C++
// port mirrors that contract; the ground-truth battery never drives the 0 case.
// ---------------------------------------------------------------------------

namespace mc::item_component::dyed_item_color {

// JLS 5.1.3 narrowing float->int: round toward zero; NaN -> 0; values >=
// INT_MAX saturate to INT_MAX; values <= INT_MIN saturate to INT_MIN. A plain
// C++ static_cast<int>(float) is UB for out-of-range / NaN, so we replicate the
// Java semantics explicitly. (In this helper the operand is always in 0..255,
// but we honour the exact narrowing the source relies on.)
inline int floatToIntJls(float f) {
    if (f != f) return 0; // NaN
    if (f >= 2147483647.0f) return 2147483647;       // >= 2^31-1 -> INT_MAX
    if (f <= -2147483648.0f) return -2147483647 - 1; // <= -2^31 -> INT_MIN
    return static_cast<int>(f);                       // truncate toward zero
}

// ---- net.minecraft.util.ARGB channel helpers (the exact ops applyDyes uses) ----
// Java ints are 32-bit two's complement; we operate on int32_t to mirror them.
inline int argbRed(int color) { return (color >> 16) & 0xFF; }
inline int argbGreen(int color) { return (color >> 8) & 0xFF; }
inline int argbBlue(int color) { return color & 0xFF; }

// ARGB.color(alpha, red, green, blue): pack four channels, masking each to a byte.
inline int argbColor(int alpha, int red, int green, int blue) {
    // Compute in uint32 to make the <<24 well-defined, then reinterpret as int32.
    uint32_t packed = (static_cast<uint32_t>(alpha & 0xFF) << 24) |
                      (static_cast<uint32_t>(red & 0xFF) << 16) |
                      (static_cast<uint32_t>(green & 0xFF) << 8) |
                      static_cast<uint32_t>(blue & 0xFF);
    return static_cast<int>(packed);
}

// ---- DyeColor.getTextureDiffuseColor() table (the 16 enum constants) -----------
// Each value is the raw 24-bit textureDiffuseColor literal from DyeColor.java:30-45
// (ARGB.opaque is applied to it in the enum ctor, but only RGB is read here).
// Index == DyeColor ordinal/id (WHITE=0 .. BLACK=15).
inline int dyeColorTextureDiffuse(int dyeId) {
    static const int TABLE[16] = {
        16383998, // WHITE
        16351261, // ORANGE
        13061821, // MAGENTA
        3847130,  // LIGHT_BLUE
        16701501, // YELLOW
        8439583,  // LIME
        15961002, // PINK
        4673362,  // GRAY
        10329495, // LIGHT_GRAY
        1481884,  // CYAN
        8991416,  // PURPLE
        3949738,  // BLUE
        8606770,  // BROWN
        6192150,  // GREEN
        11546150, // RED
        1908001,  // BLACK
    };
    return TABLE[dyeId];
}

// Java: DyedItemColor.applyDyes(@Nullable DyedItemColor currentDye, List<DyeColor> dyes).
// `currentDye` is std::nullopt when the item has no DYED_COLOR component; its
// value (when present) is the stored 32-bit rgb. `dyeIds` are DyeColor ordinals.
// Returns the new DyedItemColor.rgb 32-bit value.
inline int applyDyes(std::optional<int> currentDye, const std::vector<int>& dyeIds) {
    int redTotal = 0;
    int greenTotal = 0;
    int blueTotal = 0;
    int intensityTotal = 0;
    int colorCount = 0;

    if (currentDye.has_value()) {
        int rgb = *currentDye;
        int red = argbRed(rgb);
        int green = argbGreen(rgb);
        int blue = argbBlue(rgb);
        // Math.max(red, Math.max(green, blue)) — integer max of three bytes.
        int maxgb = green > blue ? green : blue;
        intensityTotal += (red > maxgb ? red : maxgb);
        redTotal += red;
        greenTotal += green;
        blueTotal += blue;
        colorCount++;
    }

    for (int dyeId : dyeIds) {
        int color = dyeColorTextureDiffuse(dyeId);
        int red = argbRed(color);
        int green = argbGreen(color);
        int blue = argbBlue(color);
        int maxgb = green > blue ? green : blue;
        intensityTotal += (red > maxgb ? red : maxgb);
        redTotal += red;
        greenTotal += green;
        blueTotal += blue;
        colorCount++;
    }

    // Integer division (truncation toward zero; operands non-negative -> floor).
    int red = redTotal / colorCount;
    int green = greenTotal / colorCount;
    int blue = blueTotal / colorCount;

    // (float)intensityTotal / colorCount : cast numerator first, float divide.
    float averageIntensity = static_cast<float>(intensityTotal) / static_cast<float>(colorCount);
    // resultIntensity = Math.max(red, Math.max(green, blue)) widened to float.
    int maxgbResult = green > blue ? green : blue;
    int resultIntensityInt = red > maxgbResult ? red : maxgbResult;
    float resultIntensity = static_cast<float>(resultIntensityInt);

    // red = (int)(red * averageIntensity / resultIntensity) — float math, then
    // JLS 5.1.3 narrowing (truncate toward zero / saturate / NaN->0).
    red = floatToIntJls(static_cast<float>(red) * averageIntensity / resultIntensity);
    green = floatToIntJls(static_cast<float>(green) * averageIntensity / resultIntensity);
    blue = floatToIntJls(static_cast<float>(blue) * averageIntensity / resultIntensity);

    // ARGB.color(0, red, green, blue) — alpha byte is 0.
    return argbColor(0, red, green, blue);
}

} // namespace mc::item_component::dyed_item_color

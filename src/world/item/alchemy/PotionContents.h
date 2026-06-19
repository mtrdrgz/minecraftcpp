#pragma once

// 1:1 port of the pure color math of net.minecraft.world.item.alchemy.PotionContents
// (26.1.2): getColorOptional / getColorOr / getColor. This is the potion-tint
// computation — a weighted (by amplifier+1) average of the per-channel sRGB
// components of every *visible* effect's color, then re-packed via ARGB.color.
//
// Java source (PotionContents.java):
//
//   public static final int BASE_POTION_COLOR = -13083194;
//
//   public int getColor() { return this.getColorOr(-13083194); }
//
//   public int getColorOr(int defaultColor) {
//      return this.customColor.isPresent()
//          ? this.customColor.get()
//          : getColorOptional(this.getAllEffects()).orElse(defaultColor);
//   }
//
//   public static OptionalInt getColorOptional(Iterable<MobEffectInstance> effects) {
//      int red = 0, green = 0, blue = 0, totalWeight = 0;
//      for (MobEffectInstance effect : effects) {
//         if (effect.isVisible()) {
//            int color = effect.getEffect().value().getColor();
//            int amplifier = effect.getAmplifier() + 1;
//            red   += amplifier * ARGB.red(color);
//            green += amplifier * ARGB.green(color);
//            blue  += amplifier * ARGB.blue(color);
//            totalWeight += amplifier;
//         }
//      }
//      return totalWeight == 0
//          ? OptionalInt.empty()
//          : OptionalInt.of(ARGB.color(red / totalWeight, green / totalWeight, blue / totalWeight));
//   }
//
// World-free: the per-effect color is an int the caller supplies (it is read from
// the effect registry on the Java side; the C++ side never touches a registry).
//
// 1:1 traps preserved:
//  * Java `int` arithmetic wraps on overflow. The accumulators (red/green/blue),
//    the per-effect product `amplifier * ARGB.<channel>(color)` and `totalWeight`
//    are all 32-bit-wrapping; modelled with int32_t and unsigned wraps below.
//  * `red / totalWeight` is Java integer division (truncates toward zero); with
//    the canonical inputs both operands are >= 0 so it matches C++ `/`, but we use
//    the same truncating semantics regardless.
//  * ARGB.red(c) = (c >>> 16) & 0xFF, green = (c >>> 8) & 0xFF, blue = c & 0xFF —
//    logical (unsigned) shift, so a negative `color` still yields 0..255.
//  * ARGB.color(r,g,b) = color(255, r, g, b) = (255<<24)|((r&0xFF)<<16)|((g&0xFF)<<8)|(b&0xFF).
//  * amplifier is clamped to [0,255] by the MobEffectInstance constructor, so the
//    *weight* used here is (clampedAmplifier + 1) in [1,256]. The caller passes the
//    already-clamped amplifier.
//
// Certified bit-exact by potion_color_parity against tools/PotionColorParity.java.

#include "../../../util/ARGB.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace mc::alchemy {

namespace argb = mc::argb;

// net.minecraft.world.item.alchemy.PotionContents.BASE_POTION_COLOR
inline constexpr std::int32_t BASE_POTION_COLOR = -13083194;

// One visible/invisible effect as seen by getColorOptional: its packed color and
// its (already-clamped, 0..255) amplifier. `visible` mirrors MobEffectInstance.isVisible().
struct EffectColor {
    std::int32_t color = 0;
    std::int32_t amplifier = 0;  // clamped 0..255, as stored by MobEffectInstance
    bool visible = true;
};

// PotionContents.getColorOptional(Iterable<MobEffectInstance>). Returns nullopt when
// no visible effect contributes (totalWeight == 0).
inline std::optional<std::int32_t> getColorOptional(const std::vector<EffectColor>& effects) {
    // Java ints; emulate 32-bit wrap-on-overflow via unsigned accumulation.
    std::int32_t red = 0;
    std::int32_t green = 0;
    std::int32_t blue = 0;
    std::int32_t totalWeight = 0;

    for (const EffectColor& effect : effects) {
        if (effect.visible) {
            std::int32_t color = effect.color;
            std::int32_t amplifier = effect.amplifier + 1;
            red = static_cast<std::int32_t>(static_cast<std::uint32_t>(red) +
                                            static_cast<std::uint32_t>(amplifier) * static_cast<std::uint32_t>(argb::red(color)));
            green = static_cast<std::int32_t>(static_cast<std::uint32_t>(green) +
                                              static_cast<std::uint32_t>(amplifier) * static_cast<std::uint32_t>(argb::green(color)));
            blue = static_cast<std::int32_t>(static_cast<std::uint32_t>(blue) +
                                             static_cast<std::uint32_t>(amplifier) * static_cast<std::uint32_t>(argb::blue(color)));
            totalWeight = static_cast<std::int32_t>(static_cast<std::uint32_t>(totalWeight) + static_cast<std::uint32_t>(amplifier));
        }
    }

    if (totalWeight == 0) {
        return std::nullopt;
    }
    return argb::color(red / totalWeight, green / totalWeight, blue / totalWeight);
}

// PotionContents.getColorOr(int). With potion == empty, getAllEffects() == the
// custom-effect list, which is exactly the EffectColor vector passed here.
inline std::int32_t getColorOr(const std::optional<std::int32_t>& customColor,
                               const std::vector<EffectColor>& effects,
                               std::int32_t defaultColor) {
    if (customColor.has_value()) {
        return *customColor;
    }
    std::optional<std::int32_t> mixed = getColorOptional(effects);
    return mixed.has_value() ? *mixed : defaultColor;
}

// PotionContents.getColor() == getColorOr(BASE_POTION_COLOR).
inline std::int32_t getColor(const std::optional<std::int32_t>& customColor,
                             const std::vector<EffectColor>& effects) {
    return getColorOr(customColor, effects, BASE_POTION_COLOR);
}

}  // namespace mc::alchemy

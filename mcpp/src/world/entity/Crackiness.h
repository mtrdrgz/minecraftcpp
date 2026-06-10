// 1:1 port of net.minecraft.world.entity.Crackiness (Minecraft 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/world/entity/Crackiness.java
//
// Maps a remaining-durability fraction (or a damage/maxDamage pair) to a crack
// rendering level. The class holds three thresholds (low/medium/high) and there
// are exactly two preset instances in vanilla:
//   GOLEM      = new Crackiness(0.75F, 0.5F,  0.25F)
//   WOLF_ARMOR = new Crackiness(0.95F, 0.69F, 0.32F)
//
// byFraction (Crackiness.java:18-26) — note the comparison order EXACTLY:
//   if (fraction < fractionHigh)   return HIGH;
//   if (fraction < fractionMedium) return MEDIUM;
//   return fraction < fractionLow ? LOW : NONE;
//
// byDamage(int damage, int maxDamage) (Crackiness.java:32-34):
//   return byFraction((float)(maxDamage - damage) / maxDamage);
//   -> Java integer subtraction (two's-complement wrap), int->float widening
//      cast (exact for in-range), then float division. We mirror with the same
//      widening so the divide operands are bit-identical.
//
// SKIPPED: byDamage(ItemStack) — depends on the un-ported ItemStack
// (isDamageableItem/getDamageValue/getMaxDamage); not portable here.

#pragma once

#include <cstdint>

namespace mc {

class Crackiness {
public:
    // Crackiness.Level enum, declaration order NONE, LOW, MEDIUM, HIGH
    // (Crackiness.java:36-41). Ordinal values match the Java enum.
    enum class Level : int32_t {
        NONE = 0,
        LOW = 1,
        MEDIUM = 2,
        HIGH = 3,
    };

    constexpr Crackiness(float fractionLow, float fractionMedium, float fractionHigh)
        : fractionLow_(fractionLow),
          fractionMedium_(fractionMedium),
          fractionHigh_(fractionHigh) {}

    // Crackiness.java:18-26 — comparison order is load-bearing.
    Level byFraction(float fraction) const {
        if (fraction < fractionHigh_) {
            return Level::HIGH;
        } else if (fraction < fractionMedium_) {
            return Level::MEDIUM;
        } else {
            return fraction < fractionLow_ ? Level::LOW : Level::NONE;
        }
    }

    // Crackiness.java:32-34. Java: (float)(maxDamage - damage) / maxDamage.
    // maxDamage - damage is a 32-bit int subtraction (two's-complement wrap),
    // widened to float, divided by maxDamage widened to float.
    Level byDamage(int32_t damage, int32_t maxDamage) const {
        int32_t diff = static_cast<int32_t>(
            static_cast<uint32_t>(maxDamage) - static_cast<uint32_t>(damage));
        float numerator = static_cast<float>(diff);
        return byFraction(numerator / static_cast<float>(maxDamage));
    }

    float fractionLow() const { return fractionLow_; }
    float fractionMedium() const { return fractionMedium_; }
    float fractionHigh() const { return fractionHigh_; }

private:
    float fractionLow_;
    float fractionMedium_;
    float fractionHigh_;
};

// Crackiness.java:6-7 — the two vanilla preset instances.
inline constexpr Crackiness CRACKINESS_GOLEM{0.75F, 0.5F, 0.25F};
inline constexpr Crackiness CRACKINESS_WOLF_ARMOR{0.95F, 0.69F, 0.32F};

} // namespace mc

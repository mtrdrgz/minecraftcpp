#pragma once
#include <algorithm>
#include <cstdint>

#include "world/Difficulty.h"

// ---------------------------------------------------------------------------
// Port of net/minecraft/world/DifficultyInstance.java (Minecraft Java Edition
// 26.1.2). An @Immutable value object: the constructor takes the base
// Difficulty, the total/local game time (longs) and the moon brightness
// (float), computes a single `effectiveDifficulty` float once, and exposes a
// handful of pure accessors over it. World-free and fully deterministic — no
// RNG, no Level.
//
// Ported here verbatim (every value/formula from DifficultyInstance.java):
//   ctor                             DifficultyInstance.java:14-17
//   getDifficulty()                  DifficultyInstance.java:19-21
//   getEffectiveDifficulty()         DifficultyInstance.java:23-25
//   isHard()                         DifficultyInstance.java:27-29
//   isHarderThan(float)              DifficultyInstance.java:31-33
//   getSpecialMultiplier()           DifficultyInstance.java:35-41
//   calculateDifficulty(...)         DifficultyInstance.java:43-61
//
// NOT ported: nothing — the whole (pure) public surface is mirrored. The class
// has no other members (getDisplayName/info etc. live on Difficulty, not here).
//
// 1:1 float-math notes:
//   * effectiveDifficulty is a Java `float`; every intermediate below is a
//     `float` (`scale`, `globalScale`, `localScale`), so we use `float` here.
//   * (float)totalGameTime / (float)localGameTime are Java long->float casts.
//     long->float is round-to-nearest (JLS 5.1.2); static_cast<float>(int64_t)
//     in C++ is the same IEEE conversion.
//   * Mth.clamp(float,float,float) (Mth.java:101-103) is
//     `value < min ? min : Math.min(value, max)`. For the FINITE inputs this
//     gate sweeps (no NaN), Math.min(a,b)==std::min mirrors exactly; we keep
//     the exact ternary shape so -0.0/ordering match Java.
//   * base.getId() (Difficulty.java:29-31) is the int id 0..3; the final
//     `base.getId() * scale` is int*float => float (the int is promoted).
//   * literals -72000.0F, 1440000.0F, 3600000.0F, 0.75F, 0.25F, etc. copied
//     verbatim from the source (DifficultyInstance.java:49-60).
// ---------------------------------------------------------------------------

namespace mc {

// Java: Mth.clamp(float,float,float) — Mth.java:101-103.
//   return value < min ? min : Math.min(value, max);
// std::min(value,max) returns `value < max ? value : max` which, for finite
// non-NaN floats (everything this gate feeds), equals Java's Math.min(float).
inline float difficultyInstanceClampF(float value, float min, float max) noexcept {
    return value < min ? min : std::min(value, max);
}

// Java: DifficultyInstance.calculateDifficulty(...) — DifficultyInstance.java:43-61.
// base is the enum; totalGameTime/localGameTime are Java longs; moonBrightness
// is a Java float. Returns the `effectiveDifficulty` float.
inline float difficultyInstanceCalculate(Difficulty base,
                                         int64_t totalGameTime,
                                         int64_t localGameTime,
                                         float moonBrightness) noexcept {
    if (base == Difficulty::PEACEFUL) {
        return 0.0F;
    }

    bool isHard = (base == Difficulty::HARD);
    float scale = 0.75F;
    float globalScale =
        difficultyInstanceClampF(
            (static_cast<float>(totalGameTime) + -72000.0F) / 1440000.0F, 0.0F, 1.0F) *
        0.25F;
    scale += globalScale;
    float localScale = 0.0F;
    localScale += difficultyInstanceClampF(
                      static_cast<float>(localGameTime) / 3600000.0F, 0.0F, 1.0F) *
                  (isHard ? 1.0F : 0.75F);
    localScale += difficultyInstanceClampF(moonBrightness * 0.25F, 0.0F, globalScale);
    if (base == Difficulty::EASY) {
        localScale *= 0.5F;
    }

    scale += localScale;
    // base.getId() is int; int * float promotes the int to float (JLS 5.6.2).
    return static_cast<float>(difficultyGetId(base)) * scale;
}

// Java: DifficultyInstance — the @Immutable value object (DifficultyInstance.java).
// Constructed once, then queried via the pure accessors below.
struct DifficultyInstance {
    Difficulty base;
    float effectiveDifficulty;

    // Java ctor — DifficultyInstance.java:14-17.
    DifficultyInstance(Difficulty base_, int64_t totalGameTime, int64_t localGameTime,
                       float moonBrightness) noexcept
        : base(base_),
          effectiveDifficulty(
              difficultyInstanceCalculate(base_, totalGameTime, localGameTime, moonBrightness)) {}

    // Java: getDifficulty() — DifficultyInstance.java:19-21.
    Difficulty getDifficulty() const noexcept { return base; }

    // Java: getEffectiveDifficulty() — DifficultyInstance.java:23-25.
    float getEffectiveDifficulty() const noexcept { return effectiveDifficulty; }

    // Java: isHard() — DifficultyInstance.java:27-29.
    //   return this.effectiveDifficulty >= Difficulty.HARD.ordinal();
    // Difficulty.HARD.ordinal() == 3 (ordinal == id for this enum). The compare
    // promotes the int 3 to float (3.0F).
    bool isHard() const noexcept {
        return effectiveDifficulty >= static_cast<float>(3);
    }

    // Java: isHarderThan(float) — DifficultyInstance.java:31-33.
    bool isHarderThan(float requiredDifficulty) const noexcept {
        return effectiveDifficulty > requiredDifficulty;
    }

    // Java: getSpecialMultiplier() — DifficultyInstance.java:35-41.
    float getSpecialMultiplier() const noexcept {
        if (effectiveDifficulty < 2.0F) {
            return 0.0F;
        } else {
            return effectiveDifficulty > 4.0F ? 1.0F : (effectiveDifficulty - 2.0F) / 2.0F;
        }
    }
};

} // namespace mc

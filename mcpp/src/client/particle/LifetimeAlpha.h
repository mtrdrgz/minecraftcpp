#pragma once

// 1:1 port of the PURE alpha-over-lifetime math from
// net.minecraft.client.particle.Particle.LifetimeAlpha (Minecraft 26.1.2).
//
// The record itself (Particle.java:205-220):
//
//   public record LifetimeAlpha(float startAlpha, float endAlpha,
//                               float startAtNormalizedAge, float endAtNormalizedAge) {
//      public static final LifetimeAlpha ALWAYS_OPAQUE = new LifetimeAlpha(1.0F, 1.0F, 0.0F, 1.0F);
//
//      public boolean isOpaque() {
//         return this.startAlpha >= 1.0F && this.endAlpha >= 1.0F;
//      }
//
//      public float currentAlphaForAge(final int age, final int lifetime, final float partialTickTime) {
//         if (Mth.equal(this.startAlpha, this.endAlpha)) {
//            return this.startAlpha;
//         }
//         float timeNormalized = Mth.inverseLerp((age + partialTickTime) / lifetime,
//                                                this.startAtNormalizedAge, this.endAtNormalizedAge);
//         return Mth.clampedLerp(timeNormalized, this.startAlpha, this.endAlpha);
//      }
//   }
//
// This is a self-contained, deterministic, world-free, GL-free float computation —
// nothing here touches Blaze3D, the ClientLevel, or any registry. Every operation is
// plain 32-bit IEEE-754 arithmetic, so a faithful float replication is bit-exact.
//
// 1:1 notes on the arithmetic:
//   * Mth.equal(float,float)         = Math.abs(b - a) < 1.0E-5F        (Mth.java:157-159)
//   * (age + partialTickTime)        — int age WIDENED to float, then float add
//   * ... / lifetime                 — int lifetime WIDENED to float, then float divide
//   * Mth.inverseLerp(float)         = (value - min) / (max - min)      (Mth.java:330-332)
//     (the FLOAT overload is selected: all three args are float)
//   * Mth.clampedLerp(float)         = factor<0 ? min : factor>1 ? max : lerp(factor,min,max)
//                                                                        (Mth.java:117-123)
//   * Mth.lerp(float)                = p0 + a * (p1 - p0)               (Mth.java:532-534)
// We reuse the certified mth:: helpers for equal / clampedLerpF / lerpF and inline the
// float inverseLerp body (the shared Mth.h only carries the double overload; the Java
// method resolves the float overload here, which is the same closed-form expression).
//
// Certified bit-for-bit by lifetime_alpha_parity (ground truth:
// tools/LifetimeAlphaParity.java, which drives the REAL Particle.LifetimeAlpha record).

#include "../../world/level/levelgen/Mth.h"

namespace mc::client::particle {

namespace mth = mc::levelgen::mth;

struct LifetimeAlpha {
    float startAlpha;
    float endAlpha;
    float startAtNormalizedAge;
    float endAtNormalizedAge;

    // public static final LifetimeAlpha ALWAYS_OPAQUE = new LifetimeAlpha(1.0F, 1.0F, 0.0F, 1.0F);
    static constexpr LifetimeAlpha alwaysOpaque() { return LifetimeAlpha{1.0F, 1.0F, 0.0F, 1.0F}; }

    // public boolean isOpaque()
    bool isOpaque() const { return startAlpha >= 1.0F && endAlpha >= 1.0F; }

    // public float currentAlphaForAge(int age, int lifetime, float partialTickTime)
    float currentAlphaForAge(int age, int lifetime, float partialTickTime) const {
        if (mth::equal(startAlpha, endAlpha)) {
            return startAlpha;
        }
        float value = (static_cast<float>(age) + partialTickTime) / static_cast<float>(lifetime);
        float timeNormalized = (value - startAtNormalizedAge) / (endAtNormalizedAge - startAtNormalizedAge);
        return mth::clampedLerpF(timeNormalized, startAlpha, endAlpha);
    }
};

}  // namespace mc::client::particle

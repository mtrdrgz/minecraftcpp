#pragma once

// 1:1 port of the pure static helper
//   net.minecraft.world.entity.animal.happyghast.HappyGhast$HappyGhastLookControl
//       .wrapDegrees90(float angle)
// from Minecraft 26.1.2 (HappyGhast.java:653-664).
//
// This is the angle-folding helper the happy ghast's LookControl uses to wrap a
// degree value into the half-open interval [-45, 45) modulo 90 (a quarter turn).
//
// Source (HappyGhast.java:653-664):
//     public static float wrapDegrees90(final float angle) {
//        float normalizedAngle = angle % 90.0F;
//        if (normalizedAngle >= 45.0F) {
//           normalizedAngle -= 90.0F;
//        }
//        if (normalizedAngle < -45.0F) {
//           normalizedAngle += 90.0F;
//        }
//        return normalizedAngle;
//     }
//
// 1:1 TRAPS reproduced exactly:
//   * Java's float `%` operator is IEEE-754 remainder-by-truncation, i.e. C's
//     std::fmodf (NOT std::remainderf, which rounds to nearest). It keeps the sign
//     of the dividend, so `(-30.0f) % 90.0f == -30.0f`. We use std::fmodf verbatim.
//   * The whole body runs in `float` precision; the two compare-and-adjust steps
//     (`-= 90.0F`, `+= 90.0F`) are float subtractions/additions. No double widening.
//   * NaN propagates: `NaN % 90 == NaN`; both `NaN >= 45.0F` and `NaN < -45.0F` are
//     false, so NaN is returned unchanged — matching Java's float comparison rules.
//   * +/-Infinity: `Inf % 90.0F == NaN` (fmodf(Inf, finite) == NaN), so an infinite
//     input yields NaN, exactly as the JVM does.
//
// Certified by happy_ghast_wrap_degrees90_parity (ground truth:
// tools/HappyGhastWrapDegrees90Parity.java, which reflectively invokes the REAL
// HappyGhast$HappyGhastLookControl.wrapDegrees90).

#include <cmath>

namespace mc::world::entity::animal::happyghast {

// HappyGhast.java:653-664 — public static float wrapDegrees90(final float angle).
inline float wrapDegrees90(float angle) {
    float normalizedAngle = std::fmodf(angle, 90.0F);  // Java float `%` == fmodf
    if (normalizedAngle >= 45.0F) {
        normalizedAngle -= 90.0F;
    }
    if (normalizedAngle < -45.0F) {
        normalizedAngle += 90.0F;
    }
    return normalizedAngle;
}

}  // namespace mc::world::entity::animal::happyghast

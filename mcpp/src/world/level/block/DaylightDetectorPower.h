#pragma once
#include <cmath>

#include "../../../util/ARGB.h"            // mc::argb::javaRoundF — certified Math.round(float)
#include "../levelgen/Mth.h"               // mc::levelgen::mth::{cos, clamp} — certified Mth LUT/clamp

// ---------------------------------------------------------------------------
// 1:1 C++ port of the pure analog-power computation of
//   net.minecraft.world.level.block.DaylightDetectorBlock (Minecraft Java
//   Edition 26.1.2).
//
//   DaylightDetectorBlock.updateSignalStrength(...)   DaylightDetectorBlock.java:59-75
//
// updateSignalStrength reads the effective sky brightness, the SUN_ANGLE
// environment attribute (in DEGREES) and the INVERTED block-state property,
// folds them into the redstone POWER (0..15) that the detector emits, then
// writes it back to the world. Everything BUT the world reads/writes is a pure
// scalar pipeline:
//
//   int target = level.getEffectiveSkyBrightness(pos);              // -11..15
//   float sunAngle = SUN_ANGLE_deg * (float)(Math.PI / 180.0);      // radians
//   if (inverted) {
//      target = 15 - target;
//   } else if (target > 0) {
//      float offset = sunAngle < (float)Math.PI ? 0.0F : (float)(Math.PI * 2);
//      sunAngle += (offset - sunAngle) * 0.2F;
//      target = Math.round(target * Mth.cos(sunAngle));
//   }
//   target = Mth.clamp(target, 0, 15);
//
// The two NON-trivial operations — the cosine and the rounding — are taken from
// the REAL Minecraft/JDK routines, never re-derived here:
//   * Mth.cos is the table-based approximation SIN[(int)((long)(x*SIN_SCALE +
//     16384.0) & 65535L)]  (NOT std::cos). We call the certified
//     mc::levelgen::mth::cos (same LUT, certified by mth_parity).
//   * Math.round(float) is the JDK-8010430 bit-twiddle (NOT (int)floor(a+0.5f)).
//     We call the certified mc::argb::javaRoundF (certified by argb_parity).
// The remaining glue is trivial scalar arithmetic.
//
// 1:1 TRAPS reproduced exactly:
//   * `target` is an int; for non-inverted detectors with target>0 the product
//     `target * Mth.cos(sunAngle)` is int*float -> FLOAT, then Math.round narrows
//     it to int. Doing the round in double, or using std::lround/lrintf, drifts.
//   * `sunAngle` is a FLOAT throughout. The degrees->radians factor is the
//     single-precision (float)(Math.PI/180.0); the comparison threshold is the
//     single-precision (float)Math.PI; the wrap target is (float)(Math.PI*2);
//     the smoothing `+= (offset - sunAngle) * 0.2F` is FLOAT arithmetic. Only at
//     the Mth.cos call does the float widen to double (Mth.cos takes a double).
//     Computing any of this in double changes the LUT index and the rounding.
//   * The branch ladder is exactly: inverted FIRST (target = 15 - target, NO
//     cosine), else-if target>0 (cosine path), else fall through (target left as
//     the raw, possibly negative, sky brightness). target<=0 and non-inverted
//     skips the cosine entirely and relies on the final clamp to fold it to 0.
//   * `offset` selection uses `<` (strict): sunAngle exactly (float)Math.PI takes
//     the 2*PI branch. Matches `sunAngle < (float)Math.PI ? 0 : 2*PI`.
//   * Final Mth.clamp(target, 0, 15) is the INT clamp (min(max(v,0),15)); it is
//     what turns the negative pre-cosine `15 - target` / raw-brightness values
//     and the cosine's negative results into a valid 0..15 power.
//
// This is a pure function of (effectiveSkyBrightness, sunAngleDegrees, inverted)
// -> int power; it has NO world/registry/GL coupling once those three scalars
// are supplied, so it ports standalone and is gated against ground truth that
// drives the REAL Mth.cos + Math.round through the identical pipeline.
// ---------------------------------------------------------------------------

namespace mc::block_daylight {

namespace mth = mc::levelgen::mth;

// Java: the pure scalar core of DaylightDetectorBlock.updateSignalStrength.
//   effectiveSkyBrightness : level.getEffectiveSkyBrightness(pos)  (int)
//   sunAngleDegrees        : SUN_ANGLE environment attribute       (float, deg)
//   inverted               : INVERTED block-state property         (bool)
// Returns the redstone POWER (0..15) the detector would set.
inline int computePower(int effectiveSkyBrightness, float sunAngleDegrees, bool inverted) {
    // Java's Math.PI is the double 3.141592653589793; use the literal so we do
    // not depend on the <cmath> M_PI macro (not defined on llvm-mingw without
    // _USE_MATH_DEFINES) and so we match Math.PI bit-for-bit.
    constexpr double JAVA_PI = 3.141592653589793;
    int target = effectiveSkyBrightness;
    // float sunAngle = deg * (float)(Math.PI / 180.0);
    float sunAngle = sunAngleDegrees * static_cast<float>(JAVA_PI / 180.0);

    if (inverted) {
        target = 15 - target;
    } else if (target > 0) {
        // float offset = sunAngle < (float)Math.PI ? 0.0F : (float)(Math.PI * 2);
        float offset = sunAngle < static_cast<float>(JAVA_PI) ? 0.0F : static_cast<float>(JAVA_PI * 2);
        // sunAngle += (offset - sunAngle) * 0.2F;   (all float)
        sunAngle += (offset - sunAngle) * 0.2F;
        // target = Math.round(target * Mth.cos(sunAngle));
        //   Mth.cos(double): sunAngle widens float->double for the call; returns float.
        //   target * <float> : int * float -> float; Math.round(float) -> int.
        float cosVal = mth::cos(static_cast<double>(sunAngle));
        target = mc::argb::javaRoundF(static_cast<float>(target) * cosVal);
    }

    // target = Mth.clamp(target, 0, 15);  (int clamp)
    return mth::clamp(target, 0, 15);
}

} // namespace mc::block_daylight

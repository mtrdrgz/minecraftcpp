#pragma once

// 1:1 port of net.minecraft.world.entity.projectile.Projectile.lerpRotation
// (26.1.2) — Projectile.java:333-343. The static helper that smooths a projectile's
// rotation toward a target angle each tick: it first un-wraps the previous angle
// (rotO) into the same +/-180-degree neighbourhood as the target angle (rot) using
// two while loops adding/subtracting 360, then linearly interpolates 20% of the way
// from rotO to rot. Used by Projectile.updateRotation for both xRot and yRot.
//
// PURE & self-contained: a protected static helper taking only two floats
// (rotO, rot). No entity/world/level/registry/random state. The only library call
// is Mth.lerp(float, float, float) (Mth.java:532-534) == mc::levelgen::mth::lerpF.
//
// Certified byte-exact by projectile_lerp_rotation_parity against the real
// net.minecraft class (tools/ProjectileLerpRotationParity.java).
//
// 1:1 traps preserved:
//   * ALL arithmetic is single-precision float (rotO, rot, the 360/180 literals and
//     the 0.2F lerp weight). Doing it in double would diverge on the raw IEEE-754
//     bits — the C++ literals are written as float (360.0f, 180.0f) to match.
//   * the first while loop uses strict `< -180.0F`; the second uses `>= 180.0F`
//     (asymmetric bounds) — an off-by-one-bracket mis-port would shift exactly at
//     the +/-180 boundary, so both comparisons are reproduced verbatim.
//   * rotO is the loop-mutated local (Java's parameter is non-final and reassigned);
//     we take it by value and mutate the copy, never the caller's value.
//   * Mth.lerp(0.2F, rotO, rot) resolves to the FLOAT overload (alpha is 0.2F), i.e.
//     rotO + 0.2F * (rot - rotO) computed in float — NOT the double lerp.
//   * for a target angle far from rotO (e.g. rot - rotO == 540), the loop runs
//     multiple times; the repeated +/-360.0F float additions are bit-reproduced.

#include "../../level/levelgen/Mth.h"

namespace mc {

namespace mth = mc::levelgen::mth;

// Projectile.lerpRotation — Projectile.java:333-343.
inline float projectileLerpRotation(float rotO, float rot) {
    while (rot - rotO < -180.0F) {
        rotO -= 360.0F;
    }

    while (rot - rotO >= 180.0F) {
        rotO += 360.0F;
    }

    return mth::lerpF(0.2F, rotO, rot);
}

} // namespace mc

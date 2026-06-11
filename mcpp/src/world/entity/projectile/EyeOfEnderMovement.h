#pragma once

// 1:1 port of net.minecraft.world.entity.projectile.EyeOfEnder.updateDeltaMovement
// (26.1.2) — EyeOfEnder.java:133-145. The per-tick velocity update that steers an
// Eye of Ender toward its stronghold target: it eases its horizontal speed toward
// the remaining horizontal distance, slows down and sinks once it is within one
// block horizontally, and nudges its vertical velocity up or down by a fixed step.
//
// PURE & self-contained: a private static helper taking only three Vec3 arguments
// (oldMovement, position, target). No entity/world/level/registry/random state.
// All arithmetic is double-precision; the only library call is Mth.lerp(double,
// double, double) and Math.sqrt (via Vec3.length / Vec3.horizontalDistance).
//
// Certified byte-exact by eye_of_ender_movement_parity against the real
// net.minecraft class (tools/EyeOfEnderMovementParity.java).
//
// 1:1 traps preserved:
//   * Mth.lerp(0.0025, horizontalDistance(old), horizontalLength) — start + d*(end-start),
//     NOT a clamped lerp.
//   * two DIFFERENT sqrt sources: horizontalDelta.length() (sqrt of x^2+0+z^2) and
//     oldMovement.horizontalDistance() (sqrt of x^2+z^2) — same value here but distinct
//     call sites; ported verbatim.
//   * the `horizontalLength < 1.0` branch scales BOTH wantedSpeed and movementY by 0.8.
//   * vertical step sign uses `position.y - oldMovement.y < target.y` (note: subtracts
//     oldMovement.y from position.y — an easy-to-mis-port expression) → +1.0 / -1.0.
//   * scale(wantedSpeed / horizontalLength): when horizontalLength == 0 this is a
//     division by zero producing IEEE inf/NaN; reproduced exactly (no guard in Java).
//   * final y = movementY + (wantedMovementY - movementY) * 0.015.

#include "../../phys/Vec3.h"

namespace mc {

namespace mth = mc::levelgen::mth;

// EyeOfEnder.updateDeltaMovement — EyeOfEnder.java:133-145.
inline Vec3 eyeOfEnderUpdateDeltaMovement(const Vec3& oldMovement, const Vec3& position, const Vec3& target) {
    Vec3 horizontalDelta{target.x - position.x, 0.0, target.z - position.z};
    double horizontalLength = horizontalDelta.length();
    double wantedSpeed = mth::lerp(0.0025, oldMovement.horizontalDistance(), horizontalLength);
    double movementY = oldMovement.y;
    if (horizontalLength < 1.0) {
        wantedSpeed *= 0.8;
        movementY *= 0.8;
    }

    double wantedMovementY = position.y - oldMovement.y < target.y ? 1.0 : -1.0;
    return horizontalDelta.scale(wantedSpeed / horizontalLength)
        .add(0.0, movementY + (wantedMovementY - movementY) * 0.015, 0.0);
}

} // namespace mc

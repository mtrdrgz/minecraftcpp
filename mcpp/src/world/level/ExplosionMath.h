// ExplosionMath.h — bit-exact port of the PURE explosion damage/knockback falloff
// math from Minecraft 26.1.2.
//
// Source (verbatim, NOT invented):
//   * net.minecraft.world.level.ExplosionDamageCalculator.getEntityDamageAmount
//       (26.1.2/src/.../ExplosionDamageCalculator.java:31-37)
//   * net.minecraft.world.level.ServerExplosion.hurtEntities  (the knockback
//       magnitude sub-expression)
//       (26.1.2/src/.../ServerExplosion.java:184,198)
//
// In 26.1.2 the legacy net.minecraft.world.level.Explosion class is now an
// interface; the concrete math lives in ServerExplosion (knockback) and
// ExplosionDamageCalculator (damage). This header ports ONLY the self-contained
// floating-point falloff, extracted from those two methods. The entity iteration,
// the raycast exposure term getSeenPercent (ClipContext-coupled), the
// per-attribute knockbackResistance/knockbackMultiplier lookups and the block
// raycast (calculateExplodedPositions) are NOT ported here — see
// unportedMethods / the parity test notes.
//
// Bit-exactness contract (mirrors the Java exactly):
//   - `doubleRadius` is a FLOAT (radius * 2.0F). It is promoted to double only
//     when it participates in double arithmetic (Math.sqrt(...) / doubleRadius and
//     ... * 7.0 * doubleRadius). We keep the param as float and let the usual
//     arithmetic conversions promote it, exactly as the JVM does.
//   - `exposure` is a FLOAT (the seenPercent), promoted to double in
//     (1.0 - dist) * exposure.
//   - `dist` is a double: Math.sqrt(distanceToSqr) / doubleRadius. Math.sqrt is
//     correctly rounded == std::sqrt, so this is bit-exact.
//   - The damage return applies a narrowing (float) cast to the double result.
//     Inputs are finite/physical so no JLS-5.1.3 saturation edge is exercised.
//
// All literals (2.0F, 7.0, 2.0, 1.0) are taken verbatim from the Java.

#ifndef MCPP_WORLD_LEVEL_EXPLOSION_MATH_H
#define MCPP_WORLD_LEVEL_EXPLOSION_MATH_H

#include <cmath>

namespace mc::explosion {

// ExplosionDamageCalculator.getEntityDamageAmount, lines 32-36 (verbatim):
//   float doubleRadius = explosion.radius() * 2.0F;
//   ... double dist = Math.sqrt(entity.distanceToSqr(center)) / doubleRadius;
//   double pow = (1.0 - dist) * exposure;
//   return (float)((pow * pow + pow) / 2.0 * 7.0 * doubleRadius + 1.0);
//
// Here `distanceToSqr` is the caller-supplied squared distance between the
// entity position and the explosion center (Entity.distanceToSqr(Vec3),
// Entity.java:1786-1791 — plain xd*xd+yd*yd+zd*zd, a double). We take it directly
// so the entity-position plumbing stays out of this pure helper.
//
// `radius` is the explosion radius (ServerExplosion.radius, a float); `exposure`
// is the seenPercent in [0,1] (a float). doubleRadius is recomputed here exactly
// as in the Java (radius * 2.0F, kept as float).
inline float getEntityDamageAmount(float radius, double distanceToSqr, float exposure) {
    float doubleRadius = radius * 2.0F;
    double dist = std::sqrt(distanceToSqr) / doubleRadius; // doubleRadius promoted to double
    double pow = (1.0 - dist) * exposure;                  // exposure promoted to double
    return (float)((pow * pow + pow) / 2.0 * 7.0 * doubleRadius + 1.0);
}

// ServerExplosion.hurtEntities knockback magnitude, lines 184 + 198 (verbatim):
//   double dist = Math.sqrt(entity.distanceToSqr(this.center)) / doubleRadius;
//   ...
//   double knockbackPower = (1.0 - dist) * exposure * knockbackMultiplier
//                           * (1.0 - knockbackResistance);
//
// Returns the SCALAR knockback magnitude. The direction (normalized
// entityOrigin - center) and the Vec3 scale/push are not part of the pure
// falloff and are intentionally excluded. `doubleRadius` is again radius * 2.0F
// (float). `exposure` (float), `knockbackMultiplier` (float, default 1.0F from
// ExplosionDamageCalculator.getKnockbackMultiplier) and `knockbackResistance`
// (double, the EXPLOSION_KNOCKBACK_RESISTANCE attribute, 0.0 when absent) are
// caller-supplied so no attribute/entity plumbing leaks into this helper.
inline double getKnockbackPower(float radius, double distanceToSqr, float exposure,
                                float knockbackMultiplier, double knockbackResistance) {
    float doubleRadius = radius * 2.0F;
    double dist = std::sqrt(distanceToSqr) / doubleRadius; // doubleRadius promoted to double
    return (1.0 - dist) * exposure * knockbackMultiplier * (1.0 - knockbackResistance);
}

// Convenience: the distance ratio used by both expressions, exposed for the
// parity gate. dist = Math.sqrt(distanceToSqr) / (radius * 2.0F).
// (ServerExplosion.java:184 / ExplosionDamageCalculator.java:34.)
inline double distanceRatio(float radius, double distanceToSqr) {
    float doubleRadius = radius * 2.0F;
    return std::sqrt(distanceToSqr) / doubleRadius;
}

} // namespace mc::explosion

#endif // MCPP_WORLD_LEVEL_EXPLOSION_MATH_H

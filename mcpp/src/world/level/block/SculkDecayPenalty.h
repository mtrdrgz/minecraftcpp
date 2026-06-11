#pragma once
#include <cmath>
#include <cstdint>

// ---------------------------------------------------------------------------
// 1:1 C++ port of the pure sculk-charge decay-penalty arithmetic of
//   net.minecraft.world.level.block.SculkBlock (Minecraft Java Edition 26.1.2).
//
//   SculkBlock.getDecayPenalty(SculkSpreader, BlockPos, BlockPos, int)
//                                                          SculkBlock.java:60-66
//
// This is the `private static` helper that decides, when a sculk charge cursor
// decays away from its catalyst, HOW MANY charge points to subtract this tick:
//
//   private static int getDecayPenalty(SculkSpreader spreader, BlockPos pos,
//                                      BlockPos originPos, int charge) {
//      int noGrowthRadius = spreader.noGrowthRadius();
//      float outerDistanceSquared =
//          Mth.square((float)Math.sqrt(pos.distSqr(originPos)) - noGrowthRadius);
//      int maxReachSquared = Mth.square(24 - noGrowthRadius);
//      float distanceFactor = Math.min(1.0F, outerDistanceSquared / maxReachSquared);
//      return Math.max(1, (int)(charge * distanceFactor * 0.5F));
//   }
//
// It is a PURE function of (noGrowthRadius, pos, originPos, charge) — no world,
// registry, RNG or GL coupling — so it ports standalone. (`spreader` is only a
// carrier for the int noGrowthRadius; we pass that scalar directly.)
//
// 1:1 TRAPS reproduced exactly:
//
//  * pos.distSqr(originPos) is Vec3i.distToLowCornerSqr: with INT block coords
//    it is computed in DOUBLE — dx*dx + dy*dy + dz*dz where each dx is the
//    integer coordinate difference promoted to double (Vec3i.java:216-221). We
//    compute it in double from the int deltas; doing it in float or int diverges.
//
//  * (float)Math.sqrt(...) : Math.sqrt is the correctly-rounded double sqrt; the
//    result is then NARROWED TO FLOAT. std::sqrt(double) is likewise correctly
//    rounded (sqrt is one of the few IEEE-exact ops, identical across Java/C++),
//    and we narrow to float exactly as the Java cast does. Keeping it double
//    past this point would change the next float subtraction's rounding.
//
//  * `(float)Math.sqrt(...) - noGrowthRadius` is FLOAT minus INT -> FLOAT (the
//    int is converted to float). Mth.square(float) = x*x in FLOAT.
//
//  * maxReachSquared = Mth.square(24 - noGrowthRadius) is the INT overload:
//    (24 - noGrowthRadius) computed in int, squared in int.
//
//  * outerDistanceSquared / maxReachSquared is FLOAT / INT: Java promotes the int
//    divisor to float and divides in FLOAT (single precision). Dividing in
//    double, or as int, changes the factor.
//
//  * Math.min(1.0F, factor) is the FLOAT min (clamps the factor to <= 1).
//
//  * (int)(charge * distanceFactor * 0.5F): charge*factor is INT*FLOAT -> FLOAT,
//    *0.5F stays FLOAT, then the float->int cast TRUNCATES TOWARD ZERO and, per
//    JLS 5.1.3, SATURATES on out-of-range / NaN (NaN -> 0). We reproduce the
//    saturating narrowing rather than relying on C++'s UB for out-of-range
//    float->int. For the reachable domain (charge in 0..1000, factor in [0,1])
//    the product is well within int range, but we keep the exact JLS semantics.
//
//  * Math.max(1, ...) floors the result at 1 — a cursor that triggers the decay
//    branch always loses at least one charge point.
//
// NOTE: maxReachSquared is (24 - noGrowthRadius)^2; it is 0 only at
// noGrowthRadius == 24, which the real spreader configs never produce
// (level=4, worldgen=1). The parity sweep stays within the reachable,
// nonzero-divisor domain, matching the values the game can actually generate.
// ---------------------------------------------------------------------------

namespace mc::block_sculk {

// Java JLS 5.1.3 narrowing float -> int: truncate toward zero, saturate on
// overflow, NaN -> 0. C++ static_cast<int> on out-of-range / NaN is UB, so we
// implement the Java semantics explicitly. (Mirrors the certified Mth narrowing
// trap used elsewhere in the port.)
inline int javaFloatToInt(float f) {
    if (std::isnan(f)) return 0;
    if (f >= 2147483648.0f) return 2147483647;          // >= 2^31 saturates to INT_MAX
    if (f <= -2147483648.0f) return -2147483647 - 1;    // <= -2^31 saturates to INT_MIN
    return static_cast<int>(f);                          // in range: truncate toward zero
}

// Java: Mth.square overloads.
inline float squareF(float x) { return x * x; }
inline int   squareI(int x)   { return x * x; }

// 1:1 port of SculkBlock.getDecayPenalty. `noGrowthRadius` is
// spreader.noGrowthRadius(); the two BlockPos are given by their int components.
inline int getDecayPenalty(int noGrowthRadius,
                           int posX, int posY, int posZ,
                           int originX, int originY, int originZ,
                           int charge) {
    // pos.distSqr(originPos): Vec3i.distToLowCornerSqr, computed in double from
    // the int coordinate deltas (dx*dx + dy*dy + dz*dz).
    double dx = static_cast<double>(posX) - static_cast<double>(originX);
    double dy = static_cast<double>(posY) - static_cast<double>(originY);
    double dz = static_cast<double>(posZ) - static_cast<double>(originZ);
    double distSqr = dx * dx + dy * dy + dz * dz;

    // float outerDistanceSquared = Mth.square((float)Math.sqrt(distSqr) - noGrowthRadius);
    float outerDistanceSquared =
        squareF(static_cast<float>(std::sqrt(distSqr)) - static_cast<float>(noGrowthRadius));

    // int maxReachSquared = Mth.square(24 - noGrowthRadius);
    int maxReachSquared = squareI(24 - noGrowthRadius);

    // float distanceFactor = Math.min(1.0F, outerDistanceSquared / maxReachSquared);
    // float / int -> float (int divisor promoted to float, single-precision div).
    float distanceFactor = outerDistanceSquared / static_cast<float>(maxReachSquared);
    if (!(distanceFactor < 1.0F)) {
        // Math.min(1.0F, x): returns 1.0F when x >= 1.0F; returns NaN when x is
        // NaN (then propagates through the (int) cast to 0 below). Using the
        // negated `<` keeps NaN flowing through unchanged, matching Math.min.
        distanceFactor = std::isnan(distanceFactor) ? distanceFactor : 1.0F;
    }

    // return Math.max(1, (int)(charge * distanceFactor * 0.5F));
    float scaled = static_cast<float>(charge) * distanceFactor * 0.5F;
    int penalty = javaFloatToInt(scaled);
    return penalty > 1 ? penalty : 1;
}

} // namespace mc::block_sculk

#pragma once

// 1:1 C++ port of the PURE distance + acceptance-threshold math inside the REAL
// decompiled 26.1.2 classes:
//   net.minecraft.world.level.levelgen.structure.templatesystem.LinearPosTest
//       boolean test(BlockPos inTemplatePos, BlockPos worldPos,
//                    BlockPos worldReference, RandomSource random)  [line 37-41]
//   net.minecraft.world.level.levelgen.structure.templatesystem.AxisAlignedLinearPosTest
//       boolean test(BlockPos inTemplatePos, BlockPos worldPos,
//                    BlockPos worldReference, RandomSource random)  [line 41-49]
//
// Both PosRuleTest.test() implementations are:
//     int   dist = <Manhattan / axis-projected distance worldPos->worldReference>;
//     float rnd  = random.nextFloat();
//     return rnd <= Mth.clampedLerp(Mth.inverseLerp(dist, minDist, maxDist),
//                                   minChance, maxChance);
//
// The ONLY impure part is the trailing `random.nextFloat()` comparison. The
// distance and the acceptance THRESHOLD are pure: integer/float arithmetic over
// the two positions and the four immutable rule fields. There are NO world
// writes, NO registry/datapack, and (in the part ported here) NO RandomSource.
// This header ports exactly those two pure quantities; the gate
// (linear_pos_test_math_parity) drives the REAL classes' private fields via
// reflection and calls the REAL net.minecraft.util.Mth to produce ground truth.
//
// 1:1 TRAPS faithfully reproduced:
//   * LinearPosTest distance == Vec3i.distManhattan (Vec3i.java:223-228):
//       float xd = Math.abs(dx); ...; return (int)(xd + yd + zd);
//     The per-axis |delta| is computed as a 32-bit int, WIDENED to float, summed
//     in float, then TRUNCATED back to int. For coordinates beyond 2^24 the float
//     mantissa loses precision exactly as Java does — we keep the float round-trip.
//   * AxisAlignedLinearPosTest distance (AxisAlignedLinearPosTest.java:42-46):
//       Direction d = Direction.get(POSITIVE, axis) -> normal (1,0,0)/(0,1,0)/(0,0,1);
//       float xd = Math.abs((worldPos.x - worldRef.x) * d.getStepX()); ... ;
//       int dist = (int)(xd + yd + zd);
//     delta*step is an int multiply (step is 0 or 1 here, so this selects the
//     on-axis |delta|); Math.abs(int) is widened to float, summed, truncated.
//   * Threshold == Mth.clampedLerp(Mth.inverseLerp((float)dist, (float)minDist,
//     (float)maxDist), minChance, maxChance) — single-precision throughout:
//       inverseLerp(v,min,max) = (v - min) / (max - min)            [Mth.java:330-332]
//       lerp(a,p0,p1)          = p0 + a * (p1 - p0)                 [Mth.java:532-534]
//       clampedLerp(f,min,max) = f<0 ? min : f>1 ? max : lerp(f,..) [Mth.java:117-123]
//     NOTE the `>1.0F` upper compare uses the FLOAT literal; ctor requires
//     minDist < maxDist so (max-min) is never 0 here.
//
// Certified byte-exact by linear_pos_test_math_parity
// (tools/LinearPosTestMathParity.java).

#include <cmath>
#include <cstdint>

namespace mc::levelgen::structure::templatesystem {

// net.minecraft.core.Direction.Axis ordinals (Direction.Axis enum): X=0, Y=1, Z=2.
enum class Axis : int32_t { X = 0, Y = 1, Z = 2 };

// ── net.minecraft.util.Mth (the three float ops these tests use) ──────────────

// Mth.lerp(float,float,float) — Mth.java:532-534.
inline float mthLerpF(float alpha, float p0, float p1) noexcept { return p0 + alpha * (p1 - p0); }

// Mth.inverseLerp(float,float,float) — Mth.java:330-332.
inline float mthInverseLerpF(float value, float min, float max) noexcept {
    return (value - min) / (max - min);
}

// Mth.clampedLerp(float,float,float) — Mth.java:117-123.
inline float mthClampedLerpF(float factor, float min, float max) noexcept {
    if (factor < 0.0F) return min;
    return factor > 1.0F ? max : mthLerpF(factor, min, max);
}

// ── distance helpers ─────────────────────────────────────────────────────────

// Vec3i.distManhattan(Vec3i) — Vec3i.java:223-228. Per-axis |delta| is a 32-bit
// int Math.abs, widened to float, summed in float, truncated to int. Java int
// subtraction wraps on overflow (two's complement) -> route the deltas through
// int32_t so a-b matches Java even at extreme coordinates.
inline int distManhattan(int x1, int y1, int z1, int x2, int y2, int z2) noexcept {
    // Math.abs(int) of the (wrapping) int difference, then widen to float.
    // Do every subtraction in uint32_t so it wraps like Java int (signed overflow
    // is UB in C++ and clang -O2 will miscompile it), then reinterpret as int32_t.
    int32_t dx = static_cast<int32_t>(static_cast<uint32_t>(x2) - static_cast<uint32_t>(x1));
    int32_t dy = static_cast<int32_t>(static_cast<uint32_t>(y2) - static_cast<uint32_t>(y1));
    int32_t dz = static_cast<int32_t>(static_cast<uint32_t>(z2) - static_cast<uint32_t>(z1));
    // Math.abs(Integer.MIN_VALUE) == Integer.MIN_VALUE in Java; reproduce via
    // uint negation so the C++ std::abs UB at INT_MIN cannot bite.
    auto javaAbs = [](int32_t v) -> int32_t {
        return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v;
    };
    float xd = static_cast<float>(javaAbs(dx));
    float yd = static_cast<float>(javaAbs(dy));
    float zd = static_cast<float>(javaAbs(dz));
    return static_cast<int>(xd + yd + zd); // C-style float->int truncation == (int) in Java
}

// AxisAlignedLinearPosTest distance — AxisAlignedLinearPosTest.java:42-46.
// Direction.get(POSITIVE, axis) yields the positive-facing direction whose
// normal step is 1 on `axis` and 0 elsewhere. So:
//   xd = |（worldPos.x - worldRef.x) * stepX|, etc., summed in float, truncated.
inline int axisAlignedDist(Axis axis,
                           int wx, int wy, int wz,
                           int rx, int ry, int rz) noexcept {
    uint32_t stepX = axis == Axis::X ? 1u : 0u;
    uint32_t stepY = axis == Axis::Y ? 1u : 0u;
    uint32_t stepZ = axis == Axis::Z ? 1u : 0u;
    // (worldPos - worldRef) * step — the subtract and multiply both done in
    // uint32_t (Java int wraps two's-complement; C++ signed overflow is UB).
    int32_t px = static_cast<int32_t>((static_cast<uint32_t>(wx) - static_cast<uint32_t>(rx)) * stepX);
    int32_t py = static_cast<int32_t>((static_cast<uint32_t>(wy) - static_cast<uint32_t>(ry)) * stepY);
    int32_t pz = static_cast<int32_t>((static_cast<uint32_t>(wz) - static_cast<uint32_t>(rz)) * stepZ);
    // Math.abs(int) via uint negation so Integer.MIN_VALUE maps to itself (as Java).
    auto javaAbs = [](int32_t v) -> int32_t {
        return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v;
    };
    float xd = static_cast<float>(javaAbs(px));
    float yd = static_cast<float>(javaAbs(py));
    float zd = static_cast<float>(javaAbs(pz));
    return static_cast<int>(xd + yd + zd);
}

// ── acceptance threshold (the RHS of the `rnd <=` comparison) ─────────────────
//
// threshold = Mth.clampedLerp(Mth.inverseLerp(dist, minDist, maxDist),
//                             minChance, maxChance)
// with dist/minDist/maxDist promoted to float (the Java overloads chosen by
// `inverseLerp(int, int, int)` -> the float overload, since there is no int
// overload; ints widen to float). minChance/maxChance are float fields.
inline float acceptanceThreshold(int dist, int minDist, int maxDist,
                                 float minChance, float maxChance) noexcept {
    float factor = mthInverseLerpF(static_cast<float>(dist),
                                   static_cast<float>(minDist),
                                   static_cast<float>(maxDist));
    return mthClampedLerpF(factor, minChance, maxChance);
}

// Convenience: full LinearPosTest.test() threshold for a position pair.
inline float linearPosThreshold(int wx, int wy, int wz, int rx, int ry, int rz,
                                int minDist, int maxDist, float minChance, float maxChance) noexcept {
    return acceptanceThreshold(distManhattan(wx, wy, wz, rx, ry, rz),
                               minDist, maxDist, minChance, maxChance);
}

// Convenience: full AxisAlignedLinearPosTest.test() threshold for a position pair.
inline float axisAlignedPosThreshold(Axis axis, int wx, int wy, int wz, int rx, int ry, int rz,
                                     int minDist, int maxDist, float minChance, float maxChance) noexcept {
    return acceptanceThreshold(axisAlignedDist(axis, wx, wy, wz, rx, ry, rz),
                               minDist, maxDist, minChance, maxChance);
}

} // namespace mc::levelgen::structure::templatesystem

#pragma once

// 1:1 C++ port of the COMPLETE boolean predicate
//   net.minecraft.world.level.levelgen.structure.templatesystem.AxisAlignedLinearPosTest
//       boolean test(BlockPos inTemplatePos, BlockPos worldPos,
//                    BlockPos worldReference, RandomSource random)   [AxisAlignedLinearPosTest.java:40-49]
//
// The REAL method is:
//     Direction direction = Direction.get(POSITIVE, this.axis);
//     float xd = Math.abs((worldPos.getX() - worldReference.getX()) * direction.getStepX());
//     float yd = Math.abs((worldPos.getY() - worldReference.getY()) * direction.getStepY());
//     float zd = Math.abs((worldPos.getZ() - worldReference.getZ()) * direction.getStepZ());
//     int dist = (int)(xd + yd + zd);
//     float rnd = random.nextFloat();
//     return rnd <= Mth.clampedLerp(Mth.inverseLerp(dist, this.minDist, this.maxDist),
//                                   this.minChance, this.maxChance);
//
// This is DISTINCT from the already-gated LinearPosTestMath.h, which ports ONLY
// the pure distance + acceptance THRESHOLD (it explicitly excludes the
// `random.nextFloat()` draw and the final `rnd <=` comparison — "in the part
// ported here NO RandomSource"). Here we port the WHOLE predicate: the RNG draw
// that ADVANCES the shared RandomSource stream AND the boolean result. That is
// the surface a real StructureTemplate processor sees when it calls
// ProcessorRule.test() (ProcessorRule.java:63). The deterministic threshold is
// reused verbatim from LinearPosTestMath.h so the two gates can never disagree.
//
// 1:1 TRAPS faithfully reproduced:
//   * Distance: Direction.get(POSITIVE, axis) yields a unit normal whose step is
//     1 on `axis` and 0 elsewhere (Direction.java EAST/UP/SOUTH normals
//     (1,0,0)/(0,1,0)/(0,0,1)). So `(worldPos - worldRef) * step` is an int
//     multiply selecting the on-axis signed delta; Math.abs(int) is widened to
//     float, the three floats are summed, then TRUNCATED back to int with the
//     Java `(int)` cast. Java int subtract/multiply WRAP two's-complement
//     (signed overflow is UB in C++ -O2) -> all integer ops route through
//     uint32_t. Math.abs(Integer.MIN_VALUE) == Integer.MIN_VALUE (reproduced via
//     uint negation). See axisAlignedDist() in LinearPosTestMath.h.
//   * Threshold: Mth.clampedLerp(Mth.inverseLerp((float)dist, (float)minDist,
//     (float)maxDist), minChance, maxChance) — single-precision throughout, with
//     the `>1.0F` upper compare using the FLOAT literal. The ctor invariant
//     minDist < maxDist guarantees (max-min) != 0. See acceptanceThreshold().
//   * The RNG draw: `float rnd = random.nextFloat();` is taken FIRST-arg-evaluated
//     in Java exactly once per call (the threshold RHS does NOT draw). We draw
//     `rnd` from the SAME certified RandomSource (world/level/levelgen/RandomSource)
//     before evaluating the threshold, matching Java left-to-right evaluation of
//     `rnd <= clampedLerp(...)` (the RHS has no side effects, so order is moot,
//     but we keep the textual order for clarity).
//   * Result: the boolean `rnd <= threshold` (a float <= compare; NaN threshold
//     would yield false, but the ctor invariant + finite chances preclude NaN).
//
// Certified byte-exact by axis_aligned_linear_pos_predicate_parity
// (tools/AxisAlignedLinearPosTestPredicateParity.java).

#include "LinearPosTestMath.h"

#include "world/level/levelgen/RandomSource.h"

#include <cstdint>

namespace mc::levelgen::structure::templatesystem {

// 1:1 port of net.minecraft.world.level.levelgen.structure.templatesystem.AxisAlignedLinearPosTest.
// Immutable rule record (minChance, maxChance, minDist, maxDist, axis). The ctor
// in Java throws IllegalArgumentException when minDist >= maxDist; callers here
// only ever construct legal rules (the gate enforces minDist < maxDist), so we
// mirror the field layout without re-throwing.
class AxisAlignedLinearPosTest {
public:
    AxisAlignedLinearPosTest(float minChance, float maxChance, int32_t minDist, int32_t maxDist, Axis axis) noexcept
        : m_minChance(minChance), m_maxChance(maxChance), m_minDist(minDist), m_maxDist(maxDist), m_axis(axis) {}

    // AxisAlignedLinearPosTest.test(BlockPos inTemplatePos, BlockPos worldPos,
    //                               BlockPos worldReference, RandomSource random)
    // — AxisAlignedLinearPosTest.java:40-49. `inTemplatePos` is unused by the real
    // body (it is part of the PosRuleTest signature only), so it is omitted here.
    // `random` advances exactly one nextFloat() draw per call.
    bool test(int32_t worldX, int32_t worldY, int32_t worldZ,
              int32_t refX, int32_t refY, int32_t refZ,
              mc::levelgen::RandomSource& random) const {
        int dist = axisAlignedDist(m_axis, worldX, worldY, worldZ, refX, refY, refZ);
        float rnd = random.nextFloat();
        return rnd <= acceptanceThreshold(dist, m_minDist, m_maxDist, m_minChance, m_maxChance);
    }

private:
    float m_minChance;
    float m_maxChance;
    int32_t m_minDist;
    int32_t m_maxDist;
    Axis m_axis;
};

} // namespace mc::levelgen::structure::templatesystem

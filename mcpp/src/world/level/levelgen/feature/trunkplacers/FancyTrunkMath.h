#pragma once
// ---------------------------------------------------------------------------
// Header-only 1:1 port of the PURE static math helpers of
//   net.minecraft.world.level.levelgen.feature.trunkplacers.FancyTrunkPlacer
//   (Minecraft Java Edition 26.1.2).
//
// These four helpers are self-contained pure math — NO world/chunk state, NO
// RandomSource, NO registries — so they can be gated byte-for-bit against the
// real decompiled class. The world-coupled body (placeTrunk/makeLimb/makeBranches)
// is NOT ported here; it lives in TreeGen.cpp and is exercised by the full-chunk
// decorate gate.
//
// Ported (exchanged with mcpp/tools/FancyTrunkMathParity.java):
//   treeShape(int height, int y)              FancyTrunkPlacer.java:171-186
//   getSteps(int dx,int dy,int dz)            FancyTrunkPlacer.java:126-131
//   getLogAxis(start.., block..) -> Axis      FancyTrunkPlacer.java:133-147
//   trimBranches(int height,int localY)       FancyTrunkPlacer.java:149-151
//
// 1:1 TRAPS (why this is worth a dedicated gate):
//   * treeShape early-out is a FLOAT compare:  `y < height * 0.3F`. `height*0.3F`
//     is a float; `y` is widened to float; the result is NOT truncated to int.
//     (The in-engine TreeGen.cpp wrote `y < (int)(height*0.3f)` — that truncates
//     the threshold and diverges, e.g. height=7,y=2: float 2.0<2.1 -> true, but
//     int 2<2 -> false. This standalone port restores the exact Java semantics.)
//   * Mth.sqrt(x) == (float)Math.sqrt((double)x): the float arg is widened to a
//     double, square-rooted (IEEE correctly-rounded — NOT a HotSpot-intrinsic),
//     then narrowed back to float. Reproduced as (float)std::sqrt((double)v).
//   * In Java, Mth.sqrt(radius*radius - adjacent*adjacent) is evaluated BEFORE the
//     `adjacent == 0` / `abs(adjacent) >= radius` branches and discarded in those
//     cases. Order here matches Java exactly (the sqrt argument can be negative in
//     the abs>=radius branch -> NaN -> then overwritten/returned-around, harmless,
//     but we keep the structure identical so any future edit stays faithful).
//   * trimBranches compares int `localY` to DOUBLE `height * 0.2` (0.2 has no exact
//     float/double rep). `height` is widened to double; no truncation.
//   * getSteps uses Mth.abs (== Math.abs, two's-complement; abs(INT_MIN)==INT_MIN)
//     and Math.max nesting.
//   * getLogAxis returns the Direction.Axis ORDINAL: X=0, Y=1, Z=2
//     (Direction.Axis enum order, Direction.java). Ties (xdiff==zdiff>0) pick X.
// ---------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mc::levelgen::trunkplacers {

// Direction.Axis ordinals (enum declaration order in net.minecraft.core.Direction).
enum class Axis : int32_t { X = 0, Y = 1, Z = 2 };

// Mth.abs(int) — Math.abs (FancyTrunkPlacer uses Mth.abs in getSteps).
// Two's-complement: abs(INT_MIN) == INT_MIN (Java/JDK behaviour, reproduced).
inline int32_t mthAbs(int32_t v) {
    // Math.abs(int): return (a < 0) ? -a : a;  — -INT_MIN overflows to INT_MIN.
    return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v;
}

// Mth.sqrt(float) == (float)Math.sqrt(x) — widen to double, sqrt, narrow.
inline float mthSqrt(float x) {
    return static_cast<float>(std::sqrt(static_cast<double>(x)));
}

// FancyTrunkPlacer.treeShape(int height, int y) — FancyTrunkPlacer.java:171-186.
//   if (y < height * 0.3F) return -1.0F;            // FLOAT compare, not int
//   float radius   = height / 2.0F;
//   float adjacent = radius - y;
//   float distance = Mth.sqrt(radius*radius - adjacent*adjacent);  // computed first
//   if (adjacent == 0.0F) distance = radius;
//   else if (Math.abs(adjacent) >= radius) return 0.0F;
//   return distance * 0.5F;
inline float treeShape(int32_t height, int32_t y) {
    if (static_cast<float>(y) < static_cast<float>(height) * 0.3f) {
        return -1.0f;
    }
    const float radius = static_cast<float>(height) / 2.0f;
    const float adjacent = radius - static_cast<float>(y);
    float distance = mthSqrt(radius * radius - adjacent * adjacent);
    if (adjacent == 0.0f) {
        distance = radius;
    } else if (std::fabs(adjacent) >= radius) {
        return 0.0f;
    }
    return distance * 0.5f;
}

// FancyTrunkPlacer.getSteps(BlockPos) — FancyTrunkPlacer.java:126-131.
//   int absX = Mth.abs(pos.getX()); ...
//   return Math.max(absX, Math.max(absY, absZ));
inline int32_t getSteps(int32_t dx, int32_t dy, int32_t dz) {
    const int32_t absX = mthAbs(dx);
    const int32_t absY = mthAbs(dy);
    const int32_t absZ = mthAbs(dz);
    return std::max(absX, std::max(absY, absZ));
}

// FancyTrunkPlacer.getLogAxis(startPos, blockPos) — FancyTrunkPlacer.java:133-147.
//   Axis axis = Y;
//   int xdiff = abs(blockX - startX);
//   int zdiff = abs(blockZ - startZ);
//   int maxdiff = max(xdiff, zdiff);
//   if (maxdiff > 0) axis = (xdiff == maxdiff) ? X : Z;
//   return axis;
inline Axis getLogAxis(int32_t startX, int32_t startZ, int32_t blockX, int32_t blockZ) {
    Axis axis = Axis::Y;
    // Java int subtraction wraps (two's complement); compute in uint32 to reproduce it without C++
    // signed-overflow UB (blockX - startX overflows when startX == INT_MIN), THEN take Mth.abs.
    const int32_t xdiff = mthAbs(static_cast<int32_t>(static_cast<uint32_t>(blockX) - static_cast<uint32_t>(startX)));
    const int32_t zdiff = mthAbs(static_cast<int32_t>(static_cast<uint32_t>(blockZ) - static_cast<uint32_t>(startZ)));
    const int32_t maxdiff = std::max(xdiff, zdiff);
    if (maxdiff > 0) {
        axis = (xdiff == maxdiff) ? Axis::X : Axis::Z;
    }
    return axis;
}

// FancyTrunkPlacer.trimBranches(int height, int localY) — FancyTrunkPlacer.java:149-151.
//   return localY >= height * 0.2;   // DOUBLE compare (0.2 is double)
inline bool trimBranches(int32_t height, int32_t localY) {
    return static_cast<double>(localY) >= static_cast<double>(height) * 0.2;
}

} // namespace mc::levelgen::trunkplacers

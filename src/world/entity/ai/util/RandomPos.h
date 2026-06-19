#pragma once

// 1:1 port of net.minecraft.world.entity.ai.util.RandomPos — the pure, level-free
// geometry helpers. These are static, self-contained: they consume only a
// RandomSource and Mth, and emit an integer BlockPos. NO world/level/registry/GL
// access, no entity tick state.
//
// The faithful traps live here:
//   - `Mth.atan2(zDir, xDir) - (float)(Math.PI / 2)` : atan2 is the table-LUT
//     double approximation (NOT std::atan2), then a *float* HALF_PI is subtracted
//     from a double — the cast happens before the subtract, widened back to double.
//   - `(2.0F * random.nextFloat() - 1.0F) * maxXzRadiansFromDir` : the jitter is
//     computed in float (nextFloat, the 2.0F/1.0F literals) then promoted to double
//     by the `* maxXzRadiansFromDir` multiply. Order matters for the low bits.
//   - `Mth.lerp(Math.sqrt(random.nextDouble()), min, max) * Mth.SQRT_OF_TWO` :
//     SQRT_OF_TWO is `(float)Math.sqrt(2.0F)` (a float constant), multiplied into a
//     double — a float-vs-double trap.
//   - `Math.sin/cos(yRadians)` here are java.lang.Math (double, NOT the Mth LUT).
//   - `BlockPos.containing(xt, yt, zt)` floors each double via Mth.floor =
//     (int)Math.floor(double) — truncation toward negative infinity, not toward 0.
//   - The null gate: `abs(xt) > maxHorizontalDist || abs(zt) > maxHorizontalDist`
//     rejects (returns no-pos). Must be a strict `>`.
//   - `generateRandomDirection`: three independent `nextInt(2*d+1) - d` draws; the
//     RandomSource draw ORDER (x, y, z) is load-bearing.

#include <cmath>
#include <cstdint>
#include <optional>

#include "world/level/levelgen/RandomSource.h"
#include "world/level/levelgen/Mth.h"

namespace mc::entity::ai {

// Minimal integer BlockPos result. RandomPos only ever produces a coordinate
// triple here; we avoid pulling in core/BlockPos to keep this header pure.
struct BlockPosResult {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;

    bool operator==(const BlockPosResult& o) const { return x == o.x && y == o.y && z == o.z; }
};

// Mth.SQRT_OF_TWO = (float)Math.sqrt(2.0F)  — Mth.java:26. A float constant.
inline constexpr float SQRT_OF_TWO = static_cast<float>(1.4142135623730951);  // (float)sqrt(2.0)

// RandomPos.generateRandomDirection(random, horizontalDist, verticalDist)
// RandomPos.java:18-23. Three nextInt draws in x,y,z order.
inline BlockPosResult generateRandomDirection(
    mc::levelgen::RandomSource& random, int32_t horizontalDist, int32_t verticalDist) {
    int32_t xt = random.nextInt(2 * horizontalDist + 1) - horizontalDist;
    int32_t yt = random.nextInt(2 * verticalDist + 1) - verticalDist;
    int32_t zt = random.nextInt(2 * horizontalDist + 1) - horizontalDist;
    return BlockPosResult{xt, yt, zt};
}

// RandomPos.generateRandomDirectionWithinRadians(...) — RandomPos.java:25-46.
// Returns std::nullopt when |xt| or |zt| exceeds maxHorizontalDist (Java's null).
inline std::optional<BlockPosResult> generateRandomDirectionWithinRadians(
    mc::levelgen::RandomSource& random,
    double minHorizontalDist,
    double maxHorizontalDist,
    int32_t verticalDist,
    int32_t flyingHeight,
    double xDir,
    double zDir,
    double maxXzRadiansFromDir) {
    namespace mth = mc::levelgen::mth;

    // double yRadiansCenter = Mth.atan2(zDir, xDir) - (float)(Math.PI / 2);
    // HALF_PI is the float (float)(Math.PI/2.0); subtracted from a double atan2.
    double yRadiansCenter = mth::atan2(zDir, xDir) - static_cast<double>(mth::HALF_PI);

    // double yRadians = yRadiansCenter + (2.0F * random.nextFloat() - 1.0F) * maxXzRadiansFromDir;
    float jitter = 2.0F * random.nextFloat() - 1.0F;  // float arithmetic
    double yRadians = yRadiansCenter + static_cast<double>(jitter) * maxXzRadiansFromDir;

    // double dist = Mth.lerp(Math.sqrt(random.nextDouble()), min, max) * Mth.SQRT_OF_TWO;
    double dist = mth::lerp(std::sqrt(random.nextDouble()), minHorizontalDist, maxHorizontalDist)
                  * static_cast<double>(SQRT_OF_TWO);

    // double xt = -dist * Math.sin(yRadians);   double zt = dist * Math.cos(yRadians);
    // NOTE: java.lang.Math.sin/cos here (double), NOT the Mth sine LUT.
    double xt = -dist * std::sin(yRadians);
    double zt = dist * std::cos(yRadians);

    if (!(std::fabs(xt) > maxHorizontalDist) && !(std::fabs(zt) > maxHorizontalDist)) {
        // int yt = random.nextInt(2 * verticalDist + 1) - verticalDist + flyingHeight;
        int32_t yt = random.nextInt(2 * verticalDist + 1) - verticalDist + flyingHeight;
        // BlockPos.containing(xt, yt, zt) — Mth.floor on x and z (yt already int).
        return BlockPosResult{mth::floor(xt), yt, mth::floor(zt)};
    }
    return std::nullopt;
}

}  // namespace mc::entity::ai

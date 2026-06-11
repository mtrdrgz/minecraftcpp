#pragma once

// 1:1 C++ port of the PURE, world-free deterministic ring-position skeleton of the
// REAL decompiled 26.1.2 method
//   net.minecraft.world.level.chunk.ChunkGeneratorStructureState
//       CompletableFuture<List<ChunkPos>> generateRingPositions(
//           Holder<StructureSet> structureSet,
//           ConcentricRingsStructurePlacement placement)            [lines 107-167]
//
// This is the algorithm that lays out the concentric *rings of strongholds* around
// world origin. Per ring index i it computes a candidate chunk:
//
//     double dist = 4 * distance + distance * circle * 6
//                   + (random.nextDouble() - 0.5) * (distance * 2.5);
//     int initialX = (int) Math.round(Math.cos(angle) * dist);
//     int initialZ = (int) Math.round(Math.sin(angle) * dist);
//     ... // (a biome search may then snap this to a nearby preferred biome)
//     angle += (Math.PI * 2) / spread;
//     if (++positionInCircle == spread) {
//        circle++;
//        positionInCircle = 0;
//        spread += 2 * spread / (circle + 1);
//        spread = Math.min(spread, count - i);
//        angle += random.nextDouble() * Math.PI * 2.0;
//     }
//
// The biome-search step (BiomeSource.findBiomeHorizontal) is the ONLY world-coupled
// part; when it finds nothing the real code falls back to `new ChunkPos(initialX,
// initialZ)` — i.e. this pure skeleton. The companion ground truth
// (ConcentricRingsPositionsParity.java) drives the REAL generateRingPositions with a
// BiomeSource whose findBiomeHorizontal returns null, so the emitted ChunkPos values
// are exactly this skeleton, computed by the REAL RNG + the REAL spread/circle/angle
// evolution. This header recomputes and compares bit-for-bit.
//
// The seeding: ChunkGeneratorStructureState.createForNormal sets
// concentricRingsSeed = levelSeed; generateRingPositions does
//   RandomSource random = RandomSource.create();   // a LegacyRandomSource
//   random.setSeed(this.concentricRingsSeed);
//   double angle = random.nextDouble() * Math.PI * 2.0;
//
// 1:1 TRAPS faithfully replicated below:
//   * RandomSource.create() returns a LegacyRandomSource (java.util.Random LCG);
//     nextDouble() is BitRandomSource.nextDouble == (((long)next(26)<<27)+next(27))
//     * 1.110223E-16F  — a double initialised from a FLOAT literal. We use the
//     certified mc::levelgen::LegacyRandomSource (world/level/levelgen/RandomSource.cpp)
//     so the stream is byte-identical.
//   * `4 * distance + distance * circle * 6` and `count - i` are Java *int*
//     arithmetic (two's-complement wrap) computed BEFORE the promotion to double in
//     `dist`; `distance * 2.5` promotes `distance` (int) to double. We mirror the
//     exact operand types and evaluation order.
//   * `spread += 2 * spread / (circle + 1)` is integer arithmetic with Java operator
//     precedence: `(2 * spread) / (circle + 1)`, truncating toward zero. `circle`
//     was just incremented, so the divisor is the NEW circle + 1.
//   * `Math.round(double)` == (long) Math.floor(x + 0.5d); the final `(int)` cast is
//     a Java long->int narrowing (low 32 bits). For physical strongholds dist is a
//     few thousand, so the long fits an int, but we still narrow exactly.
//   * angle accumulates in a double across the whole loop; the per-ring increment is
//     `(Math.PI * 2) / spread` (Math.PI * 2 first, then / spread), and the
//     end-of-circle bump is `random.nextDouble() * Math.PI * 2.0`. Order of the two
//     nextDouble() draws per iteration matters: the `dist` jitter draw happens
//     BEFORE the end-of-circle bump draw.
//   * Math.cos / Math.sin: Java may use HotSpot StrictMath intrinsics that can
//     differ from std::cos/std::sin by <=1 ULP. Because the value is immediately
//     rounded to an int and the nextDouble() jitter makes an exact .5 boundary a
//     measure-zero event, the rounded ints match; the parity gate enforces this
//     empirically (mismatches=0 over the full count=128 stronghold battery for many
//     seeds).
//
// No registries, no datapacks, no GL. Depends only on the certified RandomSource.

#include <cmath>
#include <cstdint>
#include <vector>

#include "../../RandomSource.h"  // mc::levelgen::LegacyRandomSource (certified)

namespace mc::levelgen::structure::placement {

// net.minecraft.world.level.ChunkPos — only (x,z) chunk coordinates are produced.
struct RingChunkPos {
    int32_t x{};
    int32_t z{};
    constexpr bool operator==(const RingChunkPos&) const = default;
};

// Java int arithmetic wraps on overflow (two's complement). C++ signed overflow is
// UB, so route the int ops that can in principle overflow through uint32_t. For the
// physical stronghold inputs (distance=32, count=128, spread=3) nothing overflows,
// but this keeps the port byte-identical for any int inputs the gate feeds.
constexpr int32_t imul(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}
constexpr int32_t iadd(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
constexpr int32_t isub(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

// java.lang.Math.round(double) == (long) Math.floor(a + 0.5d) — Math.java.
// (Java's actual impl special-cases via bit tricks but is numerically identical to
// floor(a+0.5) for all finite a we encounter; dist here is a few thousand.)
inline int64_t mathRound(double a) noexcept {
    return static_cast<int64_t>(std::floor(a + 0.5));
}

// ChunkGeneratorStructureState.generateRingPositions(...) — the PURE skeleton, with
// the biome search treated as "found nothing" so each ring resolves to
// new ChunkPos(initialX, initialZ). `concentricRingsSeed` is the level seed (for
// createForNormal); `distance`, `count`, `spread` come from the structure_set JSON
// (strongholds: 32 / 128 / 3). Returns `count` chunk positions in order.
inline std::vector<RingChunkPos> generateRingPositionsSkeleton(
    int64_t concentricRingsSeed, int32_t distance, int32_t count, int32_t spread) {
    std::vector<RingChunkPos> out;
    if (count == 0) {
        return out;  // CompletableFuture.completedFuture(List.of())
    }
    out.reserve(static_cast<std::size_t>(count));

    static constexpr double kPi = 3.141592653589793;  // Math.PI

    LegacyRandomSource random(0);          // RandomSource.create() -> LegacyRandomSource
    random.setSeed(concentricRingsSeed);   // random.setSeed(this.concentricRingsSeed)
    double angle = random.nextDouble() * kPi * 2.0;
    int32_t positionInCircle = 0;
    int32_t circle = 0;

    for (int32_t i = 0; i < count; i++) {
        // double dist = 4 * distance + distance * circle * 6
        //               + (random.nextDouble() - 0.5) * (distance * 2.5);
        // Integer subexpressions (Java int math) computed first, then promoted.
        int32_t intPart = iadd(imul(4, distance), imul(imul(distance, circle), 6));
        double dist = static_cast<double>(intPart)
                      + (random.nextDouble() - 0.5) * (static_cast<double>(distance) * 2.5);
        int32_t initialX = static_cast<int32_t>(mathRound(std::cos(angle) * dist));
        int32_t initialZ = static_cast<int32_t>(mathRound(std::sin(angle) * dist));
        // RandomSource biomeSearchGenerator = random.fork();
        // LegacyRandomSource.fork() == new LegacyRandomSource(this.nextLong()), i.e. it
        // CONSUMES one nextLong() (two next(32) draws) from the stream every iteration,
        // even though the forked generator itself is only used by the (stubbed-out)
        // biome search. Omitting this desyncs the RNG after the first ring.
        random.nextLong();
        // (biome search -> null -> fall back to new ChunkPos(initialX, initialZ))
        out.push_back(RingChunkPos{initialX, initialZ});

        angle += (kPi * 2.0) / static_cast<double>(spread);
        if (++positionInCircle == spread) {
            circle++;
            positionInCircle = 0;
            // spread += 2 * spread / (circle + 1);  // (2*spread)/(circle+1), trunc
            spread = iadd(spread, imul(2, spread) / iadd(circle, 1));
            // spread = Math.min(spread, count - i);
            int32_t bound = isub(count, i);
            if (bound < spread) {
                spread = bound;
            }
            angle += random.nextDouble() * kPi * 2.0;
        }
    }

    return out;
}

}  // namespace mc::levelgen::structure::placement

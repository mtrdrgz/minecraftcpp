#pragma once

// 1:1 port of the PURE, RNG-driven static helper math in the REAL decompiled
// 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.RuinedPortalStructure
//
// Two of RuinedPortalStructure's private static helpers are fully self-contained
// (no level / chunk-generator / registry / datapack access at all). They are
// driven purely by a RandomSource plus int/float arithmetic, so they can be
// ported and certified byte-exact in isolation:
//
//   private static boolean sample(WorldgenRandom random, float limit)  [.java:152-158]
//       if (limit == 0.0F) return false;
//       return limit == 1.0F ? true : random.nextFloat() < limit;
//     -- NOTE the short-circuits: a draw is consumed ONLY when limit is neither
//        0.0F nor 1.0F. The comparison is a strict `<`. The parameter is declared
//        WorldgenRandom, but the body only calls nextFloat(); when WorldgenRandom
//        wraps a LegacyRandomSource, WorldgenRandom.next(bits) delegates straight
//        to LegacyRandomSource.next(bits) (WorldgenRandom.java:30-35), so the draw
//        sequence is byte-identical to calling LegacyRandomSource.nextFloat().
//
//   private static int getRandomWithinInterval(RandomSource random,
//                                              int minPreferred, int max)  [.java:227-229]
//       return minPreferred < max ? Mth.randomBetweenInclusive(random, minPreferred, max) : max;
//     -- delegates to Mth.randomBetweenInclusive (Mth.java:664-666):
//          return random.nextInt(maxInclusive - min + 1) + min;
//        which consumes a draw ONLY on the `minPreferred < max` branch; otherwise
//        returns `max` with NO draw. random.nextInt(bound) throws for bound <= 0,
//        but on the taken branch minPreferred < max => (max - minPreferred + 1) >= 2,
//        so the bound is always positive and no throw can occur.
//
// Both are byte-exact against ground truth from the REAL class via
// tools/RuinedPortalYSelectorParity.java (drives RuinedPortalStructure.sample and
// .getRandomWithinInterval reflectively over a REAL seeded RandomSource and emits
// a TSV; this header recomputes and compares). RNG: certified
// world/level/levelgen/RandomSource.h LegacyRandomSource.

#include <cstdint>

#include "world/level/levelgen/RandomSource.h"

namespace mc::levelgen::structure::ruinedportal {

// net.minecraft.util.Mth.randomBetweenInclusive(RandomSource, int, int) —
// Mth.java:664-666:
//   return random.nextInt(maxInclusive - min + 1) + min;
// Java int arithmetic wraps on two's-complement overflow; `min` and
// `maxInclusive` here come from finite, well-ordered intervals (max >= min so the
// bound is positive), so the subtraction/add cannot overflow for any real input.
inline int32_t mthRandomBetweenInclusive(mc::levelgen::RandomSource& random,
                                         int32_t minInclusive, int32_t maxInclusive) {
    return random.nextInt(maxInclusive - minInclusive + 1) + minInclusive;
}

// RuinedPortalStructure.sample(WorldgenRandom, float) — .java:152-158.
// Consumes a nextFloat() draw ONLY when limit is strictly between 0.0F and 1.0F.
inline bool sample(mc::levelgen::RandomSource& random, float limit) {
    if (limit == 0.0F) {
        return false;
    }
    return limit == 1.0F ? true : random.nextFloat() < limit;
}

// RuinedPortalStructure.getRandomWithinInterval(RandomSource, int, int) —
// .java:227-229. Draws a value only on the `minPreferred < max` branch; otherwise
// returns `max` with no RNG consumption.
inline int32_t getRandomWithinInterval(mc::levelgen::RandomSource& random,
                                       int32_t minPreferred, int32_t max) {
    return minPreferred < max ? mthRandomBetweenInclusive(random, minPreferred, max) : max;
}

} // namespace mc::levelgen::structure::ruinedportal

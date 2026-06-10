#pragma once

// 1:1 port of net.minecraft.util.LinearCongruentialGenerator (26.1.2) — the
// positional-random LCG used by BiomeManager's biome-zoom fiddle. Pure 64-bit
// two's-complement integer arithmetic with two compile-time constants. Certified
// by lcg_parity (tools/LcgParity.java).
//
// Java source:
//   public class LinearCongruentialGenerator {
//      private static final long MULTIPLIER = 6364136223846793005L;
//      private static final long INCREMENT  = 1442695040888963407L;
//      public static long next(long rval, final long c) {
//         rval *= rval * 6364136223846793005L + 1442695040888963407L;
//         return rval + c;
//      }
//   }
//
// The compound assignment expands to:
//   rval = rval * (rval * MULTIPLIER + INCREMENT);  return rval + c;
// All operations are Java `long` (signed 64-bit, wrapping two's-complement). We do
// the multiplies/adds on uint64_t (defined wrap-around) and reinterpret the bits
// back to int64_t, which is exactly Java's signed-long behavior.

#include <bit>
#include <cstdint>

namespace mc::util {

class LinearCongruentialGenerator {
public:
    static constexpr int64_t MULTIPLIER = static_cast<int64_t>(6364136223846793005ULL);
    static constexpr int64_t INCREMENT  = static_cast<int64_t>(1442695040888963407ULL);

    // long next(long rval, long c)
    static int64_t next(int64_t rval, int64_t c) {
        const uint64_t u = std::bit_cast<uint64_t>(rval);
        const uint64_t cu = std::bit_cast<uint64_t>(c);
        // rval *= rval * MULTIPLIER + INCREMENT;
        const uint64_t mixed = u * (u * static_cast<uint64_t>(MULTIPLIER)
                                    + static_cast<uint64_t>(INCREMENT));
        // return rval + c;
        return std::bit_cast<int64_t>(mixed + cu);
    }
};

} // namespace mc::util

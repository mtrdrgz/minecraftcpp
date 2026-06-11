#pragma once

// 1:1 port of net.minecraft.world.level.block.NetherVines (26.1.2).
//
// NetherVines is the small static helper shared by WeepingVinesBlock and
// TwistingVinesBlock for nether-vine growth. Two of its three members are pure:
//
//   * BONEMEAL_GROW_PROBABILITY_DECREASE_RATE = 0.826
//   * GROW_PER_TICK_PROBABILITY               = 0.1   (public, used by callers)
//   * getBlocksToGrowWhenBonemealed(RandomSource): a geometric-decay loop that
//     counts how many vine segments a bone-meal application adds. Starting from
//     probability 1.0, it keeps drawing random.nextDouble() and decrementing the
//     count's threshold by the 0.826 decay factor each success. This is the
//     piece under test here; it is RNG-stream-only and therefore byte-exact when
//     driven by the certified LegacyRandomSource (java.util.Random LCG).
//
// NOT PORTED here (intentionally, hard no-op / out-of-scope):
//   * isValidGrowthState(BlockState) -> state.isAir(): a one-line BlockState
//     predicate. It needs a live BlockState, which is outside this pure helper's
//     surface, so it is omitted rather than stubbed with a fake answer.

#include <cstdint>

#include "world/level/levelgen/RandomSource.h"

namespace mc::world::level::block {

class NetherVines {
public:
    // private static final double BONEMEAL_GROW_PROBABILITY_DECREASE_RATE = 0.826;
    static constexpr double BONEMEAL_GROW_PROBABILITY_DECREASE_RATE = 0.826;
    // public static final double GROW_PER_TICK_PROBABILITY = 0.1;
    static constexpr double GROW_PER_TICK_PROBABILITY = 0.1;

    // public static int getBlocksToGrowWhenBonemealed(RandomSource random) {
    //    double growProbabilty = 1.0;
    //    int count;
    //    for (count = 0; random.nextDouble() < growProbabilty; count++) {
    //       growProbabilty *= 0.826;
    //    }
    //    return count;
    // }
    static int32_t getBlocksToGrowWhenBonemealed(mc::levelgen::RandomSource& random) {
        double growProbabilty = 1.0;

        int32_t count;
        for (count = 0; random.nextDouble() < growProbabilty; count++) {
            growProbabilty *= 0.826;
        }

        return count;
    }
};

} // namespace mc::world::level::block

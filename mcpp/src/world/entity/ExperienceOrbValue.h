// 1:1 port of net.minecraft.world.entity.ExperienceOrb's pure integer
// experience-decomposition helpers (Minecraft 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/world/entity/ExperienceOrb.java
//
// This header ports the *pure, world-free, deterministic* integer math of
// ExperienceOrb. Everything here is a static `int -> int` (or `int -> list`)
// computation that does NOT touch the Level, entity data, RNG, ItemStacks, or
// any registry, so it can be gated byte-for-byte against the real class via
// reflection.
//
// ── getExperienceValue(int maxValue) (ExperienceOrb.java:346-368) ───────────
// A descending threshold ladder that returns the largest "canonical" orb value
// not exceeding maxValue. The vanilla thresholds (and their returned values)
// are, in order:
//   >= 2477 -> 2477
//   >= 1237 -> 1237
//   >=  617 ->  617
//   >=  307 ->  307
//   >=  149 ->  149
//   >=   73 ->   73
//   >=   37 ->   37
//   >=   17 ->   17
//   >=    7 ->    7
//   >=    3 ->    3
//   else    ->    1
// The comparisons are plain signed `int` `>=` (no overflow happens — the
// constants are positive and we only compare). For maxValue < 3 (including all
// negatives and INT_MIN) the method returns 1. For maxValue exactly on a
// boundary it returns that boundary. This is the value sequence used by
// ExperienceOrb.awardWithDirection (lines 191-199) to break a reward into orbs.
//
// ── splitIntoOrbs(int amount) (derived from awardWithDirection:191-199) ─────
// The deterministic decomposition loop of awardWithDirection, with the
// world-side merge/spawn stripped out. The merge step (tryMergeToExisting)
// only decides whether a *new entity* is created vs. an existing orb's count is
// bumped — it does NOT change the sequence of orb values produced, which is
// purely:
//     while (amount > 0) { v = getExperienceValue(amount); amount -= v; emit v; }
// We expose this as a pure helper so the C++ engine can compute the canonical
// orb breakdown of a reward. It is fully determined by getExperienceValue, so
// gating getExperienceValue byte-exact also validates every value this emits.
// (Java `int amount` here is always called with amount > 0 from award paths;
// for amount <= 0 the loop body never runs and the result is empty, matching
// the `while (amount > 0)` guard.)
//
// NOT PORTED HERE (entangled with entity/world/registry state — hard no-op /
// out of scope, NOT silently faked):
//   * getIcon()              — reads this.getValue() from synched entity data.
//   * award / awardWithDirection / tryMergeToExisting — need a ServerLevel,
//     RandomSource (level.getRandom().nextInt(40)) and entity spawning.
//   * repairPlayerItems / merge / setValue — need ItemStacks / live entity.

#pragma once

#include <cstdint>
#include <vector>

namespace mc {

class ExperienceOrbValue {
public:
    // ExperienceOrb.java:346-368 — static int -> int threshold ladder.
    // Signed int comparisons, exactly as in the Java source.
    static int32_t getExperienceValue(int32_t maxValue) {
        if (maxValue >= 2477) {
            return 2477;
        } else if (maxValue >= 1237) {
            return 1237;
        } else if (maxValue >= 617) {
            return 617;
        } else if (maxValue >= 307) {
            return 307;
        } else if (maxValue >= 149) {
            return 149;
        } else if (maxValue >= 73) {
            return 73;
        } else if (maxValue >= 37) {
            return 37;
        } else if (maxValue >= 17) {
            return 17;
        } else if (maxValue >= 7) {
            return 7;
        } else {
            return maxValue >= 3 ? 3 : 1;
        }
    }

    // Derived from ExperienceOrb.awardWithDirection (lines 191-199), world-side
    // merge/spawn removed. Returns the ordered sequence of orb values a reward
    // of `amount` is broken into. Empty for amount <= 0 (matches `while
    // (amount > 0)`).
    //
    // `amount` is decremented by `getExperienceValue(amount)` each iteration;
    // since that value is always >= 1 and <= amount whenever amount >= 1, the
    // loop strictly decreases and terminates.
    static std::vector<int32_t> splitIntoOrbs(int32_t amount) {
        std::vector<int32_t> out;
        while (amount > 0) {
            int32_t v = getExperienceValue(amount);
            amount -= v;
            out.push_back(v);
        }
        return out;
    }
};

} // namespace mc

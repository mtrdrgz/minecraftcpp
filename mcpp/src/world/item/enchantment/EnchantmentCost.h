#pragma once
//
// 1:1 port of the pure enchanting-table cost math from
//   net.minecraft.world.item.enchantment.EnchantmentHelper.getEnchantmentCost
//   (26.1.2, EnchantmentHelper.java lines 504-520).
//
// This is the math that computes the level requirement shown on each of the
// three enchanting-table slots given the surrounding bookshelf count and the
// table's per-attempt RandomSource. It is *pure*: no world/level access, no
// entity tick state, no registry/datapack lookups, no GL. Its only external
// touch is `itemStack.get(DataComponents.ENCHANTABLE)`, used solely as a
// boolean guard (the Enchantable.value() is never read), so it is captured
// here as the `hasEnchantable` parameter — keeping this helper self-contained.
//
// Verbatim Java body:
//
//   public static int getEnchantmentCost(RandomSource random, int slot,
//                                        int bookcases, ItemStack itemStack) {
//      Enchantable enchantable = itemStack.get(DataComponents.ENCHANTABLE);
//      if (enchantable == null) {
//         return 0;
//      }
//      if (bookcases > 15) {
//         bookcases = 15;
//      }
//      int selected = random.nextInt(8) + 1 + (bookcases >> 1) + random.nextInt(bookcases + 1);
//      if (slot == 0) {
//         return Math.max(selected / 3, 1);
//      } else {
//         return slot == 1 ? selected * 2 / 3 + 1 : Math.max(selected, bookcases * 2);
//      }
//   }
//
// 1:1 traps preserved here (do NOT "simplify"):
//   * RNG stream order: nextInt(8) is drawn BEFORE nextInt(bookcases + 1), and
//     the second draw's bound is the (clamped) bookcases+1 — both consume the
//     LegacyRandomSource exactly as production does.
//   * bookcases is clamped to 15 *after* potentially being used to bound the
//     second draw — i.e. the clamp happens before `selected` is computed, but
//     note the bound is `bookcases + 1` on the already-clamped value.
//   * Java `int` arithmetic: `selected / 3` and `selected * 2 / 3` are
//     truncating-toward-zero integer divisions evaluated left-to-right
//     (`(selected * 2) / 3`), NOT `selected * (2 / 3)`.
//   * Math.max(selected, bookcases * 2) for slot 2 (the high slot can be lifted
//     to twice the bookshelf count).
//   * Math.max(selected / 3, 1) floors slot 0 at 1.
//   * Non-enchantable item -> hard 0 and the RNG is NOT consumed (the guard
//     returns before any nextInt call).

#include "world/level/levelgen/RandomSource.h"

#include <algorithm>
#include <cstdint>

namespace mc::enchantment {

// Direct translation of EnchantmentHelper.getEnchantmentCost. `hasEnchantable`
// is `itemStack.get(DataComponents.ENCHANTABLE) != null`. When false, returns 0
// and leaves `random` untouched (the early return precedes both nextInt calls).
//
// All arithmetic is performed in 32-bit two's-complement to match Java `int`.
// The intermediate products here (selected <= ~22, bookcases*2 <= 30) cannot
// overflow, but we still funnel through int32_t to be explicit about widths.
inline int getEnchantmentCost(mc::levelgen::RandomSource& random, int slot,
                              int bookcases, bool hasEnchantable) {
    if (!hasEnchantable) {
        return 0;
    }

    if (bookcases > 15) {
        bookcases = 15;
    }

    // RNG stream order is load-bearing: first nextInt(8), then nextInt(bookcases+1).
    const int32_t firstDraw = random.nextInt(8);
    const int32_t secondDraw = random.nextInt(bookcases + 1);
    const int32_t selected =
        firstDraw + 1 + (bookcases >> 1) + secondDraw;

    if (slot == 0) {
        return std::max(selected / 3, 1);
    } else {
        // selected * 2 / 3 is ((selected * 2) / 3) with truncating int division.
        return slot == 1 ? selected * 2 / 3 + 1
                         : std::max(selected, bookcases * 2);
    }
}

}  // namespace mc::enchantment

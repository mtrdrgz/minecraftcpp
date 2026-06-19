#pragma once

// 1:1 port of the PURE static cost helper from
// net.minecraft.world.inventory.AnvilMenu (26.1.2).
//
// AnvilMenu itself is a heavy stateful menu (ItemCombinerMenu subclass: createResult()
// reaches into ItemStacks, EnchantmentHelper, DataComponents, DataSlot, the player, and
// ContainerLevelAccess). NONE of that is ported here. The ONLY self-contained static on
// AnvilMenu that touches purely primitive int arithmetic — and the one repeatedly applied
// to the result item's REPAIR_COST every time it leaves an anvil — is
// calculateIncreasedRepairCost(int). That is what we port and certify byte-exact.
//
// Ported helper (AnvilMenu.java line numbers in 26.1.2):
//   calculateIncreasedRepairCost(int baseCost)   :276-278
//       return (int)Math.min(baseCost * 2L + 1L, 2147483647L);
//
// 1:1 TRAP (the whole reason this is worth gating):
//   * baseCost is a Java `int`. The multiply `baseCost * 2L` promotes baseCost to `long`
//     FIRST (because the literal 2L is long), so the doubling happens in 64-bit and does
//     NOT wrap at 32 bits. Then `+ 1L` is also long. So for any 32-bit `int` input the
//     intermediate `baseCost * 2L + 1L` is exact in 64-bit (no overflow: 2*INT_MAX+1 =
//     4294967295 fits easily in long).
//   * Math.min(long, long) keeps it clamped at 2_147_483_647L = Integer.MAX_VALUE.
//   * The outer `(int)` is a long->int NARROWING cast. After the min-clamp the value is in
//     [LONG_MIN..2147483647]; for the realistic non-negative repair-cost domain it lands in
//     [0..2147483647] and the cast is lossless. For NEGATIVE baseCost the clamp is a no-op
//     (negative < MAX) and the long->int cast simply truncates the low 32 bits (JLS 5.1.3
//     narrowing of an integral type: keep the low 32 bits, two's-complement). We reproduce
//     ALL of this exactly: do the arithmetic in int64_t, std::min against the literal, then
//     a defined int64->int32 truncation (modular wrap, NOT C++ implementation-defined-UB:
//     we go through uint32_t so the low-32-bit two's-complement result is portable).
//
// Verified bit-for-bit against the REAL AnvilMenu.calculateIncreasedRepairCost by
// anvil_cost_parity (tools/AnvilMenuCostParity.java).

#include <algorithm>
#include <cstdint>

namespace mc::world::inventory {

// AnvilMenu.java:276-278
//   public static int calculateIncreasedRepairCost(final int baseCost) {
//      return (int)Math.min(baseCost * 2L + 1L, 2147483647L);
//   }
//
// `baseCost` arrives as a 32-bit value. `baseCost * 2L + 1L` is computed in 64-bit because
// the literal operands are `long` (Java binary numeric promotion). Math.min(long,long)
// against Integer.MAX_VALUE clamps the upper end. The final `(int)` cast is a long->int
// narrowing that keeps the low 32 bits as a two's-complement value (JLS 5.1.3).
inline int32_t calculateIncreasedRepairCost(int32_t baseCost) {
    // baseCost promoted to int64_t (matches Java promoting `int` to `long` before *2L).
    const int64_t doubledPlusOne = static_cast<int64_t>(baseCost) * 2LL + 1LL;
    const int64_t clamped = std::min<int64_t>(doubledPlusOne, INT64_C(2147483647));
    // long -> int narrowing (JLS 5.1.3): keep the low 32 bits, two's-complement. Going
    // through uint32_t makes the wrap defined and portable instead of relying on
    // implementation-defined / UB signed narrowing under -O2.
    return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(clamped)));
}

}  // namespace mc::world::inventory

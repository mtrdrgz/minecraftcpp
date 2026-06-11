#pragma once
// 1:1 C++ port of the PURE deterministic arithmetic of
// net.minecraft.world.item.trading.MerchantOffer (Minecraft Java 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/world/item/trading/MerchantOffer.java
//
// Ported (pure, world-free) members:
//   * getModifiedCostCount(cost)            — MerchantOffer.java:123-127
//   * updateDemand()                        — MerchantOffer.java:145-147
//   * isOutOfStock()                        — MerchantOffer.java:197-199
//   * needsRestock()                        — MerchantOffer.java:205-207
//
// NOT ported here (out of scope — ItemStack/registry/codec/stream coupled):
//   getCostA/getCostB/getResult/assemble/satisfiedBy/take/copy and the
//   Codec/StreamCodec read+write paths. getModifiedCostCount's only world
//   dependency is `cost.itemStack().getMaxStackSize()`, which we surface here as
//   the explicit `maxStackSize` parameter (the caller in vanilla resolves it from
//   the ItemStack's MAX_STACK_SIZE data component); every other operand is a
//   plain int/float field on the offer.
//
// 1:1 TRAPS preserved:
//   * getModifiedCostCount: `basePrice * this.demand` is int*int (Java int
//     multiply WRAPS mod 2^32 on overflow), then `(that int) * this.priceMultiplier`
//     widens to float; Mth.floor of that float = (int)Math.floor((double)f), which
//     SATURATES per JLS 5.1.3 (mc::mthFloor handles NaN->0 and the int range).
//     Math.max(0, .) then Mth.clamp(int) to [1, maxStackSize].
//   * updateDemand: `demand + uses - (maxUses - uses)` — all int, wraps on overflow.

#include <cstdint>

#include "world/phys/shapes/JavaMath.h"  // mc::mthFloor, mc::mthClamp (JLS-exact)

namespace mc::world::item::trading {

// MerchantOffer.getModifiedCostCount(ItemCost cost) — MerchantOffer.java:123-127.
//
//   int basePrice = cost.count();
//   int demandDiff = Math.max(0, Mth.floor(basePrice * this.demand * this.priceMultiplier));
//   return Mth.clamp(basePrice + demandDiff + this.specialPriceDiff, 1, cost.itemStack().getMaxStackSize());
//
// All int arithmetic uses two's-complement wraparound (signed overflow is UB
// under -O2, so we compute the wrapping products/sums through uint32_t).
inline int32_t getModifiedCostCount(int32_t basePrice, int32_t demand, float priceMultiplier,
                                    int32_t specialPriceDiff, int32_t maxStackSize) {
    // basePrice * this.demand  (int * int, wrapping)
    const int32_t intProduct = static_cast<int32_t>(static_cast<uint32_t>(basePrice) * static_cast<uint32_t>(demand));
    // (int) * priceMultiplier  -> float  (int promoted to float, JLS 5.6.2)
    const float scaled = static_cast<float>(intProduct) * priceMultiplier;
    // Mth.floor(float) = (int)Math.floor((double)f)  — saturating narrow.
    const int32_t floored = mc::mthFloor(static_cast<double>(scaled));
    // Math.max(0, .)
    const int32_t demandDiff = floored > 0 ? floored : 0;
    // basePrice + demandDiff + specialPriceDiff  (int adds, wrapping)
    const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(basePrice)
                                             + static_cast<uint32_t>(demandDiff)
                                             + static_cast<uint32_t>(specialPriceDiff));
    // Mth.clamp(sum, 1, maxStackSize)
    return mc::mthClamp(sum, 1, maxStackSize);
}

// MerchantOffer.updateDemand() — MerchantOffer.java:145-147.
//   this.demand = this.demand + this.uses - (this.maxUses - this.uses);
// Returns the NEW demand value (the field after mutation).
inline int32_t updateDemand(int32_t demand, int32_t uses, int32_t maxUses) {
    const uint32_t inner = static_cast<uint32_t>(maxUses) - static_cast<uint32_t>(uses);  // (maxUses - uses)
    const uint32_t result = static_cast<uint32_t>(demand) + static_cast<uint32_t>(uses) - inner;
    return static_cast<int32_t>(result);
}

// MerchantOffer.isOutOfStock() — MerchantOffer.java:197-199.
//   return this.uses >= this.maxUses;
inline bool isOutOfStock(int32_t uses, int32_t maxUses) { return uses >= maxUses; }

// MerchantOffer.needsRestock() — MerchantOffer.java:205-207.
//   return this.uses > 0;
inline bool needsRestock(int32_t uses) { return uses > 0; }

}  // namespace mc::world::item::trading

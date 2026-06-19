#pragma once

// 1:1 port of the durability-bar display math from net.minecraft.world.item.Item
// (Item.java:218-230) together with the damage clamp that the real values flow
// through in net.minecraft.world.item.ItemStack (ItemStack.java:422-426).
//
//   ItemStack.getDamageValue() = Mth.clamp(stored DAMAGE, 0, getMaxDamage())
//   ItemStack.isDamaged()      = isDamageableItem() && getDamageValue() > 0
//
//   Item.isBarVisible(stack) = stack.isDamaged()
//   Item.getBarWidth(stack)  = Mth.clamp(Math.round(13.0F - getDamageValue()*13.0F/getMaxDamage()), 0, 13)
//   Item.getBarColor(stack)  = Mth.hsvToRgb(Math.max(0.0F, ((float)maxDamage - getDamageValue())/maxDamage)/3.0F, 1.0F, 1.0F)
//
// These are pure functions of (raw stored damage, maxDamage, damageable). They
// are the load-bearing arithmetic the hotbar/inventory renderer reads. Several
// 1:1 traps live here:
//   * the whole getBarWidth expression is FLOAT arithmetic in a fixed order
//     (dmg*13.0F is a float multiply, /maxDamage promotes maxDamage to float).
//   * Math.round(float) == (int)floor(a + 0.5f)  (ties round toward +inf), with
//     the +0.5f performed in float.
//   * getBarColor divides (float)maxDamage-dmg by maxDamage promoted to float,
//     then Math.max against 0.0F, then /3.0F, all float, before the real
//     Mth.hsvToRgb table conversion.

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../level/levelgen/Mth.h"

namespace mc::item {

// net.minecraft.world.item.ItemStack.getDamageValue() — ItemStack.java:425-427.
// Mth.clamp(getOrDefault(DAMAGE, 0), 0, getMaxDamage()).
inline int32_t getDamageValue(int32_t storedDamage, int32_t maxDamage) {
    return mc::levelgen::mth::clamp(storedDamage, 0, maxDamage);
}

// Java Math.round(float): "Returns the closest int, ties rounding toward
// positive infinity" == (int)Math.floor(a + 0.5f). The +0.5f is a float add.
inline int32_t mathRoundFloat(float a) {
    return static_cast<int32_t>(std::floor(a + 0.5f));
}

// net.minecraft.world.item.Item.isBarVisible(stack) — Item.java:218-220.
// Returns stack.isDamaged() == isDamageableItem() && getDamageValue() > 0.
inline bool isBarVisible(int32_t storedDamage, int32_t maxDamage, bool damageable) {
    return damageable && getDamageValue(storedDamage, maxDamage) > 0;
}

// net.minecraft.world.item.Item.getBarWidth(stack) — Item.java:222-224.
inline int32_t getBarWidth(int32_t storedDamage, int32_t maxDamage) {
    const int32_t dmg = getDamageValue(storedDamage, maxDamage);
    const float width = 13.0f - static_cast<float>(dmg) * 13.0f / static_cast<float>(maxDamage);
    return mc::levelgen::mth::clamp(mathRoundFloat(width), 0, 13);
}

// net.minecraft.world.item.Item.getBarColor(stack) — Item.java:226-230.
inline int32_t getBarColor(int32_t storedDamage, int32_t maxDamage) {
    const int32_t dmg = getDamageValue(storedDamage, maxDamage);
    const float healthPercentage =
        std::max(0.0f, (static_cast<float>(maxDamage) - static_cast<float>(dmg)) / static_cast<float>(maxDamage));
    return mc::levelgen::mth::hsvToRgb(healthPercentage / 3.0f, 1.0f, 1.0f);
}

} // namespace mc::item

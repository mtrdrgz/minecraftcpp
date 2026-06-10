#pragma once

// 1:1 port of net.minecraft.world.damagesource.CombatRules (26.1.2).
//
// Pure-static armor/magic damage formulas. Every constant and operation below is
// VERBATIM from CombatRules.java — float arithmetic only, with Mth.clamp (the
// `value < min ? min : Math.min(value, max)` variant), reused from the certified
// engine Mth.h (mc::levelgen::mth::clamp). Do NOT substitute std::clamp here:
// that uses different ordering and would diverge for signed-zero/NaN edges.
//
// Java source (CombatRules.java:9-37):
//   public static final float MAX_ARMOR              = 20.0F;
//   public static final float ARMOR_PROTECTION_DIVIDER = 25.0F;
//   public static final float BASE_ARMOR_TOUGHNESS    = 2.0F;
//   public static final float MIN_ARMOR_RATIO         = 0.2F;
//   private static final int  NUM_ARMOR_ITEMS         = 4;
//
//   getDamageAfterAbsorb(victim, damage, source, totalArmor, armorToughness):
//     float toughness    = 2.0F + armorToughness / 4.0F;
//     float realArmor     = Mth.clamp(totalArmor - damage / toughness, totalArmor * 0.2F, 20.0F);
//     float armorFraction = realArmor / 25.0F;
//     ItemStack weaponItem = source.getWeaponItem();
//     float modifiedArmorFraction;
//     if (weaponItem != null && victim.level() instanceof ServerLevel level) {
//        modifiedArmorFraction = Mth.clamp(
//            EnchantmentHelper.modifyArmorEffectiveness(level, weaponItem, victim, source, armorFraction), 0.0F, 1.0F);
//     } else {
//        modifiedArmorFraction = armorFraction;
//     }
//     float damageMultiplier = 1.0F - modifiedArmorFraction;
//     return damage * damageMultiplier;
//
//   getDamageAfterMagicAbsorb(damage, totalMagicArmor):
//     float realArmor = Mth.clamp(totalMagicArmor, 0.0F, 20.0F);
//     return damage * (1.0F - realArmor / 25.0F);
//
// NOTE on getDamageAfterAbsorb: the enchantment branch
// (EnchantmentHelper.modifyArmorEffectiveness) is an UN-PORTED dependency and is
// reached only when source.getWeaponItem() != null AND victim.level() is a
// ServerLevel. For a DamageSource with no direct entity, getWeaponItem() returns
// null (DamageSource.java:67-68), so that branch is NOT taken and `victim` is
// never dereferenced — the pure float formula below is fully faithful for that
// (default) case. getDamageAfterAbsorbNoEnchant() ports exactly that else-branch.

#include "world/level/levelgen/Mth.h"

namespace mc::damagesource {

// CombatRules.java:10-13 — public float constants.
inline constexpr float MAX_ARMOR               = 20.0F;
inline constexpr float ARMOR_PROTECTION_DIVIDER = 25.0F;
inline constexpr float BASE_ARMOR_TOUGHNESS    = 2.0F;
inline constexpr float MIN_ARMOR_RATIO         = 0.2F;
inline constexpr int   NUM_ARMOR_ITEMS         = 4; // private in Java

// CombatRules.getDamageAfterAbsorb, else-branch (weaponItem == null, i.e. a
// DamageSource with no direct entity). This is the pure-float path with no
// dereference of `victim` or the enchantment helper. Identical bit-for-bit to
// the full method whenever source.getWeaponItem() == null.
inline float getDamageAfterAbsorbNoEnchant(float damage, float totalArmor, float armorToughness) {
    float toughness    = 2.0F + armorToughness / 4.0F;                                          // :19
    float realArmor    = mc::levelgen::mth::clamp(totalArmor - damage / toughness, totalArmor * 0.2F, 20.0F); // :20
    float armorFraction = realArmor / 25.0F;                                                     // :21
    // weaponItem == null  =>  modifiedArmorFraction = armorFraction                              // :22-28 (else)
    float modifiedArmorFraction = armorFraction;
    float damageMultiplier = 1.0F - modifiedArmorFraction;                                        // :30
    return damage * damageMultiplier;                                                             // :31
}

// CombatRules.getDamageAfterMagicAbsorb — CombatRules.java:34-37. Pure float.
inline float getDamageAfterMagicAbsorb(float damage, float totalMagicArmor) {
    float realArmor = mc::levelgen::mth::clamp(totalMagicArmor, 0.0F, 20.0F); // :35
    return damage * (1.0F - realArmor / 25.0F);                              // :36
}

} // namespace mc::damagesource

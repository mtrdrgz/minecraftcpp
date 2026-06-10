#pragma once

// 1:1 port of the DATA constants of net.minecraft.world.item.ToolMaterial (26.1.2).
//
// The Java class is a record:
//   public record ToolMaterial(
//      TagKey<Block> incorrectBlocksForDrops, int durability, float speed,
//      float attackDamageBonus, int enchantmentValue, TagKey<Item> repairItems)
//
// Only the four scalar fields (durability, speed, attackDamageBonus,
// enchantmentValue) are pure data and ported here, VERBATIM from the seven
// static instances declared at ToolMaterial.java:23-31. The two TagKey<Block>/
// TagKey<Item> fields (incorrectBlocksForDrops, repairItems) and every method
// (applyCommonProperties/applyToolProperties/createToolAttributes/
// applySwordProperties/createSwordAttributes) are registry/component-coupled
// (TagKey, BuiltInRegistries, DataComponents, ItemAttributeModifiers, Tool,
// Weapon, AttributeModifier) and are intentionally NOT ported — see the gate's
// unportedMethods. This header exposes exactly the certified scalar table.
//
// Source rows (ToolMaterial.java):
//   :23 WOOD      = (INCORRECT_FOR_WOODEN_TOOL,    59,  2.0F, 0.0F, 15, ...)
//   :24 STONE     = (INCORRECT_FOR_STONE_TOOL,    131,  4.0F, 1.0F,  5, ...)
//   :25 COPPER    = (INCORRECT_FOR_COPPER_TOOL,   190,  5.0F, 1.0F, 13, ...)
//   :26 IRON      = (INCORRECT_FOR_IRON_TOOL,     250,  6.0F, 2.0F, 14, ...)
//   :27 DIAMOND   = (INCORRECT_FOR_DIAMOND_TOOL, 1561,  8.0F, 3.0F, 10, ...)
//   :28 GOLD      = (INCORRECT_FOR_GOLD_TOOL,      32, 12.0F, 0.0F, 22, ...)
//   :29 NETHERITE = (INCORRECT_FOR_NETHERITE_TOOL,2031, 9.0F, 4.0F, 15, ...)

#include <cstdint>

namespace mc::item {

// The four pure-data fields of a ToolMaterial record instance.
struct ToolMaterial {
    int32_t durability;        // ToolMaterial.durability
    float   speed;             // ToolMaterial.speed
    float   attackDamageBonus; // ToolMaterial.attackDamageBonus
    int32_t enchantmentValue;  // ToolMaterial.enchantmentValue
};

// The seven vanilla static instances, VERBATIM from ToolMaterial.java:23-31.
inline constexpr ToolMaterial WOOD      { 59,   2.0F, 0.0F, 15 }; // :23
inline constexpr ToolMaterial STONE     { 131,  4.0F, 1.0F, 5  }; // :24
inline constexpr ToolMaterial COPPER    { 190,  5.0F, 1.0F, 13 }; // :25
inline constexpr ToolMaterial IRON      { 250,  6.0F, 2.0F, 14 }; // :26
inline constexpr ToolMaterial DIAMOND   { 1561, 8.0F, 3.0F, 10 }; // :27
inline constexpr ToolMaterial GOLD      { 32,   12.0F, 0.0F, 22 }; // :28
inline constexpr ToolMaterial NETHERITE { 2031, 9.0F, 4.0F, 15 }; // :29

} // namespace mc::item

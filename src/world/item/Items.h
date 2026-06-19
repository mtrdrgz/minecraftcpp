#pragma once
#include "Item.h"
#include "../../core/Registry.h"
#include <memory>
#include <vector>

namespace mc {

// Global item registry — holds all Item instances. Mirrors Blocks.h /
// BuiltInRegistries.ITEM.
extern Registry<Item>                     g_itemRegistry;
extern std::vector<std::unique_ptr<Item>> g_itemStorage;

// Populates the items:: pointers and registers them into g_itemRegistry.
// Must be called once at startup, after initBlocks() (block items reference
// blocks, even though we don't store the link yet).
void initItems();

// Convenience pointers to the small set of items the hotbar/inventory renderer
// needs right now. Direct equivalent of the `public static final Item …`
// fields in net.minecraft.world.item.Items.
namespace items {
    extern Item* AIR;
    extern Item* STONE;
    extern Item* DIRT;
    extern Item* GRASS_BLOCK;
    extern Item* COBBLESTONE;
    extern Item* OAK_PLANKS;
    extern Item* OAK_LOG;
    extern Item* WOODEN_PICKAXE;
    extern Item* STONE_PICKAXE;
    extern Item* IRON_PICKAXE;
    extern Item* WOODEN_SWORD;
    extern Item* IRON_SWORD;
    extern Item* DIAMOND_SWORD;
    extern Item* BREAD;
    extern Item* APPLE;
    extern Item* COOKED_BEEF;
    extern Item* COAL;
    extern Item* IRON_INGOT;
    extern Item* DIAMOND;
    extern Item* STICK;
    extern Item* TORCH;
    extern Item* CRAFTING_TABLE;
} // namespace items

} // namespace mc

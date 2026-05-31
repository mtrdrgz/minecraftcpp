#include "Items.h"
#include "../../core/Log.h"
#include <string>
#include <string_view>

namespace mc {

Registry<Item>                     g_itemRegistry;
std::vector<std::unique_ptr<Item>> g_itemStorage;

namespace items {
    Item* AIR            = nullptr;
    Item* STONE          = nullptr;
    Item* DIRT           = nullptr;
    Item* GRASS_BLOCK    = nullptr;
    Item* COBBLESTONE    = nullptr;
    Item* OAK_PLANKS     = nullptr;
    Item* OAK_LOG        = nullptr;
    Item* WOODEN_PICKAXE = nullptr;
    Item* STONE_PICKAXE  = nullptr;
    Item* IRON_PICKAXE   = nullptr;
    Item* WOODEN_SWORD   = nullptr;
    Item* IRON_SWORD     = nullptr;
    Item* DIAMOND_SWORD  = nullptr;
    Item* BREAD          = nullptr;
    Item* APPLE          = nullptr;
    Item* COOKED_BEEF    = nullptr;
    Item* COAL           = nullptr;
    Item* IRON_INGOT     = nullptr;
    Item* DIAMOND        = nullptr;
    Item* STICK          = nullptr;
    Item* TORCH          = nullptr;
    Item* CRAFTING_TABLE = nullptr;
} // namespace items

// ── Helpers ───────────────────────────────────────────────────────────────────
//
// Java's Items uses two helpers in different forms:
//   Items.registerBlock(Block) — for BlockItem variants (stack=64, no damage)
//   Items.registerItem(name, props) — for plain items / tools / food
// Both end up calling Registry.register(ResourceKey<Item>, Item) on
// BuiltInRegistries.ITEM. We collapse them into one helper here since we don't
// have a BlockItem subclass yet.
static Item* registerItem(std::string_view path, Item::Properties props) {
    props.registryName = std::string("minecraft:") + std::string(path);
    auto up = std::make_unique<Item>(props);
    Item* ptr = up.get();
    g_itemStorage.push_back(std::move(up));
    g_itemRegistry.register_(ResourceLocation::parse(props.registryName), ptr);
    return ptr;
}

// ── ToolMaterial durability values (from ToolMaterial.java) ───────────────────
// Kept inline so we don't yet need a ToolMaterial port.
namespace {
    constexpr int32_t TOOL_DURABILITY_WOOD    = 59;
    constexpr int32_t TOOL_DURABILITY_STONE   = 131;
    constexpr int32_t TOOL_DURABILITY_IRON    = 250;
    constexpr int32_t TOOL_DURABILITY_DIAMOND = 1561;
}

// ── Property factories matching Java Item.Properties().{durability,stacksTo} ──
static Item::Properties propsBlock() {
    Item::Properties p;
    p.maxStackSize = 64;
    p.maxDamage    = 0;
    return p;
}

static Item::Properties propsGeneric() {
    return propsBlock(); // identical to block items for non-tool items
}

static Item::Properties propsTool(int32_t durability) {
    Item::Properties p;
    p.maxStackSize = 1;          // Java: implicit when durability() is set
    p.maxDamage    = durability;
    return p;
}

// ── Registration entry point ─────────────────────────────────────────────────
//
// Matches the static-initializer order in net.minecraft.world.item.Items.java.
// We register ~20 items here; the full set (~1,200) will be added as more of
// the inventory system comes online.
void initItems() {
    using namespace items;

    // Java: public static final Item AIR = registerBlock(Blocks.AIR, AirItem::new);
    // AirItem behaves as the canonical "empty" item — count is normally 0 and
    // ItemStack.isEmpty() returns true when item == AIR. We keep it in the
    // registry at id 0 so byId(0) returns it, but ItemStack should still use
    // nullptr or AIR + count=0 to represent empty. See ItemStack::isEmpty().
    AIR            = registerItem("air",             propsBlock());

    // Blocks → BlockItem (stack=64)
    STONE          = registerItem("stone",           propsBlock());
    GRASS_BLOCK    = registerItem("grass_block",     propsBlock());
    DIRT           = registerItem("dirt",            propsBlock());
    COBBLESTONE    = registerItem("cobblestone",     propsBlock());
    OAK_PLANKS     = registerItem("oak_planks",      propsBlock());
    OAK_LOG        = registerItem("oak_log",         propsBlock());

    // Tools — stack=1, durability from ToolMaterial
    WOODEN_PICKAXE = registerItem("wooden_pickaxe",  propsTool(TOOL_DURABILITY_WOOD));
    STONE_PICKAXE  = registerItem("stone_pickaxe",   propsTool(TOOL_DURABILITY_STONE));
    IRON_PICKAXE   = registerItem("iron_pickaxe",    propsTool(TOOL_DURABILITY_IRON));
    WOODEN_SWORD   = registerItem("wooden_sword",    propsTool(TOOL_DURABILITY_WOOD));
    IRON_SWORD     = registerItem("iron_sword",      propsTool(TOOL_DURABILITY_IRON));
    DIAMOND_SWORD  = registerItem("diamond_sword",   propsTool(TOOL_DURABILITY_DIAMOND));

    // Foods — stack=64 (Food properties not yet ported)
    BREAD          = registerItem("bread",           propsGeneric());
    APPLE          = registerItem("apple",           propsGeneric());
    COOKED_BEEF    = registerItem("cooked_beef",     propsGeneric());

    // Materials — stack=64
    COAL           = registerItem("coal",            propsGeneric());
    IRON_INGOT     = registerItem("iron_ingot",      propsGeneric());
    DIAMOND        = registerItem("diamond",         propsGeneric());
    STICK          = registerItem("stick",           propsGeneric());

    // Misc blocks
    TORCH          = registerItem("torch",           propsBlock());
    CRAFTING_TABLE = registerItem("crafting_table",  propsBlock());

    MC_LOG_INFO("Items: {} items registered", g_itemRegistry.size());
}

} // namespace mc

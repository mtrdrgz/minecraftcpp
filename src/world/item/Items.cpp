#include "Items.h"
#include "../../core/Log.h"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#include "../../assets/resource_ids.h"
#endif

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

// ── Data-driven loader: the full vanilla item registry ───────────────────────
//
// Loads assets/items.json — generated 1:1 from BuiltInRegistries.ITEM and gated
// byte-for-byte by item_registry_parity (1506/1506/0). Items are registered in
// vanilla id order so g_itemRegistry ids match the real registry (saves/network
// item ids depend on this). The named convenience pointers are rebound by lookup.
// Falls back to the hand-registered set if the table is unavailable (e.g. a test
// binary that doesn't embed the resource and has no MCPP_ITEMS/disk copy).
static void initItemsFallback();

void initItems() {
    using namespace items;

    std::string fileData;
    const char* raw = nullptr;
    std::size_t sz = 0;
#if defined(_WIN32)
    HMODULE hmod = GetModuleHandleW(nullptr);
    HRSRC hres = FindResourceW(hmod, MAKEINTRESOURCEW(IDR_ITEMS), MAKEINTRESOURCEW(10));
    if (hres) {
        HGLOBAL hg = LoadResource(hmod, hres);
        raw = static_cast<const char*>(LockResource(hg));
        sz = SizeofResource(hmod, hres);
    }
#endif
    if (!raw) {
        const char* env = std::getenv("MCPP_ITEMS");
        for (const char* p : { env, "mcpp/src/assets/items.json", "src/assets/items.json" }) {
            if (!p) continue;
            std::ifstream in(p, std::ios::binary);
            if (in) { std::stringstream ss; ss << in.rdbuf(); fileData = ss.str(); raw = fileData.data(); sz = fileData.size(); break; }
        }
    }
    if (!raw) {
        MC_LOG_WARN("Items: items.json not available — using fallback registry");
        initItemsFallback();
        return;
    }

    try {
        auto j = nlohmann::json::parse(raw, raw + sz);
        for (auto& it : j.at("items")) {
            Item::Properties props;
            props.maxStackSize = it.value("max_stack", 64);
            props.maxDamage    = it.value("max_damage", 0);
            props.registryName = "minecraft:" + it.at("name").get<std::string>();
            auto up = std::make_unique<Item>(props);
            Item* ptr = up.get();
            g_itemStorage.push_back(std::move(up));
            g_itemRegistry.register_(ResourceLocation::parse(props.registryName), ptr);
        }
    } catch (const std::exception& e) {
        MC_LOG_ERROR("Items: items.json parse error: {} — falling back", e.what());
        g_itemStorage.clear();
        g_itemRegistry = Registry<Item>{};
        initItemsFallback();
        return;
    }

    // Rebind the named convenience pointers from the full registry.
    auto get = [](const char* n) { return g_itemRegistry.getByName(ResourceLocation::parse(std::string("minecraft:") + n)); };
    AIR = get("air"); STONE = get("stone"); DIRT = get("dirt"); GRASS_BLOCK = get("grass_block");
    COBBLESTONE = get("cobblestone"); OAK_PLANKS = get("oak_planks"); OAK_LOG = get("oak_log");
    WOODEN_PICKAXE = get("wooden_pickaxe"); STONE_PICKAXE = get("stone_pickaxe"); IRON_PICKAXE = get("iron_pickaxe");
    WOODEN_SWORD = get("wooden_sword"); IRON_SWORD = get("iron_sword"); DIAMOND_SWORD = get("diamond_sword");
    BREAD = get("bread"); APPLE = get("apple"); COOKED_BEEF = get("cooked_beef");
    COAL = get("coal"); IRON_INGOT = get("iron_ingot"); DIAMOND = get("diamond"); STICK = get("stick");
    TORCH = get("torch"); CRAFTING_TABLE = get("crafting_table");
    MC_LOG_INFO("Items: {} items registered (full vanilla table)", g_itemRegistry.size());
}

// ── Fallback: the hand-registered subset (used only when items.json is absent) ─
//
// Matches the static-initializer order in net.minecraft.world.item.Items.java.
static void initItemsFallback() {
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

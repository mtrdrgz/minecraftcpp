#include "Blocks.h"
#include "../../../core/Log.h"
#include "../../../assets/AssetManager.h"
#include "BlockStates.h"
#include <nlohmann/json.hpp>
#if defined(_WIN32)
#include "../../../assets/resource_ids.h"
#include <windows.h>
#endif
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <set>
#include <string>

namespace mc {

Registry<Block>                     g_blockRegistry;
std::vector<std::unique_ptr<Block>> g_blockStorage;
std::vector<BlockState>             g_blockStates;
std::unordered_map<std::string, Block*> g_blocksByName;
std::unordered_map<std::string, uint32_t> g_defaultStateByName;

namespace blocks {
    Block* AIR         = nullptr;
    Block* STONE       = nullptr;
    Block* GRASS_BLOCK = nullptr;
    Block* DIRT        = nullptr;
    Block* WATER       = nullptr;
    Block* LAVA        = nullptr;
    Block* BEDROCK     = nullptr;
    Block* DEEPSLATE   = nullptr;
    Block* SAND        = nullptr;
    Block* GRAVEL      = nullptr;
    Block* NETHERRACK  = nullptr;
    Block* END_STONE   = nullptr;
    Block* OAK_LOG     = nullptr;
    Block* OAK_LEAVES  = nullptr;
    Block* GLASS       = nullptr;
}

Block* getBlockByName(std::string_view name) {
    auto it = g_blocksByName.find(std::string(name));
    return it == g_blocksByName.end() ? nullptr : it->second;
}

uint32_t getDefaultBlockStateId(std::string_view name, uint32_t fallback) {
    auto it = g_defaultStateByName.find(std::string(name));
    return it == g_defaultStateByName.end() ? fallback : it->second;
}

namespace {

std::string stripMinecraftNamespace(std::string name) {
    if (name.starts_with("minecraft:")) {
        name.erase(0, 10);
    }
    return name;
}

bool isPillarBlockName(const std::string& name) {
    return name.ends_with("_log") || name.ends_with("_wood") ||
           name.ends_with("_stem") || name.ends_with("_hyphae") ||
           name == "bamboo_block";
}

uint32_t checkedStateOffset(const std::string& name, uint32_t base, int offset) {
    const uint32_t id = base + static_cast<uint32_t>(offset);
    if (id >= g_blockStates.size()) {
        return base;
    }
    const BlockState* state = getBlockState(id);
    if (state && state->block && state->block->name == name) {
        return id;
    }
    return base;
}

static void setFallbackTexture(Block* block, const std::string& name) {
    if (!block) return;
    std::string tex = name;
    if (tex.starts_with("minecraft:")) tex.erase(0, 10);

    if (tex == "tall_grass") tex = "tall_grass_top";
    else if (tex == "large_fern") tex = "large_fern_top";
    else if (tex == "tall_seagrass") tex = "tall_seagrass_top";
    else if (tex == "sweet_berry_bush") tex = "sweet_berry_bush_stage3";
    else if (tex == "kelp_plant") tex = "kelp";
    else if (tex == "cave_vines_plant") tex = "cave_vines";
    else if (tex == "twisting_vines_plant") tex = "twisting_vines";
    else if (tex == "weeping_vines_plant") tex = "weeping_vines";
    else if (tex == "pitcher_plant") tex = "pitcher_plant_top";
    else if (tex == "big_dripleaf") tex = "big_dripleaf_top";
    else if (tex == "small_dripleaf") tex = "small_dripleaf_top";
    else if (tex == "bamboo") tex = "bamboo_stalk";
    else if (tex == "pumpkin") tex = "pumpkin_side";
    else if (tex == "melon") tex = "melon_side";
    else if (tex == "cactus") tex = "cactus_side";

    block->textures.all = tex;
    if (block->name == "cactus") {
        block->textures.top = "cactus_top";
        block->textures.bot = "cactus_bottom";
        block->textures.side = "cactus_side";
    } else if (block->name == "pumpkin") {
        block->textures.top = block->textures.bot = "pumpkin_top";
        block->textures.side = "pumpkin_side";
    } else if (block->name == "melon") {
        block->textures.top = block->textures.bot = "melon_top";
        block->textures.side = "melon_side";
    }
}

std::string normalizeModelPath(std::string value) {
    if (value.starts_with("minecraft:")) value.erase(0, 10);
    return "minecraft/models/" + value + ".json";
}

std::string normalizeTextureName(std::string value) {
    if (value.empty()) return value;
    if (value.starts_with("minecraft:")) value.erase(0, 10);
    if (value.starts_with("block/")) value.erase(0, 6);
    if (value.starts_with("blocks/")) value.erase(0, 7);
    if (value.starts_with("textures/block/")) value.erase(0, 15);
    if (value.ends_with(".png")) value.resize(value.size() - 4);
    return value;
}

std::vector<uint8_t> readAssetOrLocal(const std::string& assetPath) {
    if (auto bytes = AssetManager::instance().readRaw(assetPath); !bytes.empty()) {
        return bytes;
    }
    std::ifstream in("assets/client-extract/assets/" + assetPath, std::ios::binary);
    if (!in) return {};
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

bool parseAssetJson(const std::string& assetPath, nlohmann::json& out) {
    const std::vector<uint8_t> bytes = readAssetOrLocal(assetPath);
    if (bytes.empty()) return false;
    try {
        out = nlohmann::json::parse(bytes.begin(), bytes.end());
        return true;
    } catch (...) {
        return false;
    }
}

std::string firstModelFromBlockstate(const std::string& blockName) {
    nlohmann::json j;
    if (!parseAssetJson("minecraft/blockstates/" + blockName + ".json", j)) {
        return {};
    }

    auto readApply = [](const nlohmann::json& node) -> std::string {
        if (node.is_array() && !node.empty()) {
            const auto& first = node.front();
            return first.value("model", "");
        }
        if (node.is_object()) return node.value("model", "");
        return {};
    };

    if (auto it = j.find("variants"); it != j.end() && it->is_object() && !it->empty()) {
        for (const auto& item : it->items()) {
            if (std::string model = readApply(item.value()); !model.empty()) return model;
        }
    }
    if (auto it = j.find("multipart"); it != j.end() && it->is_array()) {
        for (const auto& part : *it) {
            if (auto app = part.find("apply"); app != part.end()) {
                if (std::string model = readApply(*app); !model.empty()) return model;
            }
        }
    }
    return {};
}

void collectModelTextures(const std::string& modelName,
                          std::unordered_map<std::string, std::string>& textures,
                          std::set<std::string>& seen) {
    if (modelName.empty() || !seen.insert(modelName).second) return;

    nlohmann::json j;
    if (!parseAssetJson(normalizeModelPath(modelName), j)) {
        return;
    }
    if (std::string parent = j.value("parent", ""); !parent.empty()) {
        collectModelTextures(parent, textures, seen);
    }
    if (auto it = j.find("textures"); it != j.end() && it->is_object()) {
        for (const auto& item : it->items()) {
            if (item.value().is_string()) {
                textures[item.key()] = item.value().get<std::string>();
            }
        }
    }
}

std::string resolveTextureRef(const std::unordered_map<std::string, std::string>& textures,
                              std::string key) {
    std::set<std::string> seen;
    while (!key.empty()) {
        if (key[0] != '#') return normalizeTextureName(key);
        key.erase(0, 1);
        if (!seen.insert(key).second) return {};
        auto it = textures.find(key);
        if (it == textures.end()) return {};
        key = it->second;
    }
    return {};
}

std::string firstTextureKey(const std::unordered_map<std::string, std::string>& textures,
                            std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        auto it = textures.find(key);
        if (it == textures.end()) continue;
        if (std::string tex = resolveTextureRef(textures, it->second); !tex.empty()) {
            return tex;
        }
    }
    return {};
}

static void assignVanillaModelTextures() {
    int assigned = 0;
    for (auto& blockPtr : g_blockStorage) {
        if (!blockPtr) continue;
        const std::string model = firstModelFromBlockstate(blockPtr->name);
        if (model.empty()) continue;

        std::unordered_map<std::string, std::string> textures;
        std::set<std::string> seen;
        collectModelTextures(model, textures, seen);
        if (textures.empty()) continue;

        const std::string all = firstTextureKey(textures, {
            "all", "texture", "cross", "plant", "layer0", "particle"
        });
        const std::string top = firstTextureKey(textures, {
            "top", "up", "end", "all", "texture", "cross", "plant", "particle"
        });
        const std::string bot = firstTextureKey(textures, {
            "bottom", "down", "end", "all", "texture", "cross", "plant", "particle"
        });
        const std::string side = firstTextureKey(textures, {
            "side", "north", "south", "east", "west", "all", "texture", "cross", "plant", "particle"
        });

        if (all.empty() && top.empty() && bot.empty() && side.empty()) continue;
        blockPtr->textures.all = !all.empty() ? all : (!side.empty() ? side : (!top.empty() ? top : bot));
        blockPtr->textures.top = top;
        blockPtr->textures.bot = bot;
        blockPtr->textures.side = side;
        ++assigned;
    }
    MC_LOG_INFO("Blocks: assigned vanilla model texture hints for {} blocks", assigned);
}

static void assignKnownBlockTextures() {
    if (blocks::STONE)       blocks::STONE->textures.all = "stone";
    if (blocks::GRASS_BLOCK) {
        blocks::GRASS_BLOCK->textures.top  = "grass_block_top";
        blocks::GRASS_BLOCK->textures.side = "grass_block_side";
        blocks::GRASS_BLOCK->textures.bot  = blocks::GRASS_BLOCK->textures.all = "dirt";
    }
    if (blocks::DIRT)       blocks::DIRT->textures.all = "dirt";
    if (blocks::WATER)      blocks::WATER->textures.all = "water_still";
    if (blocks::LAVA)       blocks::LAVA->textures.all = "lava_still";
    if (blocks::BEDROCK)    blocks::BEDROCK->textures.all = "bedrock";
    if (blocks::DEEPSLATE)  blocks::DEEPSLATE->textures.all = "deepslate";
    if (blocks::SAND)       blocks::SAND->textures.all = "sand";
    if (blocks::GRAVEL)     blocks::GRAVEL->textures.all = "gravel";
    if (blocks::NETHERRACK) blocks::NETHERRACK->textures.all = "netherrack";
    if (blocks::END_STONE)  blocks::END_STONE->textures.all = "end_stone";
    if (blocks::OAK_LOG) {
        blocks::OAK_LOG->textures.all = "oak_log";
        blocks::OAK_LOG->textures.top = blocks::OAK_LOG->textures.bot = "oak_log_top";
    }
    if (blocks::OAK_LEAVES) blocks::OAK_LEAVES->textures.all = "oak_leaves";
    if (blocks::GLASS)      blocks::GLASS->textures.all = "glass";

    assignVanillaModelTextures();

    for (auto& blockPtr : g_blockStorage) {
        if (!blockPtr || !blockPtr->textures.all.empty()) continue;
        setFallbackTexture(blockPtr.get(), blockPtr->name);
    }
}

} // namespace

uint32_t getBlockStateId(std::string_view serializedState, uint32_t fallback) {
    const std::string state(serializedState);
    const std::string name = stripMinecraftNamespace(block::blockName(state));
    const uint32_t base = getDefaultBlockStateId(name, fallback);
    const auto props = block::properties(state);
    if (props.empty()) {
        return base;
    }

    if (auto axis = props.find("axis"); axis != props.end() && isPillarBlockName(name)) {
        if (axis->second == "x") return checkedStateOffset(name, base, 0);
        if (axis->second == "y") return checkedStateOffset(name, base, 1);
        if (axis->second == "z") return checkedStateOffset(name, base, 2);
    }

    if (auto half = props.find("half"); half != props.end()) {
        if (half->second == "lower") return checkedStateOffset(name, base, 0);
        if (half->second == "upper") return checkedStateOffset(name, base, 1);
    }

    return base;
}

const BlockState* getDefaultBlockState(std::string_view name) {
    uint32_t id = getDefaultBlockStateId(name, 0);
    return getBlockState(id);
}

uint32_t getBlockStateIdWith(std::string_view name,
                             const std::initializer_list<std::pair<std::string_view, std::string_view>>& overrides,
                             uint32_t fallback) {
    // Match Java's BlockState.setValue(key, value) semantics: start from the
    // block's default state and override only the specified properties. So we
    // first find the default state for `name`, then look for a state whose
    // properties equal the default's properties with the overrides applied.
    // This matters for blocks like spruce_stairs where the default is
    // (facing=north,half=bottom,shape=straight,waterlogged=false) — calling
    // setValue(FACING, EAST) should produce (facing=east,half=bottom,...) NOT
    // the first matching state in the table (which might be half=top,waterlogged=true).
    const std::string nameStr(name);
    // Find the default state for this block.
    auto defIt = g_defaultStateByName.find(nameStr);
    if (defIt == g_defaultStateByName.end()) {
        return getDefaultBlockStateId(name, fallback);
    }
    const BlockState& defBs = g_blockStates[defIt->second];
    // Build the target props map: default + overrides.
    std::unordered_map<std::string, std::string> target = defBs.properties;
    for (const auto& kv : overrides) target[std::string(kv.first)] = std::string(kv.second);
    // Now find a state for this block whose properties exactly match `target`.
    for (uint32_t id = 0; id < g_blockStates.size(); ++id) {
        const BlockState& bs = g_blockStates[id];
        if (!bs.block || bs.block->name != nameStr) continue;
        if (bs.properties.size() != target.size()) continue;
        bool ok = true;
        for (const auto& [k, v] : target) {
            auto it = bs.properties.find(k);
            if (it == bs.properties.end() || it->second != v) { ok = false; break; }
        }
        if (ok) return id;
    }
    // No exact match — fall back to the block's default state.
    return defIt->second;
}

// -- Minimal fallback if block_states.json isn't embedded ---------------------
static Block* registerBlock(std::string_view name, Block::Properties props,
                             std::unordered_map<std::string, Block*>& byName) {
    auto b = std::make_unique<Block>(props);
    b->name = std::string(name).substr(name.find(':') + 1);
    Block* ptr = b.get();
    g_blockStorage.push_back(std::move(b));
    g_blockRegistry.register_(ResourceLocation::parse(name), ptr);
    byName[std::string(name).substr(name.find(':') + 1)] = ptr;
    g_blocksByName[ptr->name] = ptr;
    g_defaultStateByName[ptr->name] = ptr->registryId;
    ptr->defaultStateId = ptr->registryId;
    return ptr;
}

static void initFallback(std::unordered_map<std::string, Block*>& byName) {
    auto air = Block::Properties{};
    air.hasCollision = false; air.isAir = true; air.isOpaque = false; air.isSolid = false;
    blocks::AIR = registerBlock("minecraft:air", air, byName);

    auto solid = Block::Properties{};
    solid.destroyTime = 1.5f;
    blocks::STONE = registerBlock("minecraft:stone", solid, byName);
    blocks::STONE->textures.all = "stone";

    auto grass_p = solid;
    blocks::GRASS_BLOCK = registerBlock("minecraft:grass_block", grass_p, byName);
    blocks::GRASS_BLOCK->textures.top = "grass_block_top";
    blocks::GRASS_BLOCK->textures.side = "grass_block_side";
    blocks::GRASS_BLOCK->textures.bot = blocks::GRASS_BLOCK->textures.all = "dirt";

    blocks::DIRT = registerBlock("minecraft:dirt", solid, byName);
    blocks::DIRT->textures.all = "dirt";

    auto fluid_p = Block::Properties{};
    fluid_p.hasCollision = false; fluid_p.isOpaque = false;
    fluid_p.isFluid = true; fluid_p.isSolid = false;
    blocks::WATER = registerBlock("minecraft:water", fluid_p, byName);
    blocks::WATER->textures.all = "water_still";
    blocks::LAVA  = registerBlock("minecraft:lava", fluid_p, byName);
    blocks::LAVA->textures.all = "lava_still";
    blocks::BEDROCK = registerBlock("minecraft:bedrock", solid, byName);
    blocks::BEDROCK->textures.all = "bedrock";
    blocks::DEEPSLATE = registerBlock("minecraft:deepslate", solid, byName);
    blocks::DEEPSLATE->textures.all = "deepslate";

    blocks::SAND = registerBlock("minecraft:sand", solid, byName);
    blocks::SAND->textures.all = "sand";
    blocks::GRAVEL = registerBlock("minecraft:gravel", solid, byName);
    blocks::GRAVEL->textures.all = "gravel";
    blocks::NETHERRACK = registerBlock("minecraft:netherrack", solid, byName);
    blocks::NETHERRACK->textures.all = "netherrack";
    blocks::END_STONE = registerBlock("minecraft:end_stone", solid, byName);
    blocks::END_STONE->textures.all = "end_stone";
    blocks::OAK_LOG = registerBlock("minecraft:oak_log", solid, byName);
    blocks::OAK_LOG->textures.all = "oak_log";
    blocks::OAK_LOG->textures.top = blocks::OAK_LOG->textures.bot = "oak_log_top";

    for (const char* b : {
             "minecraft:granite", "minecraft:diorite", "minecraft:andesite", "minecraft:tuff",
             "minecraft:clay", "minecraft:blackstone", "minecraft:magma_block", "minecraft:soul_sand",
             "minecraft:coal_ore", "minecraft:deepslate_coal_ore", "minecraft:iron_ore", "minecraft:deepslate_iron_ore",
             "minecraft:copper_ore", "minecraft:deepslate_copper_ore", "minecraft:gold_ore", "minecraft:deepslate_gold_ore",
             "minecraft:redstone_ore", "minecraft:deepslate_redstone_ore", "minecraft:lapis_ore", "minecraft:deepslate_lapis_ore",
             "minecraft:diamond_ore", "minecraft:deepslate_diamond_ore", "minecraft:emerald_ore", "minecraft:deepslate_emerald_ore",
             "minecraft:nether_gold_ore", "minecraft:nether_quartz_ore", "minecraft:ancient_debris", "minecraft:infested_stone",
             "minecraft:infested_deepslate", "minecraft:raw_copper_block", "minecraft:raw_iron_block" }) {
        registerBlock(b, solid, byName)->textures.all = std::string(b).substr(10);
    }

    auto leaves_p = solid;
    leaves_p.isOpaque = false; leaves_p.noOcclusion = true;
    blocks::OAK_LEAVES = registerBlock("minecraft:oak_leaves", leaves_p, byName);
    blocks::OAK_LEAVES->textures.all = "oak_leaves";

    for (const char* wood : { "birch", "spruce", "jungle", "acacia", "dark_oak", "cherry", "mangrove", "pale_oak" }) {
        registerBlock(("minecraft:" + std::string(wood) + "_log"), solid, byName)
            ->textures.all = wood + std::string("_log");
        registerBlock(("minecraft:" + std::string(wood) + "_leaves"), leaves_p, byName)
            ->textures.all = wood + std::string("_leaves");
    }
    for (const char* b : { "minecraft:red_mushroom_block", "minecraft:brown_mushroom_block", "minecraft:mushroom_stem",
                           "minecraft:moss_block", "minecraft:pale_moss_block", "minecraft:nether_wart_block",
                           "minecraft:warped_wart_block", "minecraft:shroomlight", "minecraft:chorus_plant", "minecraft:chorus_flower",
                           "minecraft:mangrove_roots", "minecraft:muddy_mangrove_roots", "minecraft:mud", "minecraft:moss_carpet",
                           "minecraft:crimson_stem", "minecraft:warped_stem", "minecraft:sculk", "minecraft:rooted_dirt",
                           "minecraft:tube_coral_block", "minecraft:brain_coral_block", "minecraft:bubble_coral_block",
                           "minecraft:fire_coral_block", "minecraft:horn_coral_block" }) {
        registerBlock(b, solid, byName)->textures.all = std::string(b).substr(10);
    }

    auto glass_p = solid;
    glass_p.isOpaque = false; glass_p.noOcclusion = true;
    blocks::GLASS = registerBlock("minecraft:glass", glass_p, byName);
    blocks::GLASS->textures.all = "glass";

    auto plant_p = Block::Properties{};
    plant_p.hasCollision = false; plant_p.isOpaque = false; plant_p.isSolid = false;
    plant_p.noOcclusion = true;
    for (const char* plant : {
             "minecraft:short_grass", "minecraft:fern", "minecraft:tall_grass", "minecraft:large_fern",
             "minecraft:dandelion", "minecraft:poppy", "minecraft:blue_orchid", "minecraft:allium",
             "minecraft:azure_bluet", "minecraft:oxeye_daisy", "minecraft:cornflower", "minecraft:lily_of_the_valley",
             "minecraft:red_tulip", "minecraft:orange_tulip", "minecraft:white_tulip", "minecraft:pink_tulip",
             "minecraft:dead_bush", "minecraft:sugar_cane", "minecraft:lily_pad", "minecraft:sweet_berry_bush",
             "minecraft:brown_mushroom", "minecraft:red_mushroom", "minecraft:sunflower", "minecraft:lilac",
             "minecraft:rose_bush", "minecraft:peony", "minecraft:wither_rose", "minecraft:torchflower",
             "minecraft:pink_petals", "minecraft:wildflowers", "minecraft:bush", "minecraft:firefly_bush",
             "minecraft:leaf_litter", "minecraft:short_dry_grass", "minecraft:tall_dry_grass", "minecraft:cactus",
             "minecraft:bamboo", "minecraft:melon", "minecraft:pumpkin", "minecraft:spore_blossom",
             "minecraft:closed_eyeblossom", "minecraft:open_eyeblossom", "minecraft:pitcher_plant", "minecraft:seagrass",
             "minecraft:tall_seagrass", "minecraft:kelp", "minecraft:kelp_plant", "minecraft:sea_pickle",
             "minecraft:cave_vines", "minecraft:cave_vines_plant", "minecraft:hanging_roots", "minecraft:glow_lichen",
             "minecraft:nether_sprouts", "minecraft:crimson_roots", "minecraft:warped_roots", "minecraft:crimson_fungus",
             "minecraft:warped_fungus", "minecraft:twisting_vines", "minecraft:twisting_vines_plant", "minecraft:weeping_vines",
             "minecraft:weeping_vines_plant", "minecraft:vine", "minecraft:big_dripleaf", "minecraft:small_dripleaf" }) {
        Block* block = registerBlock(plant, plant_p, byName);
        setFallbackTexture(block, std::string(plant));
    }

    g_blockStates.resize(g_blockRegistry.size());
    for (uint32_t i = 0; i < g_blockRegistry.size(); ++i) {
        g_blockStates[i].block   = g_blockRegistry.getById(i);
        g_blockStates[i].stateId = i;
    }
}

void initBlocks() {
    std::unordered_map<std::string, Block*> byName;

    const char* raw = nullptr;
    std::size_t sz = 0;
    std::string fileData;
#if defined(_WIN32)
    HMODULE hmod = GetModuleHandleW(nullptr);
    HRSRC   hres = FindResourceW(hmod, MAKEINTRESOURCEW(IDR_BLOCK_STATES), MAKEINTRESOURCEW(10));
    if (hres) {
        HGLOBAL hg = LoadResource(hmod, hres);
        raw = static_cast<const char*>(LockResource(hg));
        sz = SizeofResource(hmod, hres);
    }
#endif
    if (!raw) {
        if (const char* path = std::getenv("MCPP_BLOCK_STATES")) {
            std::ifstream in(path, std::ios::binary);
            if (in) {
                std::stringstream ss;
                ss << in.rdbuf();
                fileData = ss.str();
                raw = fileData.data();
                sz = fileData.size();
            }
        }
    }
    if (!raw) {
        MC_LOG_WARN("Blocks: block_states.json not available - using fallback registry");
        initFallback(byName);
        return;
    }

    try {
        auto j = nlohmann::json::parse(raw, raw + sz);
        int total = j["total"].get<int>();

        g_blockStates.resize((size_t)total);

        for (auto& jst : j["states"]) {
            int id = jst["id"].get<int>();
            if (id < 0 || id >= total) continue;

            std::string name = jst["name"].get<std::string>();

            Block* blk = nullptr;
            auto it = byName.find(name);
            if (it != byName.end()) {
                blk = it->second;
            } else {
                Block::Properties props{};
                props.isAir        = jst["is_air"].get<bool>();
                props.isOpaque     = jst["is_opaque"].get<bool>();
                props.isSolid      = jst["is_solid"].get<bool>();
                props.isFluid      = jst["is_fluid"].get<bool>();
                props.hasCollision = props.isSolid;
                props.noOcclusion  = !props.isOpaque && !props.isAir;

                auto b = std::make_unique<Block>(props);
                b->name = name;
                b->textures.all  = jst.value("tex_side", "");
                b->textures.top  = jst.value("tex_top",  "");
                b->textures.side = jst.value("tex_side", "");
                b->textures.bot  = jst.value("tex_bot",  "");

                blk = b.get();
                g_blockStorage.push_back(std::move(b));
                g_blockRegistry.register_(ResourceLocation::parse("minecraft:" + name), blk);
                byName[name] = blk;
                g_blocksByName[name] = blk;
            }

            const bool isDefault = jst.value("default", false);
            if (isDefault || !g_defaultStateByName.contains(name)) {
                g_defaultStateByName[name] = (uint32_t)id;
                blk->defaultStateId = (uint32_t)id;
            }

            g_blockStates[id].block   = blk;
            g_blockStates[id].stateId = (uint32_t)id;
            if (auto pit = jst.find("props"); pit != jst.end() && !pit->get<std::string>().empty()) {
                const std::string& ps = pit->get_ref<const std::string&>();
                g_blockStates[id].props = ps;
                size_t start = 0;
                while (start < ps.size()) {
                    size_t comma = ps.find(',', start);
                    if (comma == std::string::npos) comma = ps.size();
                    size_t eq = ps.find('=', start);
                    if (eq != std::string::npos && eq < comma) {
                        g_blockStates[id].properties.emplace(ps.substr(start, eq - start), ps.substr(eq + 1, comma - eq - 1));
                    }
                    start = comma + 1;
                }
            }
        }
    } catch (const std::exception& e) {
        MC_LOG_WARN("Blocks: failed to parse block_states.json: {} - using fallback registry", e.what());
        g_blockRegistry = Registry<Block>();
        g_blockStorage.clear();
        g_blockStates.clear();
        g_blocksByName.clear();
        g_defaultStateByName.clear();
        initFallback(byName);
        return;
    }

    blocks::AIR         = getBlockByName("air");
    blocks::STONE       = getBlockByName("stone");
    blocks::GRASS_BLOCK = getBlockByName("grass_block");
    blocks::DIRT        = getBlockByName("dirt");
    blocks::WATER       = getBlockByName("water");
    blocks::LAVA        = getBlockByName("lava");
    blocks::BEDROCK     = getBlockByName("bedrock");
    blocks::DEEPSLATE   = getBlockByName("deepslate");
    blocks::SAND        = getBlockByName("sand");
    blocks::GRAVEL      = getBlockByName("gravel");
    blocks::NETHERRACK  = getBlockByName("netherrack");
    blocks::END_STONE   = getBlockByName("end_stone");
    blocks::OAK_LOG     = getBlockByName("oak_log");
    blocks::OAK_LEAVES  = getBlockByName("oak_leaves");
    blocks::GLASS       = getBlockByName("glass");

    assignKnownBlockTextures();
    MC_LOG_INFO("Blocks: loaded {} block states, {} blocks", g_blockStates.size(), g_blockRegistry.size());
}

const Block* getBlock(uint32_t stateId) {
    const BlockState* bs = getBlockState(stateId);
    return bs ? bs->block : nullptr;
}

} // namespace mc

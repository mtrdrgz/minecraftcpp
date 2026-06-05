#include "Blocks.h"
#include "../../../core/Log.h"
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
#include <string>

namespace mc {

Registry<Block>                     g_blockRegistry;
std::vector<std::unique_ptr<Block>> g_blockStorage;
std::vector<BlockState>             g_blockStates;
static std::unordered_map<std::string, Block*> g_blocksByName;
static std::unordered_map<std::string, uint32_t> g_defaultStateByName;

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

// ── Minimal fallback if block_states.json isn't embedded ──────────────────────
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
    blocks::LAVA  = registerBlock("minecraft:lava", fluid_p, byName);
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

    auto leaves_p = solid;
    leaves_p.isOpaque = false; leaves_p.noOcclusion = true;
    blocks::OAK_LEAVES = registerBlock("minecraft:oak_leaves", leaves_p, byName);
    blocks::OAK_LEAVES->textures.all = "oak_leaves";

    // Tree woods for the decoration step's tree configs (oak above; the rest here)
    // so they resolve to real states on the fallback registry.
    for (const char* wood : { "birch", "spruce", "jungle", "acacia", "dark_oak", "cherry", "mangrove", "pale_oak" }) {
        registerBlock(("minecraft:" + std::string(wood) + "_log"), solid, byName)
            ->textures.all = wood + std::string("_log");
        registerBlock(("minecraft:" + std::string(wood) + "_leaves"), leaves_p, byName)
            ->textures.all = wood + std::string("_leaves");
    }
    // Solid decoration blocks (huge mushroom caps/stem, moss, chorus) — rendered as voxels.
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

    // Surface vegetation (non-collidable, non-opaque plants). Required so the
    // biome-decoration step can actually place them when running on the fallback
    // registry (no block_states.json); the full table supersedes this on Windows.
    auto plant_p = Block::Properties{};
    plant_p.hasCollision = false; plant_p.isOpaque = false; plant_p.isSolid = false;
    plant_p.noOcclusion = true;
    for (const char* plant : {
             "minecraft:short_grass", "minecraft:fern", "minecraft:tall_grass", "minecraft:large_fern",
             "minecraft:dandelion", "minecraft:poppy", "minecraft:blue_orchid", "minecraft:allium",
             "minecraft:azure_bluet", "minecraft:oxeye_daisy", "minecraft:cornflower", "minecraft:lily_of_the_valley",
             "minecraft:red_tulip", "minecraft:orange_tulip", "minecraft:white_tulip", "minecraft:pink_tulip",
             "minecraft:dead_bush", "minecraft:sugar_cane", "minecraft:lily_pad", "minecraft:sweet_berry_bush",
             // wider surface vegetation referenced by the data-driven decoration features
             "minecraft:brown_mushroom", "minecraft:red_mushroom", "minecraft:sunflower", "minecraft:lilac",
             "minecraft:rose_bush", "minecraft:peony", "minecraft:wither_rose", "minecraft:torchflower",
             "minecraft:pink_petals", "minecraft:wildflowers", "minecraft:bush", "minecraft:firefly_bush",
             "minecraft:leaf_litter", "minecraft:short_dry_grass", "minecraft:tall_dry_grass", "minecraft:cactus",
             "minecraft:bamboo", "minecraft:melon", "minecraft:pumpkin", "minecraft:spore_blossom",
             "minecraft:closed_eyeblossom", "minecraft:open_eyeblossom", "minecraft:pitcher_plant", "minecraft:seagrass",
             // underwater / nether / cave vegetation placed by the data-driven features
             "minecraft:tall_seagrass", "minecraft:kelp", "minecraft:kelp_plant", "minecraft:sea_pickle",
             "minecraft:cave_vines", "minecraft:cave_vines_plant", "minecraft:hanging_roots", "minecraft:glow_lichen",
             "minecraft:nether_sprouts", "minecraft:crimson_roots", "minecraft:warped_roots", "minecraft:crimson_fungus",
             "minecraft:warped_fungus", "minecraft:twisting_vines", "minecraft:twisting_vines_plant", "minecraft:weeping_vines",
             "minecraft:weeping_vines_plant", "minecraft:vine", "minecraft:big_dripleaf", "minecraft:small_dripleaf" }) {
        registerBlock(plant, plant_p, byName);
    }

    // Build minimal state table: one state per block, state 0 = air
    g_blockStates.resize(g_blockRegistry.size());
    for (uint32_t i = 0; i < g_blockRegistry.size(); ++i) {
        g_blockStates[i].block   = g_blockRegistry.getById(i);
        g_blockStates[i].stateId = i;
    }
}

// ── Main init — loads full state table from block_states.json ─────────────────
void initBlocks() {
    std::unordered_map<std::string, Block*> byName;

    const char* raw = nullptr;
    std::size_t sz = 0;
    std::string fileData;
#if defined(_WIN32)
    HMODULE hmod = GetModuleHandleW(nullptr);
    HRSRC   hres = FindResourceW(hmod, MAKEINTRESOURCEW(IDR_BLOCK_STATES), RT_RCDATA);
    if (hres) {
        HGLOBAL hg = LoadResource(hmod, hres);
        raw = static_cast<const char*>(LockResource(hg));
        sz = SizeofResource(hmod, hres);
    }
#else
    // Non-Windows: load block_states.json from $MCPP_BLOCK_STATES if provided.
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
#endif
    if (!raw) {
        MC_LOG_WARN("Blocks: block_states.json not available — using fallback registry");
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

            if (!g_defaultStateByName.contains(name)) {
                g_defaultStateByName[name] = (uint32_t)id;
                blk->defaultStateId = (uint32_t)id;
            }

            g_blockStates[id].block   = blk;
            g_blockStates[id].stateId = (uint32_t)id;
            const uint32_t baseId = getDefaultBlockStateId(name, (uint32_t)id);
            const uint32_t offset = (uint32_t)id - baseId;
            if (isPillarBlockName(name) && offset < 3) {
                static constexpr const char* AXES[3] = { "x", "y", "z" };
                g_blockStates[id].properties["axis"] = AXES[offset];
            }
        }

        // Fix any gaps (states not in the JSON map to air)
        Block* airBlk = byName.count("air") ? byName["air"] : nullptr;
        for (auto& bs : g_blockStates) {
            if (!bs.block) {
                bs.block = airBlk;
            }
        }

        MC_LOG_INFO("Blocks: {} blocks, {} states loaded", byName.size(), total);
    } catch (const std::exception& e) {
        MC_LOG_ERROR("Blocks: JSON parse error: {} — falling back", e.what());
        // Clear partial state and use fallback
        g_blockStates.clear();
        g_blockStorage.clear();
        g_blocksByName.clear();
        g_defaultStateByName.clear();
        g_blockRegistry = Registry<Block>{};
        byName.clear();
        initFallback(byName);
        return;
    }

    // Set global convenience pointers
    auto get = [&](const char* n) -> Block* {
        auto it = byName.find(n);
        return (it != byName.end()) ? it->second : nullptr;
    };
    blocks::AIR         = get("air");
    blocks::STONE       = get("stone");
    blocks::GRASS_BLOCK = get("grass_block");
    blocks::DIRT        = get("dirt");
    blocks::WATER       = get("water");
    blocks::LAVA        = get("lava");
    blocks::BEDROCK     = get("bedrock");
    blocks::DEEPSLATE   = get("deepslate");
    blocks::SAND        = get("sand");
    blocks::GRAVEL      = get("gravel");
    blocks::NETHERRACK  = get("netherrack");
    blocks::END_STONE   = get("end_stone");
    blocks::OAK_LOG     = get("oak_log");
    blocks::OAK_LEAVES  = get("oak_leaves");
    blocks::GLASS       = get("glass");
}

} // namespace mc

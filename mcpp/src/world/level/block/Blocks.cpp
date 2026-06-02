#include "Blocks.h"
#include "../../../core/Log.h"
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

    auto glass_p = solid;
    glass_p.isOpaque = false; glass_p.noOcclusion = true;
    blocks::GLASS = registerBlock("minecraft:glass", glass_p, byName);
    blocks::GLASS->textures.all = "glass";

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

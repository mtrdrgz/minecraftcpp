#pragma once
#include "Block.h"
#include "BlockState.h"
#include "../../../core/Registry.h"
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mc {

// Global block registry — holds all Block instances
extern Registry<Block> g_blockRegistry;
// All block instances (owns the memory)
extern std::vector<std::unique_ptr<Block>> g_blockStorage;
// Per-state table (id -> BlockState). Populated by initBlocks().
extern std::vector<BlockState> g_blockStates;
// Reverse lookups used by getDefaultBlockStateId / getBlockByName. Exposed so
// standalone parity binaries can populate them from block_states.json without
// going through the full engine init.
extern std::unordered_map<std::string, Block*> g_blocksByName;
extern std::unordered_map<std::string, uint32_t> g_defaultStateByName;

// Initializes the block registry from the embedded blocks.json asset
// (assets/minecraft/blocks.json in the Minecraft jar, describes all state IDs)
void initBlocks(/* AssetPack* pack */);

Block* getBlockByName(std::string_view name);
uint32_t getDefaultBlockStateId(std::string_view name, uint32_t fallback = 0);
uint32_t getBlockStateId(std::string_view serializedState, uint32_t fallback = 0);
const BlockState* getDefaultBlockState(std::string_view name);

// Look up a block state id by block name + a subset of properties (e.g.
// "spruce_stairs" with {{facing,east},{shape,outer_left}}). Returns the first
// state whose block name matches AND every key/value in `overrides` matches the
// state's parsed properties map. Falls back to `getDefaultBlockStateId(name)`
// if no match is found (e.g. block_states.json not loaded yet — important for
// parity tests that don't init the full registry). Used by structure pieces
// that need stair/slab/wall facing & shape variants (SwampHutPiece, etc.).
uint32_t getBlockStateIdWith(std::string_view name,
                             const std::initializer_list<std::pair<std::string_view, std::string_view>>& overrides,
                             uint32_t fallback = 0);

// Convenience references to important blocks (set during initBlocks)
namespace blocks {
    extern Block* AIR;
    extern Block* STONE;
    extern Block* GRASS_BLOCK;
    extern Block* DIRT;
    extern Block* WATER;
    extern Block* LAVA;
    extern Block* BEDROCK;
    extern Block* DEEPSLATE;
    extern Block* SAND;
    extern Block* GRAVEL;
    extern Block* NETHERRACK;
    extern Block* END_STONE;
    extern Block* OAK_LOG;
    extern Block* OAK_LEAVES;
    extern Block* GLASS;
} // namespace blocks

} // namespace mc

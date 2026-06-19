#pragma once
#include "Block.h"
#include "BlockState.h"
#include "../../../core/Registry.h"
#include <memory>
#include <string_view>
#include <vector>

namespace mc {

// Global block registry — holds all Block instances
extern Registry<Block> g_blockRegistry;
// All block instances (owns the memory)
extern std::vector<std::unique_ptr<Block>> g_blockStorage;

// Initializes the block registry from the embedded blocks.json asset
// (assets/minecraft/blocks.json in the Minecraft jar, describes all state IDs)
void initBlocks(/* AssetPack* pack */);

Block* getBlockByName(std::string_view name);
uint32_t getDefaultBlockStateId(std::string_view name, uint32_t fallback = 0);
uint32_t getBlockStateId(std::string_view serializedState, uint32_t fallback = 0);
const BlockState* getDefaultBlockState(std::string_view name);

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

#pragma once
#include "Block.h"
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

namespace mc {

// Port of net.minecraft.world.level.block.state.BlockState
// Each distinct combination of a block's properties = one BlockState
// BlockStates are identified by a global numeric state ID
struct BlockState {
    const Block* block     = nullptr;
    uint32_t     stateId   = 0;   // global state ID (matches protocol)

    // Properties as string key-value (e.g. "facing" -> "north")
    std::unordered_map<std::string, std::string> properties;

    bool isAir()    const { return block && block->isAir(); }
    bool isOpaque() const { return block && block->isOpaque(); }
    bool isSolid()  const { return block && block->isSolid(); }
    bool isFluid()  const { return block && block->isFluid(); }
    uint8_t lightLevel() const { return block ? block->lightLevel() : 0; }

    bool hasProperty(std::string_view key) const {
        return properties.find(std::string(key)) != properties.end();
    }
    std::string getProperty(std::string_view key) const {
        auto it = properties.find(std::string(key));
        return it == properties.end() ? "" : it->second;
    }
};

// Global state table: indexed by state ID
// Populated at startup from the block registry
extern std::vector<BlockState> g_blockStates;

// Convenience accessor
inline const BlockState* getBlockState(uint32_t id) {
    if (id < g_blockStates.size()) return &g_blockStates[id];
    return &g_blockStates[0]; // air
}

} // namespace mc

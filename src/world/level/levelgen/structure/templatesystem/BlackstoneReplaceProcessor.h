#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.structure.templatesystem.
// BlackstoneReplaceProcessor (the INSTANCE map) — swaps stone/cobble family blocks to
// their blackstone equivalents (used by ruined_portal in the Nether), copying the
// stair FACING/HALF and slab TYPE. Pure state->state on the global registry; no RNG.

#include "../../../block/Blocks.h"
#include "../../../block/BlockState.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace mc::levelgen::structure {

namespace blackstone_detail {
// BlackstoneReplaceProcessor static map (block name -> replacement block name).
inline const std::unordered_map<std::string, std::string>& replacements() {
    static const std::unordered_map<std::string, std::string> M = {
        {"cobblestone", "blackstone"},
        {"mossy_cobblestone", "blackstone"},
        {"stone", "polished_blackstone"},
        {"stone_bricks", "polished_blackstone_bricks"},
        {"mossy_stone_bricks", "polished_blackstone_bricks"},
        {"cobblestone_stairs", "blackstone_stairs"},
        {"mossy_cobblestone_stairs", "blackstone_stairs"},
        {"stone_stairs", "polished_blackstone_stairs"},
        {"stone_brick_stairs", "polished_blackstone_brick_stairs"},
        {"mossy_stone_brick_stairs", "polished_blackstone_brick_stairs"},
        {"cobblestone_slab", "blackstone_slab"},
        {"mossy_cobblestone_slab", "blackstone_slab"},
        {"smooth_stone_slab", "polished_blackstone_slab"},
        {"stone_slab", "polished_blackstone_slab"},
        {"stone_brick_slab", "polished_blackstone_brick_slab"},
        {"mossy_stone_brick_slab", "polished_blackstone_brick_slab"},
        {"stone_brick_wall", "polished_blackstone_brick_wall"},
        {"mossy_stone_brick_wall", "polished_blackstone_brick_wall"},
        {"cobblestone_wall", "blackstone_wall"},
        {"mossy_cobblestone_wall", "blackstone_wall"},
        {"chiseled_stone_bricks", "chiseled_polished_blackstone"},
        {"cracked_stone_bricks", "cracked_polished_blackstone_bricks"},
        {"iron_bars", "iron_chain"},
    };
    return M;
}
}  // namespace blackstone_detail

// BlackstoneReplaceProcessor.processBlock — returns the replaced state id, or stateId
// unchanged when the block has no blackstone equivalent. Copies stair FACING/HALF and
// slab TYPE from the source state onto the default replacement state.
inline uint32_t blackstoneReplaceProcess(uint32_t stateId) {
    const mc::BlockState* src = mc::getBlockState(stateId);
    if (!src || !src->block) return stateId;
    auto it = blackstone_detail::replacements().find(src->block->name);
    if (it == blackstone_detail::replacements().end()) return stateId;

    const std::string& repl = it->second;
    // Copy the stair (facing/half) and slab (type) properties iff present on the source.
    const std::string facing = src->getProperty("facing");
    const std::string half = src->getProperty("half");
    const std::string type = src->getProperty("type");
    if (!facing.empty() && !half.empty())  // StairBlock.FACING + HALF
        return mc::getBlockStateIdWith(repl, {{"facing", facing}, {"half", half}});
    if (!type.empty())                     // SlabBlock.TYPE
        return mc::getBlockStateIdWith(repl, {{"type", type}});
    return mc::getBlockStateId("minecraft:" + repl);
}

}  // namespace mc::levelgen::structure

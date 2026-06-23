#pragma once

// 1:1 ports of two world-aware template processors used by RuinedPortalPiece:
//   * ProtectedBlockProcessor (ProtectedBlockProcessor.java): drop the template block
//     iff the EXISTING world block is non-replaceable (in the cannotReplace tag) — so the
//     structure never overwrites protected blocks.
//   * LavaSubmergedBlockProcessor (LavaSubmergedBlockProcessor.java): if the world block
//     was LAVA and the template block is not a full cube, place LAVA instead (keeps lava
//     visible through the ruin's gaps).
//
// Both need the world block at the target position, so they're applied inside placeTemplate
// (which has the StructureWorld) rather than as pure state->state functions. The lava
// shape test uses the block's full-opaque-cube flag as Block.isShapeFullBlock (the ruin
// templates only contain full stone cubes vs stairs/slabs/air, which this distinguishes).

#include "../../../block/Blocks.h"
#include "../../../block/BlockState.h"
#include "../../../block/BlockTags.h"

#include <cstdint>
#include <string>

namespace mc::levelgen::structure {

// ProtectedBlockProcessor.processBlock: keep iff Feature.isReplaceable(cannotReplace)
// .test(worldState) — i.e. the world block is NOT in the cannotReplace tag.
inline bool protectedBlockKeep(uint32_t worldStateId, const mc::block::BlockTags& tags,
                               const std::string& cannotReplaceTag = "minecraft:features_cannot_replace") {
    const mc::BlockState* w = mc::getBlockState(worldStateId);
    if (!w || !w->block) return true;  // air/unknown is replaceable
    return !tags.isInTag("minecraft:" + w->block->name, cannotReplaceTag);
}

// LavaSubmergedBlockProcessor.processBlock: wasLava && !isShapeFullBlock(template) -> LAVA.
// Returns the lava state id when the swap applies, else `templateStateId` unchanged.
inline uint32_t lavaSubmergedProcess(uint32_t worldStateId, uint32_t templateStateId) {
    const mc::BlockState* w = mc::getBlockState(worldStateId);
    const bool wasLava = w && w->block && w->block->name == "lava";
    if (!wasLava) return templateStateId;
    const mc::BlockState* t = mc::getBlockState(templateStateId);
    const bool fullBlock = t && t->isOpaque();  // Block.isShapeFullBlock approximation
    if (fullBlock) return templateStateId;
    return mc::getBlockStateId("minecraft:lava");
}

}  // namespace mc::levelgen::structure

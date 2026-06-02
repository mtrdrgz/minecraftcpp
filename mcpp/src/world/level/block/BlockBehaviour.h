#pragma once

// Block-survival rules (the canSurvive boundary that features gate on), ported
// 1:1 from the decompiled block classes. Currently covers the VegetationBlock
// family (short_grass, fern, the common flowers) — the surface vegetation set.
// More families (DryVegetationBlock, sugar cane, cactus, ...) are added as their
// features are ported.

#include "BlockTags.h"

#include <string>

namespace mc::block {

// VegetationBlock.canSurvive(state, level, pos):
//   mayPlaceOn(level.getBlockState(pos.below())) = below.is(SUPPORTS_VEGETATION)
inline bool vegetationBlockCanSurvive(const std::string& belowBlock, const BlockTags& tags) {
    return tags.isInTag(belowBlock, "minecraft:supports_vegetation");
}

} // namespace mc::block

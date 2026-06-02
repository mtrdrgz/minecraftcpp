#pragma once

// Block-survival rules (the canSurvive boundary that features gate on), ported
// 1:1 from the decompiled block classes. Covers the surface-vegetation families:
//   VegetationBlock  (grass, ferns, single flowers)  -> SUPPORTS_VEGETATION
//   DoublePlantBlock (tall_grass, large_fern, double flowers; extends
//                     VegetationBlock, so lower half) -> SUPPORTS_VEGETATION
//   DryVegetationBlock (dead_bush, dry grass)         -> SUPPORTS_DRY_VEGETATION
// Other families (cactus, sugar cane, mushrooms, ...) have bespoke rules and are
// added as their features are ported. The block->family mapping is a focused set
// for this vegetation subset (the full set comes with the block registry port).

#include "BlockStates.h"
#include "BlockTags.h"

#include <set>
#include <string>

namespace mc::block {

inline const std::set<std::string>& doublePlantBlocks() {
    static const std::set<std::string> s = {
        "minecraft:tall_grass", "minecraft:large_fern", "minecraft:sunflower",
        "minecraft:lilac", "minecraft:rose_bush", "minecraft:peony", "minecraft:pitcher_plant",
    };
    return s;
}

inline const std::set<std::string>& dryVegetationBlocks() {
    static const std::set<std::string> s = {
        "minecraft:dead_bush", "minecraft:short_dry_grass", "minecraft:tall_dry_grass",
    };
    return s;
}

inline bool isDoublePlant(const std::string& blockOrState) {
    return doublePlantBlocks().count(blockName(blockOrState)) != 0;
}

// VegetationBlock.canSurvive / DryVegetationBlock.mayPlaceOn etc.: the below
// block must be in the family's support tag.
inline bool canSurvive(const std::string& plantBlockOrState, const std::string& belowBlock, const BlockTags& tags) {
    const std::string block = blockName(plantBlockOrState);
    if (dryVegetationBlocks().count(block) != 0) {
        return tags.isInTag(belowBlock, "minecraft:supports_dry_vegetation");
    }
    // VegetationBlock + DoublePlantBlock (lower half) family
    return tags.isInTag(belowBlock, "minecraft:supports_vegetation");
}

// Kept for the existing parity test.
inline bool vegetationBlockCanSurvive(const std::string& belowBlock, const BlockTags& tags) {
    return tags.isInTag(belowBlock, "minecraft:supports_vegetation");
}
inline bool dryVegetationBlockCanSurvive(const std::string& belowBlock, const BlockTags& tags) {
    return tags.isInTag(belowBlock, "minecraft:supports_dry_vegetation");
}

} // namespace mc::block

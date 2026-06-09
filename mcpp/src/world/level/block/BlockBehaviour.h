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

// Full-block "plants" (PumpkinBlock/MelonBlock extend Block): canSurvive is the
// Block default (true); their ground is gated by the placed-feature's
// block_predicate_filter, not canSurvive.
inline const std::set<std::string>& fullBlockPlants() {
    static const std::set<std::string> s = { "minecraft:pumpkin", "minecraft:melon" };
    return s;
}

inline bool isDoublePlant(const std::string& blockOrState) {
    return doublePlantBlocks().count(blockName(blockOrState)) != 0;
}

// canSurvive dispatch for the surface-vegetation set. The VegetationBlock family
// (grass, ferns, all standard flowers, bush, sweet_berry_bush, firefly_bush, the
// double plants) shares SUPPORTS_VEGETATION; DryVegetationBlock uses
// SUPPORTS_DRY_VEGETATION; full-block plants survive anywhere.
// NOTE: mushrooms (light-dependent), cactus (neighbour-dependent, block_column),
// and wither_rose (netherrack) have bespoke rules and are NOT handled here.
inline bool canSurvive(const std::string& plantBlockOrState, const std::string& belowBlock, const BlockTags& tags) {
    const std::string block = blockName(plantBlockOrState);
    if (dryVegetationBlocks().count(block) != 0) {
        return tags.isInTag(belowBlock, "minecraft:supports_dry_vegetation");
    }
    if (fullBlockPlants().count(block) != 0) {
        return true;
    }
    return tags.isInTag(belowBlock, "minecraft:supports_vegetation");
}

// Kept for the existing parity test.
inline bool vegetationBlockCanSurvive(const std::string& belowBlock, const BlockTags& tags) {
    return tags.isInTag(belowBlock, "minecraft:supports_vegetation");
}
inline bool dryVegetationBlockCanSurvive(const std::string& belowBlock, const BlockTags& tags) {
    return tags.isInTag(belowBlock, "minecraft:supports_dry_vegetation");
}

// ---------------------------------------------------------------------------
// BlockStateBase.blocksMotion() (BlockBehaviour.java:540-543):
//   block != COBWEB && block != BAMBOO_SAPLING && legacySolid
// legacySolid = calculateSolid() (BlockBehaviour.java:482-502): forceSolidOn ->
// true; forceSolidOff -> false; empty collision shape -> false; else solid if the
// collision bounds are (near-)full. Evaluated here over the closed set of blocks
// the worldgen pipeline can produce, each entry grounded in its Blocks.java
// registration:
//   FALSE - air/cave_air/void_air        isAir, no collision
//         - water/lava                   LiquidBlock, empty collision shape
//         - seagrass (Blocks.java:769-778), tall_seagrass (:780-790),
//           kelp (:4788-4797), kelp_plant (:4799-4802),
//           glow_lichen (:2464-2475), bubble_column (:5236-5241): .noCollission()
//         - snow (:1999-2004): .forceSolidOff()
//         - cobweb / bamboo_sapling: explicit exclusions in blocksMotion()
//   TRUE  - every other block the overworld generator/carvers/surface rules and
//           the ported features emit (stone family, deepslate, bedrock, sand,
//           gravel, dirt/grass_block, clay, sandstone, ice/packed_ice/blue_ice,
//           magma_block, obsidian, the ore blocks, ...): all registered as
//           full-cube colliding blocks.
// Blocks outside the enumerated sets default to TRUE (full cubes are the worldgen
// default); pass `defaulted` to surface such ids for verification instead of
// trusting the default silently.
inline bool blocksMotion(const std::string& blockOrState, bool* defaulted = nullptr) {
    static const std::set<std::string> nonMotionBlocking = {
        "minecraft:air", "minecraft:cave_air", "minecraft:void_air",
        "minecraft:water", "minecraft:lava",
        "minecraft:seagrass", "minecraft:tall_seagrass",
        "minecraft:kelp", "minecraft:kelp_plant",
        "minecraft:glow_lichen", "minecraft:bubble_column",
        "minecraft:snow",
        "minecraft:cobweb", "minecraft:bamboo_sapling",
    };
    static const std::set<std::string> knownMotionBlocking = {
        "minecraft:stone", "minecraft:granite", "minecraft:diorite", "minecraft:andesite",
        "minecraft:tuff", "minecraft:deepslate", "minecraft:bedrock", "minecraft:obsidian",
        "minecraft:gravel", "minecraft:sand", "minecraft:red_sand", "minecraft:sandstone",
        "minecraft:dirt", "minecraft:grass_block", "minecraft:coarse_dirt", "minecraft:podzol",
        "minecraft:clay", "minecraft:mud", "minecraft:magma_block", "minecraft:snow_block",
        "minecraft:ice", "minecraft:packed_ice", "minecraft:blue_ice",
        "minecraft:coal_ore", "minecraft:copper_ore", "minecraft:iron_ore", "minecraft:gold_ore",
        "minecraft:redstone_ore", "minecraft:lapis_ore", "minecraft:diamond_ore", "minecraft:emerald_ore",
        "minecraft:deepslate_coal_ore", "minecraft:deepslate_copper_ore", "minecraft:deepslate_iron_ore",
        "minecraft:deepslate_gold_ore", "minecraft:deepslate_redstone_ore", "minecraft:deepslate_lapis_ore",
        "minecraft:deepslate_diamond_ore", "minecraft:deepslate_emerald_ore",
    };
    const std::string block = blockName(blockOrState);
    if (nonMotionBlocking.count(block) != 0) return false;
    if (knownMotionBlocking.count(block) == 0 && defaulted != nullptr) *defaulted = true;
    return true;
}

// BlockStateBase.isFaceSturdy(level, pos, Direction.UP) with SupportType.FULL:
// Block.isFaceFull(state.getBlockSupportShape(...), UP). For the worldgen block
// set this coincides with blocksMotion(): every motion-blocking block the
// pipeline emits is a full cube (sturdy on all faces), and the non-colliding /
// liquid / air blocks have no support shape. Non-full-cube colliding blocks
// (slabs, stairs, snow layers>=8, ...) never occur as worldgen ground here; if
// one ever does, `defaulted` flags it for review.
inline bool isFaceSturdyUp(const std::string& blockOrState, bool* defaulted = nullptr) {
    return blocksMotion(blockOrState, defaulted);
}

} // namespace mc::block

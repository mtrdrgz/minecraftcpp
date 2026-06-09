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
        // .noCollision() registrations (empty collision shape -> legacySolid false):
        // short_grass (Blocks.java "short_grass"), bush ("bush"), lily_of_the_valley,
        // brown_mushroom, red_mushroom, vine, lilac, rose_bush, peony, leaf_litter,
        // oak/birch_sapling (SaplingBlock registrations) — all .noCollision().
        "minecraft:short_grass", "minecraft:bush", "minecraft:lily_of_the_valley",
        "minecraft:brown_mushroom", "minecraft:red_mushroom", "minecraft:vine",
        "minecraft:lilac", "minecraft:rose_bush", "minecraft:peony",
        "minecraft:leaf_litter", "minecraft:oak_sapling", "minecraft:birch_sapling",
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
        // Tree/dungeon family (Blocks.java registrations, all full-cube or near-full
        // colliding):
        //   oak_log/birch_log (logProperties full cubes), oak/birch_leaves (leaves
        //   collide as full cubes; only .noOcclusion() — render-side), bee_nest
        //   (BeehiveBlock full cube), cobblestone / mossy_cobblestone (full cubes),
        //   spawner (full cube, .noOcclusion() only), chest (ChestBlock 14/16 box:
        //   collision bounds avg size 0.875 >= 0.7291666 -> legacySolid TRUE,
        //   BlockBehaviour.java:495-502).
        "minecraft:oak_log", "minecraft:birch_log",
        "minecraft:oak_leaves", "minecraft:birch_leaves",
        "minecraft:bee_nest", "minecraft:cobblestone", "minecraft:mossy_cobblestone",
        "minecraft:spawner", "minecraft:chest",
    };
    const std::string block = blockName(blockOrState);
    if (nonMotionBlocking.count(block) != 0) return false;
    if (knownMotionBlocking.count(block) == 0 && defaulted != nullptr) *defaulted = true;
    return true;
}

// BlockStateBase.isSolid() == legacySolid (BlockBehaviour.java:546-548), and
// blocksMotion() == legacySolid minus the COBWEB/BAMBOO_SAPLING exceptions
// (BlockBehaviour.java:540-543). Neither exception block is producible by the
// worldgen pipeline here, so the blocksMotion table doubles as isSolid.
// Used by MonsterRoomFeature's wall/floor solidity checks (MonsterRoomFeature.java).
inline bool isSolid(const std::string& blockOrState, bool* defaulted = nullptr) {
    return blocksMotion(blockOrState, defaulted);
}

// BlockStateBase.isSolidRender() (BlockBehaviour.java:655-657): cached
// solidRender = Block.isShapeFullBlock(occlusionShape) where occlusionShape is
// EMPTY unless canOcclude (initCache, BlockBehaviour.java:511-517). Consumed by
// PlaceOnGroundDecorator.attemptToPlaceBlockAbove (PlaceOnGroundDecorator.java:81)
// and StructurePiece.reorient. Per Blocks.java registration:
//   NOT solid-render: all no-collision plants (no occlusion shape), water/lava
//     (LiquidBlock), leaves (leavesProperties .noOcclusion()), spawner
//     (.noOcclusion()), chest (non-full 14/16 shape), ice (.noOcclusion()),
//     glow_lichen / bubble_column / snow layers (non-full shapes), air family.
//   solid-render: the full opaque cubes (stone family, deepslate, tuff, bedrock,
//     obsidian, sands/sandstone, gravel, dirt family, grass_block, clay, mud,
//     magma_block, snow_block, packed/blue ice, all ores, logs, bee_nest,
//     cobblestone/mossy_cobblestone).
inline bool isSolidRender(const std::string& blockOrState, bool* defaulted = nullptr) {
    static const std::set<std::string> notSolidRender = {
        "minecraft:air", "minecraft:cave_air", "minecraft:void_air",
        "minecraft:water", "minecraft:lava",
        "minecraft:seagrass", "minecraft:tall_seagrass",
        "minecraft:kelp", "minecraft:kelp_plant",
        "minecraft:glow_lichen", "minecraft:bubble_column", "minecraft:snow",
        "minecraft:ice",
        "minecraft:oak_leaves", "minecraft:birch_leaves",
        "minecraft:short_grass", "minecraft:bush", "minecraft:lily_of_the_valley",
        "minecraft:brown_mushroom", "minecraft:red_mushroom", "minecraft:vine",
        "minecraft:lilac", "minecraft:rose_bush", "minecraft:peony",
        "minecraft:leaf_litter", "minecraft:oak_sapling", "minecraft:birch_sapling",
        "minecraft:spawner", "minecraft:chest",
    };
    static const std::set<std::string> knownSolidRender = {
        "minecraft:stone", "minecraft:granite", "minecraft:diorite", "minecraft:andesite",
        "minecraft:tuff", "minecraft:deepslate", "minecraft:bedrock", "minecraft:obsidian",
        "minecraft:gravel", "minecraft:sand", "minecraft:red_sand", "minecraft:sandstone",
        "minecraft:dirt", "minecraft:grass_block", "minecraft:coarse_dirt", "minecraft:podzol",
        "minecraft:clay", "minecraft:mud", "minecraft:magma_block", "minecraft:snow_block",
        "minecraft:packed_ice", "minecraft:blue_ice",
        "minecraft:coal_ore", "minecraft:copper_ore", "minecraft:iron_ore", "minecraft:gold_ore",
        "minecraft:redstone_ore", "minecraft:lapis_ore", "minecraft:diamond_ore", "minecraft:emerald_ore",
        "minecraft:deepslate_coal_ore", "minecraft:deepslate_copper_ore", "minecraft:deepslate_iron_ore",
        "minecraft:deepslate_gold_ore", "minecraft:deepslate_redstone_ore", "minecraft:deepslate_lapis_ore",
        "minecraft:deepslate_diamond_ore", "minecraft:deepslate_emerald_ore",
        "minecraft:oak_log", "minecraft:birch_log",
        "minecraft:bee_nest", "minecraft:cobblestone", "minecraft:mossy_cobblestone",
    };
    const std::string block = blockName(blockOrState);
    if (notSolidRender.count(block) != 0) return false;
    if (knownSolidRender.count(block) == 0 && defaulted != nullptr) *defaulted = true;
    return true;
}

// The LeavesBlock subclasses (instanceof LeavesBlock): exactly the #minecraft:leaves
// tag members (data/minecraft/tags/block/leaves.json), each registered with a
// LeavesBlock-derived class in Blocks.java. Consumed by the MOTION_BLOCKING_NO_LEAVES
// heightmap predicate (Heightmap.java:151-156) and LeavesBlock.getOptionalDistanceAt.
inline bool isLeavesBlock(const std::string& blockOrState) {
    static const std::set<std::string> leaves = {
        "minecraft:jungle_leaves", "minecraft:oak_leaves", "minecraft:spruce_leaves",
        "minecraft:pale_oak_leaves", "minecraft:dark_oak_leaves", "minecraft:acacia_leaves",
        "minecraft:birch_leaves", "minecraft:azalea_leaves", "minecraft:flowering_azalea_leaves",
        "minecraft:mangrove_leaves", "minecraft:cherry_leaves",
    };
    return leaves.count(blockName(blockOrState)) != 0;
}

// BlockStateBase.isAir (the three AirBlock registrations: air, void_air, cave_air —
// Blocks.java .air()). The worldgen grid stores cave_air distinctly (MonsterRoom
// writes Blocks.CAVE_AIR), so every isAir gate must accept all three.
inline bool isAirBlock(const std::string& blockOrState) {
    const std::string block = blockName(blockOrState);
    return block == "minecraft:air" || block == "minecraft:cave_air" || block == "minecraft:void_air";
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

// BlockStateBase.getFaceOcclusionShape(direction) full-block test, as consumed by
// UnderwaterMagmaFeature.isVisibleFromOutside (UnderwaterMagmaFeature.java:77-81):
// "visible" == faceOcclusionShape == empty || !isShapeFullBlock(faceShape). The
// occlusion shape is empty unless canOcclude (BlockBehaviour.java:512: occlusionShape
// = canOcclude ? owner.getOcclusionShape(state) : empty; :515-517 EMPTY vs FULL by
// isFullBlock; :562-563 per-face slice). Over the worldgen block set:
//   NOT occluding - air/cave_air/void_air                 (no shape)
//                 - water/lava        LiquidBlock.getShape = Shapes.empty()
//                                     (LiquidBlock.java:145-148) -> empty occlusion
//                 - ice               .noOcclusion() (Blocks.java "ice" registration)
//                 - seagrass/tall_seagrass/kelp/kelp_plant/glow_lichen/bubble_column
//                                     non-full plant/column shapes -> no full face
//                 - snow              layered shape (height 2/16): NOT full -> treated
//                                     not-occluding AND flagged via `defaulted` (its
//                                     DOWN face alone is full; never queried by the
//                                     ported features -- surface review if it appears)
//   occluding     - every full opaque cube the pipeline emits (stone family, deepslate,
//                   tuff, bedrock, obsidian, sand/red_sand/sandstone, gravel, dirt /
//                   grass_block/coarse_dirt/podzol, clay, mud, magma_block, snow_block,
//                   packed_ice/blue_ice (no .noOcclusion() in Blocks.java), all ores).
// Unknown blocks default to occluding (full cubes are the worldgen default) and are
// flagged via `defaulted` for review, mirroring blocksMotion's discipline.
inline bool isFaceOccludingFullBlock(const std::string& blockOrState, bool* defaulted = nullptr) {
    static const std::set<std::string> nonOccluding = {
        "minecraft:air", "minecraft:cave_air", "minecraft:void_air",
        "minecraft:water", "minecraft:lava",
        "minecraft:ice",
        "minecraft:seagrass", "minecraft:tall_seagrass",
        "minecraft:kelp", "minecraft:kelp_plant",
        "minecraft:glow_lichen", "minecraft:bubble_column",
    };
    static const std::set<std::string> knownOccluding = {
        "minecraft:stone", "minecraft:granite", "minecraft:diorite", "minecraft:andesite",
        "minecraft:tuff", "minecraft:deepslate", "minecraft:bedrock", "minecraft:obsidian",
        "minecraft:gravel", "minecraft:sand", "minecraft:red_sand", "minecraft:sandstone",
        "minecraft:dirt", "minecraft:grass_block", "minecraft:coarse_dirt", "minecraft:podzol",
        "minecraft:clay", "minecraft:mud", "minecraft:magma_block", "minecraft:snow_block",
        "minecraft:packed_ice", "minecraft:blue_ice",
        "minecraft:coal_ore", "minecraft:copper_ore", "minecraft:iron_ore", "minecraft:gold_ore",
        "minecraft:redstone_ore", "minecraft:lapis_ore", "minecraft:diamond_ore", "minecraft:emerald_ore",
        "minecraft:deepslate_coal_ore", "minecraft:deepslate_copper_ore", "minecraft:deepslate_iron_ore",
        "minecraft:deepslate_gold_ore", "minecraft:deepslate_redstone_ore", "minecraft:deepslate_lapis_ore",
        "minecraft:deepslate_diamond_ore", "minecraft:deepslate_emerald_ore",
    };
    const std::string block = blockName(blockOrState);
    if (block == "minecraft:snow") {
        if (defaulted != nullptr) *defaulted = true;   // see note above
        return false;
    }
    if (nonOccluding.count(block) != 0) return false;
    if (knownOccluding.count(block) == 0 && defaulted != nullptr) *defaulted = true;
    return true;
}

} // namespace mc::block

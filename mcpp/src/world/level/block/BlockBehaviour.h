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
        // SmallDripleafBlock extends DoublePlantBlock (SmallDripleafBlock.java:29):
        // SimpleBlockFeature routes it through DoublePlantBlock.placeAt (BOTH halves).
        "minecraft:small_dripleaf",
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
        // Taiga/plains/swamp/desert vegetal additions, all .noCollission() in
        // Blocks.java (TallGrassBlock fern, DoublePlantBlock tall_grass/large_fern/
        // sunflower, SaplingBlock spruce_sapling, DryVegetationBlock dead_bush /
        // short_dry_grass / tall_dry_grass, SweetBerryBushBlock, FireflyBushBlock,
        // FlowerBlock dandelion/poppy/azure_bluet/oxeye_daisy/cornflower/tulips/
        // allium/blue_orchid, CactusFlowerBlock, SugarCaneBlock, WaterlilyBlock
        // (lily_pad: tiny 1.5/16 plate, legacySolid false)):
        "minecraft:fern", "minecraft:tall_grass", "minecraft:large_fern",
        "minecraft:sunflower", "minecraft:spruce_sapling",
        "minecraft:dead_bush", "minecraft:short_dry_grass", "minecraft:tall_dry_grass",
        "minecraft:sweet_berry_bush", "minecraft:firefly_bush",
        "minecraft:dandelion", "minecraft:poppy", "minecraft:azure_bluet",
        "minecraft:oxeye_daisy", "minecraft:cornflower", "minecraft:allium",
        "minecraft:blue_orchid", "minecraft:red_tulip", "minecraft:orange_tulip",
        "minecraft:white_tulip", "minecraft:pink_tulip",
        "minecraft:cactus_flower", "minecraft:sugar_cane", "minecraft:lily_pad",
        // FlowerBedBlock (wildflowers/pink_petals): 1/16-tall segment plates,
        // legacySolid false.
        "minecraft:wildflowers", "minecraft:pink_petals",
        // Lush-caves vegetal (Blocks.java registrations): small_dripleaf /
        // hanging_roots / cave_vines / cave_vines_plant / big_dripleaf_stem are
        // .noCollision(); azalea / flowering_azalea / big_dripleaf / spore_blossom
        // are .forceSolidOff(); moss_carpet collides but its 1/16 plate fails the
        // calculateSolid bounds test (avg 0.687 < 0.7291).
        "minecraft:small_dripleaf", "minecraft:hanging_roots",
        "minecraft:cave_vines", "minecraft:cave_vines_plant", "minecraft:big_dripleaf_stem",
        "minecraft:azalea", "minecraft:flowering_azalea", "minecraft:big_dripleaf",
        "minecraft:spore_blossom", "minecraft:moss_carpet",
        // AmethystClusterBlock family: bounds (3..13)/16 xz, 7/16 tall -> avg size
        // (0.625+0.4375+0.625)/3 = 0.5625 < 0.7291 and ySize < 1 -> legacySolid FALSE
        // (BlockBehaviour.calculateSolid; AmethystClusterBlock shapes).
        "minecraft:small_amethyst_bud", "minecraft:medium_amethyst_bud",
        "minecraft:large_amethyst_bud", "minecraft:amethyst_cluster",
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
        // New-biome additions (Blocks.java registrations):
        //   spruce_log/spruce_leaves — same log/leaves properties as oak/birch;
        //   mycelium — full SnowyDirtBlock cube; pumpkin/melon full cubes;
        //   cactus — CactusBlock 15/16 collision column: avg size
        //     (0.875+0.9375+0.875)/3 = 0.896 >= 0.7291 -> legacySolid TRUE;
        //   sandstone_slab — bottom slab: avg (1+0.5+1)/3 = 0.833 -> legacySolid TRUE;
        //   suspicious_sand (BrushableBlock full cube);
        //   amethyst_block/budding_amethyst/calcite/smooth_basalt full cubes (geode).
        "minecraft:spruce_log", "minecraft:spruce_leaves", "minecraft:mycelium",
        "minecraft:pumpkin", "minecraft:melon", "minecraft:cactus",
        "minecraft:sandstone_slab", "minecraft:suspicious_sand",
        "minecraft:amethyst_block", "minecraft:budding_amethyst",
        "minecraft:calcite", "minecraft:smooth_basalt",
        // Lush/dripstone caves: full cubes (moss_block, rooted_dirt, dripstone_block,
        // sculk, raw ore blocks); pointed_dripstone is .forceSolidOn(); azalea leaves
        // are the leaves family (full collision cubes).
        "minecraft:moss_block", "minecraft:rooted_dirt", "minecraft:dripstone_block",
        "minecraft:pointed_dripstone", "minecraft:sculk",
        "minecraft:raw_iron_block", "minecraft:raw_copper_block", "minecraft:raw_gold_block",
        "minecraft:azalea_leaves", "minecraft:flowering_azalea_leaves",
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
        // New-biome non-occluding states: spruce_leaves (.noOcclusion()), the
        // no-collision plants (no occlusion shape), cactus/sugar_cane/lily_pad/
        // cactus_flower (non-full shapes), sandstone_slab (non-full), the amethyst
        // buds/cluster (non-full).
        "minecraft:spruce_leaves",
        "minecraft:fern", "minecraft:tall_grass", "minecraft:large_fern",
        "minecraft:sunflower", "minecraft:spruce_sapling",
        "minecraft:dead_bush", "minecraft:short_dry_grass", "minecraft:tall_dry_grass",
        "minecraft:sweet_berry_bush", "minecraft:firefly_bush",
        "minecraft:dandelion", "minecraft:poppy", "minecraft:azure_bluet",
        "minecraft:oxeye_daisy", "minecraft:cornflower", "minecraft:allium",
        "minecraft:blue_orchid", "minecraft:red_tulip", "minecraft:orange_tulip",
        "minecraft:white_tulip", "minecraft:pink_tulip",
        "minecraft:cactus", "minecraft:cactus_flower", "minecraft:sugar_cane",
        "minecraft:lily_pad", "minecraft:sandstone_slab",
        "minecraft:wildflowers", "minecraft:pink_petals",
        "minecraft:small_amethyst_bud", "minecraft:medium_amethyst_bud",
        "minecraft:large_amethyst_bud", "minecraft:amethyst_cluster",
        // Lush/dripstone caves non-occluding states (plants, azalea .noOcclusion(),
        // pointed_dripstone .noOcclusion(), azalea leaves .noOcclusion()).
        "minecraft:small_dripleaf", "minecraft:hanging_roots",
        "minecraft:cave_vines", "minecraft:cave_vines_plant", "minecraft:big_dripleaf_stem",
        "minecraft:azalea", "minecraft:flowering_azalea", "minecraft:big_dripleaf",
        "minecraft:spore_blossom", "minecraft:moss_carpet", "minecraft:pointed_dripstone",
        "minecraft:azalea_leaves", "minecraft:flowering_azalea_leaves",
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
        // New-biome full opaque cubes: spruce_log, mycelium, pumpkin/melon,
        // suspicious_sand, the geode shell blocks.
        "minecraft:spruce_log", "minecraft:mycelium", "minecraft:pumpkin", "minecraft:melon",
        "minecraft:suspicious_sand", "minecraft:amethyst_block", "minecraft:budding_amethyst",
        "minecraft:calcite", "minecraft:smooth_basalt",
        "minecraft:moss_block", "minecraft:rooted_dirt", "minecraft:dripstone_block",
        "minecraft:sculk", "minecraft:raw_iron_block", "minecraft:raw_copper_block",
        "minecraft:raw_gold_block",
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

// BlockStateBase.isFaceSturdy(level, pos, direction) with SupportType.FULL:
// Block.isFaceFull(state.getBlockSupportShape(...), direction). The SUPPORT shape
// is the collision shape EXCEPT for overrides (BlockBehaviour.java:297-299):
//   LeavesBlock.getBlockSupportShape  -> Shapes.empty()  (LeavesBlock.java:55-58)
//     => leaves are NEVER face-sturdy (although they collide as full cubes)
//   SnowLayerBlock.getBlockSupportShape -> SHAPES[layers] (SnowLayerBlock.java:55-58)
//     => a worldgen snow layer (layers=1, 2/16 tall) is sturdy on DOWN only
// Full-cube colliding blocks are sturdy on every face; partial blocks
// (mud 14/16, cactus, bottom slabs, chest) are sturdy only where a face is full:
//   mud / sandstone_slab (bottom): DOWN only; cactus/chest/buds: none.
// Direction indices: 0=DOWN,1=UP,2=NORTH,3=SOUTH,4=WEST,5=EAST.
// Unknown blocks default to sturdy (full cubes are the worldgen default) and are
// flagged via `defaulted` for review.
inline bool isFaceSturdyFull(const std::string& blockOrState, int direction, bool* defaulted = nullptr) {
    const std::string block = blockName(blockOrState);
    if (isLeavesBlock(block)) return false;                    // support shape EMPTY
    static const std::set<std::string> downOnly = {
        "minecraft:snow", "minecraft:mud", "minecraft:sandstone_slab",
        "minecraft:moss_carpet",   // 1/16 carpet: DOWN face full only
    };
    if (downOnly.count(block) != 0) return direction == 0;
    // AzaleaBlock support = collision = full-xz top slab (y 8..16) + stem
    // (AzaleaBlock.java:21): only the UP face is full.
    if (block == "minecraft:azalea" || block == "minecraft:flowering_azalea") return direction == 1;
    static const std::set<std::string> neverSturdy = {
        "minecraft:chest", "minecraft:cactus", "minecraft:lily_pad",
        "minecraft:small_amethyst_bud", "minecraft:medium_amethyst_bud",
        "minecraft:large_amethyst_bud", "minecraft:amethyst_cluster",
        "minecraft:pointed_dripstone",
        "minecraft:small_dripleaf", "minecraft:big_dripleaf", "minecraft:big_dripleaf_stem",
        "minecraft:hanging_roots", "minecraft:spore_blossom",
        "minecraft:cave_vines", "minecraft:cave_vines_plant",
    };
    if (neverSturdy.count(block) != 0) return false;
    // The remaining sets share blocksMotion's discipline: motion-blockers here are
    // full cubes (sturdy on all faces); non-colliding/liquid/air are not sturdy.
    return blocksMotion(blockOrState, defaulted);
}

// Compatibility shim for the UP-face callers (fallen trees, leaf_litter).
inline bool isFaceSturdyUp(const std::string& blockOrState, bool* defaulted = nullptr) {
    return isFaceSturdyFull(blockOrState, 1, defaulted);
}

// Block.isFaceFull(state.getCollisionShape(level, pos), direction) — the
// COLLISION-shape face-full test. Consumers: SnowLayerBlock.canSurvive (UP face
// of the block below, SnowLayerBlock.java:77-86) and MultifaceBlock.canAttachTo's
// second disjunct (MultifaceBlock.java:256-261). This differs from isSolidRender
// (leaves are .noOcclusion() but their COLLISION shape is the full cube — snow
// and vines attach to leaves) and from blocksMotion (a bottom slab blocks motion
// but its top face is at y=0.5; pointed_dripstone is forceSolidOn with a partial
// shape). Per Blocks.java registration over the worldgen set:
//   TRUE  - every full-cube colliding block (stone/deepslate/tuff family, bedrock,
//           obsidian, sands/sandstone, gravel, dirt family incl. grass_block/podzol/
//           coarse_dirt/mycelium, clay, magma_block, snow_block, ice/packed_ice/
//           blue_ice (the cannot_support_snow_layer TAG intercepts ice first),
//           ores, logs, LEAVES (full collision cube), bee_nest, spawner,
//           cobblestone/mossy_cobblestone, pumpkin/melon, suspicious_sand,
//           amethyst_block/budding_amethyst/calcite/smooth_basalt)
//   FALSE - air family, liquids, every no-collision plant, glow_lichen /
//           bubble_column, cactus (15/16 inset), sugar_cane/lily_pad/cactus_flower,
//           chest (14/16 inset), the amethyst buds/cluster, vine.
//   DOWN only - snow layer (2/16 bottom slab), mud (14/16), sandstone_slab
//           (bottom slab), moss_carpet (1/16).
//   UP only - azalea/flowering_azalea (collision top slab spans the full xz at
//           y=1, AzaleaBlock.java:21).
// Unknown blocks default TRUE (worldgen default is full cubes) and are flagged.
// Direction indices: 0=DOWN,1=UP,2=NORTH,3=SOUTH,4=WEST,5=EAST.
inline bool isCollisionFaceFull(const std::string& blockOrState, int direction, bool* defaulted = nullptr) {
    {
        const std::string b = blockName(blockOrState);
        static const std::set<std::string> downOnly = {
            "minecraft:snow", "minecraft:mud", "minecraft:sandstone_slab", "minecraft:moss_carpet",
        };
        if (downOnly.count(b) != 0) return direction == 0;
        if (b == "minecraft:azalea" || b == "minecraft:flowering_azalea") return direction == 1;
    }
    static const std::set<std::string> notFaceFull = {
        "minecraft:air", "minecraft:cave_air", "minecraft:void_air",
        "minecraft:water", "minecraft:lava",
        "minecraft:seagrass", "minecraft:tall_seagrass",
        "minecraft:kelp", "minecraft:kelp_plant",
        "minecraft:glow_lichen", "minecraft:bubble_column",
        "minecraft:chest",
        "minecraft:short_grass", "minecraft:bush", "minecraft:lily_of_the_valley",
        "minecraft:brown_mushroom", "minecraft:red_mushroom", "minecraft:vine",
        "minecraft:lilac", "minecraft:rose_bush", "minecraft:peony",
        "minecraft:leaf_litter", "minecraft:oak_sapling", "minecraft:birch_sapling",
        "minecraft:fern", "minecraft:tall_grass", "minecraft:large_fern",
        "minecraft:sunflower", "minecraft:spruce_sapling",
        "minecraft:dead_bush", "minecraft:short_dry_grass", "minecraft:tall_dry_grass",
        "minecraft:sweet_berry_bush", "minecraft:firefly_bush",
        "minecraft:dandelion", "minecraft:poppy", "minecraft:azure_bluet",
        "minecraft:oxeye_daisy", "minecraft:cornflower", "minecraft:allium",
        "minecraft:blue_orchid", "minecraft:red_tulip", "minecraft:orange_tulip",
        "minecraft:white_tulip", "minecraft:pink_tulip",
        "minecraft:cactus", "minecraft:cactus_flower", "minecraft:sugar_cane",
        "minecraft:lily_pad", "minecraft:sandstone_slab",
        "minecraft:wildflowers", "minecraft:pink_petals",
        "minecraft:small_amethyst_bud", "minecraft:medium_amethyst_bud",
        "minecraft:large_amethyst_bud", "minecraft:amethyst_cluster",
        // Lush/dripstone caves: no-collision plants, the partial big_dripleaf leaf
        // (top at 15/16), pointed dripstone (partial despite forceSolidOn), spore blossom.
        "minecraft:small_dripleaf", "minecraft:hanging_roots",
        "minecraft:cave_vines", "minecraft:cave_vines_plant", "minecraft:big_dripleaf_stem",
        "minecraft:big_dripleaf", "minecraft:spore_blossom",
        "minecraft:pointed_dripstone",
    };
    static const std::set<std::string> knownFaceFull = {
        "minecraft:stone", "minecraft:granite", "minecraft:diorite", "minecraft:andesite",
        "minecraft:tuff", "minecraft:deepslate", "minecraft:bedrock", "minecraft:obsidian",
        "minecraft:gravel", "minecraft:sand", "minecraft:red_sand", "minecraft:sandstone",
        "minecraft:dirt", "minecraft:grass_block", "minecraft:coarse_dirt", "minecraft:podzol",
        "minecraft:mycelium", "minecraft:clay", "minecraft:magma_block", "minecraft:snow_block",
        "minecraft:ice", "minecraft:packed_ice", "minecraft:blue_ice",
        "minecraft:coal_ore", "minecraft:copper_ore", "minecraft:iron_ore", "minecraft:gold_ore",
        "minecraft:redstone_ore", "minecraft:lapis_ore", "minecraft:diamond_ore", "minecraft:emerald_ore",
        "minecraft:deepslate_coal_ore", "minecraft:deepslate_copper_ore", "minecraft:deepslate_iron_ore",
        "minecraft:deepslate_gold_ore", "minecraft:deepslate_redstone_ore", "minecraft:deepslate_lapis_ore",
        "minecraft:deepslate_diamond_ore", "minecraft:deepslate_emerald_ore",
        "minecraft:oak_log", "minecraft:birch_log", "minecraft:spruce_log",
        "minecraft:oak_leaves", "minecraft:birch_leaves", "minecraft:spruce_leaves",
        "minecraft:bee_nest", "minecraft:spawner",
        "minecraft:cobblestone", "minecraft:mossy_cobblestone",
        "minecraft:pumpkin", "minecraft:melon", "minecraft:suspicious_sand",
        "minecraft:amethyst_block", "minecraft:budding_amethyst",
        "minecraft:calcite", "minecraft:smooth_basalt",
        "minecraft:moss_block", "minecraft:rooted_dirt", "minecraft:dripstone_block",
        "minecraft:sculk", "minecraft:raw_iron_block", "minecraft:raw_copper_block",
        "minecraft:raw_gold_block",
        "minecraft:azalea_leaves", "minecraft:flowering_azalea_leaves",
    };
    const std::string block = blockName(blockOrState);
    if (notFaceFull.count(block) != 0) return false;
    if (knownFaceFull.count(block) == 0 && defaulted != nullptr) *defaulted = true;
    return true;
}
// SnowLayerBlock.canSurvive's UP-face test.
inline bool isCollisionFaceFullUp(const std::string& blockOrState, bool* defaulted = nullptr) {
    return isCollisionFaceFull(blockOrState, 1, defaulted);
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

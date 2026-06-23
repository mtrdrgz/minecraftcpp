#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.structure.templatesystem.BlockAgeProcessor
// (BlockAgeProcessor.java) — the "ageing" template processor that erodes ruined-portal /
// ocean-monument style stonework into cracked/mossy variants and stairs/slabs. Operates on
// the global block-state registry (mc::getBlockState / getBlockStateIdWith) + the loaded
// BlockTags, with the per-block random == StructurePlaceSettings.getRandom(pos)
// (RandomSource::create(Mth.getSeed(pos))). Every constant + the RNG draw order are taken
// verbatim from the decompiled source so the ageing is reproducible.
//
// withPropertiesOf is reproduced by copying the shared properties (stairs:
// facing/half/shape/waterlogged; slab: type/waterlogged; wall connection set) from the
// source state onto the mossy replacement — exactly the properties the two blocks share
// (always present on the full stair/slab/wall states fed here).

#include "../../../block/Blocks.h"
#include "../../../block/BlockState.h"
#include "../../../block/BlockTags.h"
#include "../../RandomSource.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace mc::levelgen::structure {

namespace blockage_detail {

// Direction.Plane.HORIZONTAL.getRandomDirection: faces[nextInt(4)] = N,E,S,W.
inline const char* HORIZONTAL_FACINGS[4] = { "north", "east", "south", "west" };
// Half.values() = TOP, BOTTOM.
inline const char* HALF_VALUES[2] = { "top", "bottom" };

inline std::string blockName(uint32_t stateId) {
    const mc::BlockState* s = mc::getBlockState(stateId);
    return (s && s->block) ? s->block->name : std::string();
}

// StairBlock default + FACING=HORIZONTAL.getRandomDirection + HALF=Util.getRandom(Half.values).
inline uint32_t getRandomFacingStairs(mc::levelgen::RandomSource& random, const char* stairBlock) {
    const std::string facing = HORIZONTAL_FACINGS[random.nextInt(4)];
    const std::string half = HALF_VALUES[random.nextInt(2)];
    return mc::getBlockStateIdWith(stairBlock, {{"facing", facing}, {"half", half}});
}

// getRandomBlock(random, blocks) = blocks[random.nextInt(blocks.length)].
inline uint32_t pick(mc::levelgen::RandomSource& random, const std::vector<uint32_t>& blocks) {
    return blocks[static_cast<std::size_t>(random.nextInt(static_cast<int32_t>(blocks.size())))];
}
// getRandomBlock(random, nonMossy, mossy) = nextFloat<mossiness ? pick(mossy) : pick(nonMossy).
inline uint32_t pickMossy(mc::levelgen::RandomSource& random, float mossiness,
                          const std::vector<uint32_t>& nonMossy, const std::vector<uint32_t>& mossy) {
    return random.nextFloat() < mossiness ? pick(random, mossy) : pick(random, nonMossy);
}

}  // namespace blockage_detail

// BlockAgeProcessor.processBlock — returns the aged state id, or `stateId` unchanged.
// `random` MUST be a fresh per-block RandomSource::create(Mth.getSeed(pos)).
inline uint32_t blockAgeProcess(uint32_t stateId, float mossiness,
                                mc::levelgen::RandomSource& random, const mc::block::BlockTags& tags) {
    using namespace blockage_detail;
    const mc::BlockState* src = mc::getBlockState(stateId);
    const std::string name = (src && src->block) ? src->block->name : std::string();
    if (name.empty()) return stateId;
    const std::string id = "minecraft:" + name;

    constexpr uint32_t kInvalid = static_cast<uint32_t>(-1);
    uint32_t newState = kInvalid;

    if (name == "stone_bricks" || name == "stone" || name == "chiseled_stone_bricks") {
        // maybeReplaceFullStoneBlock
        if (random.nextFloat() >= 0.5f) return stateId;
        // Both arrays are built eagerly in Java (RNG consumed for both) before the pick.
        const std::vector<uint32_t> nonMossy = {
            mc::getBlockStateId("minecraft:cracked_stone_bricks"),
            getRandomFacingStairs(random, "stone_brick_stairs"),
        };
        const std::vector<uint32_t> mossy = {
            mc::getBlockStateId("minecraft:mossy_stone_bricks"),
            getRandomFacingStairs(random, "mossy_stone_brick_stairs"),
        };
        newState = pickMossy(random, mossiness, nonMossy, mossy);
    } else if (tags.isInTag(id, "minecraft:stairs")) {
        if (random.nextFloat() >= 0.5f) return stateId;
        const std::vector<uint32_t> nonMossy = {
            mc::getBlockStateId("minecraft:stone_slab"),
            mc::getBlockStateId("minecraft:stone_brick_slab"),
        };
        const std::vector<uint32_t> mossy = {
            mc::getBlockStateIdWith("mossy_stone_brick_stairs",
                                    {{"facing", src->getProperty("facing")},
                                     {"half", src->getProperty("half")},
                                     {"shape", src->getProperty("shape")},
                                     {"waterlogged", src->getProperty("waterlogged")}}),
            mc::getBlockStateId("minecraft:mossy_stone_brick_slab"),
        };
        newState = pickMossy(random, mossiness, nonMossy, mossy);
    } else if (tags.isInTag(id, "minecraft:slabs")) {
        if (random.nextFloat() < mossiness)
            newState = mc::getBlockStateIdWith("mossy_stone_brick_slab",
                                               {{"type", src->getProperty("type")},
                                                {"waterlogged", src->getProperty("waterlogged")}});
    } else if (tags.isInTag(id, "minecraft:walls")) {
        if (random.nextFloat() < mossiness)
            newState = mc::getBlockStateIdWith("mossy_stone_brick_wall",
                                               {{"up", src->getProperty("up")},
                                                {"north", src->getProperty("north")},
                                                {"east", src->getProperty("east")},
                                                {"south", src->getProperty("south")},
                                                {"west", src->getProperty("west")},
                                                {"waterlogged", src->getProperty("waterlogged")}});
    } else if (name == "obsidian") {
        if (random.nextFloat() < 0.15f) newState = mc::getBlockStateId("minecraft:crying_obsidian");
    }

    return newState != kInvalid ? newState : stateId;
}

}  // namespace mc::levelgen::structure

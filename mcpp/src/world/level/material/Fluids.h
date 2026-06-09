#pragma once

// Block-level fluid model for worldgen: BlockState.getFluidState() for the block
// set reachable during overworld decoration, ported 1:1 from the decompiled
// block classes (each entry cites its Java override):
//   water        -> Fluids.WATER.getSource()       LiquidBlock: state LEVEL=0 is the
//                                                  source; WaterFluid.Source.getAmount=8,
//                                                  isSource=true (WaterFluid.java:149-158)
//   lava         -> Fluids.LAVA.getSource()        (LavaFluid.Source analogous)
//   seagrass     -> Fluids.WATER.getSource(false)  SeagrassBlock.java:86-88
//   tall_seagrass-> Fluids.WATER.getSource(false)  TallSeagrassBlock.java:78-81
//   kelp         -> Fluids.WATER.getSource(false)  KelpBlock.java:71-74
//   kelp_plant   -> Fluids.WATER.getSource(false)  KelpPlantBlock.java:34-37
//   bubble_column-> Fluids.WATER.getSource(false)  BubbleColumnBlock.java:73-75
//   anything else-> Fluids.EMPTY
//
// FluidState.isFull() == getAmount() == 8 (FluidState.java:57-59); a source
// water/lava state has amount 8.
//
// LIMITATION (documented, fail-visible rather than fail-wrong): the engine's
// chunk storage in the parity tests keeps block ids without the `waterlogged`
// property, so waterlogged states of OTHER blocks (e.g. glow_lichen placed in
// water) are not represented; none of the ported features place them. Flowing
// water (LEVEL>0) never occurs in worldgen writes (aquifers/carvers/springs all
// place the source state), so the block-level model is exact for this scope.

#include "../block/BlockStates.h"
#include "../block/BlockTags.h"

#include <string>

namespace mc::material {

struct FluidState {
    std::string fluid;    // "minecraft:water", "minecraft:lava" or "" (empty)
    bool source = false;
    int amount = 0;

    bool isEmpty() const { return fluid.empty(); }
    // FluidState.isFull() (FluidState.java:57-59)
    bool isFull() const { return amount == 8; }
    bool isSource() const { return source; }
    // FluidState.is(TagKey<Fluid>): membership of this state's fluid in a fluid
    // tag (resolved over data/minecraft/tags/fluid by a BlockTags instance --
    // the tag-directory format is identical for all registries).
    bool is(const mc::block::BlockTags& fluidTags, const std::string& tag) const {
        return !fluid.empty() && fluidTags.isInTag(fluid, tag);
    }
};

// BlockState.getFluidState() by block id (properties ignored: see LIMITATION).
inline FluidState fluidStateOf(const std::string& blockOrState) {
    const std::string block = mc::block::blockName(blockOrState);
    if (block == "minecraft:water") return { "minecraft:water", true, 8 };
    if (block == "minecraft:lava") return { "minecraft:lava", true, 8 };
    if (block == "minecraft:seagrass" || block == "minecraft:tall_seagrass"
        || block == "minecraft:kelp" || block == "minecraft:kelp_plant"
        || block == "minecraft:bubble_column") {
        return { "minecraft:water", true, 8 };   // Fluids.WATER.getSource(false)
    }
    return {};
}

} // namespace mc::material

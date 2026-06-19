#pragma once

// Port of net.minecraft.world.level.levelgen.GenerationStep.Decoration.
// The ordinal is BOTH the index into a biome's `features` array AND the `step`
// argument to WorldgenRandom.setFeatureSeed(decorationSeed, featureIndex, step),
// so it must match Java exactly.

namespace mc::levelgen::feature {

struct GenerationStep {
    enum Decoration {
        RAW_GENERATION = 0,
        LAKES,
        LOCAL_MODIFICATIONS,
        UNDERGROUND_STRUCTURES,
        SURFACE_STRUCTURES,
        STRONGHOLDS,
        UNDERGROUND_ORES,
        UNDERGROUND_DECORATION,
        FLUID_SPRINGS,
        VEGETAL_DECORATION,
        TOP_LAYER_MODIFICATION,
        COUNT
    };
};

} // namespace mc::levelgen::feature

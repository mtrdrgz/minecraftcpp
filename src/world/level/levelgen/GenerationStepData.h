#pragma once

// Verified descriptor table for net.minecraft.world.level.levelgen.GenerationStep
// (Minecraft 26.1.2).
//
// The Java class declares a SINGLE nested enum in 26.1.2:
//
//   public class GenerationStep {
//      public enum Decoration implements StringRepresentable {
//         RAW_GENERATION("raw_generation"),
//         LAKES("lakes"),
//         LOCAL_MODIFICATIONS("local_modifications"),
//         UNDERGROUND_STRUCTURES("underground_structures"),
//         SURFACE_STRUCTURES("surface_structures"),
//         STRONGHOLDS("strongholds"),
//         UNDERGROUND_ORES("underground_ores"),
//         UNDERGROUND_DECORATION("underground_decoration"),
//         FLUID_SPRINGS("fluid_springs"),
//         VEGETAL_DECORATION("vegetal_decoration"),
//         TOP_LAYER_MODIFICATION("top_layer_modification");
//         ...
//         public String getName()            { return this.name; }
//         public String getSerializedName()  { return this.name; }  // == getName()
//      }
//   }
//
// NOTE: the old `GenerationStep.Carving` enum (AIR / LIQUID) NO LONGER EXISTS in
// 26.1.2 — `BiomeGenerationSettings` now stores a single, un-split
// `HolderSet<ConfiguredWorldCarver<?>> carvers` (see
// 26.1.2/src/.../biome/BiomeGenerationSettings.java line 36). There is no
// `enum Carving` anywhere in 26.1.2/src, so it is intentionally NOT modeled here.
//
// The ordinal of Decoration is BOTH the index into a biome's `features` array AND
// the `step` argument to WorldgenRandom.setFeatureSeed(seed, featureIndex, step),
// so ordinal order is load-bearing and must match Java exactly.
//
// This header DELIBERATELY does NOT re-declare the enum (the engine's enum lives
// in world/level/levelgen/feature/GenerationStep.h). It only adds the per-constant
// name() / getSerializedName() string table that the engine header omits, so the
// parity gate can diff both the ordinals (against the engine enum) and the strings.

#include <cstddef>

namespace mc::levelgen {

struct DecorationStepDesc {
    int ordinal;                  // == enum ordinal / declaration order
    const char* name;             // Java name()  (e.g. "RAW_GENERATION")
    const char* serializedName;   // Java getSerializedName() == getName()
};

// VERBATIM from GenerationStep.Decoration, in declaration (ordinal) order.
inline constexpr DecorationStepDesc kDecorationSteps[] = {
    {0,  "RAW_GENERATION",         "raw_generation"},
    {1,  "LAKES",                  "lakes"},
    {2,  "LOCAL_MODIFICATIONS",    "local_modifications"},
    {3,  "UNDERGROUND_STRUCTURES", "underground_structures"},
    {4,  "SURFACE_STRUCTURES",     "surface_structures"},
    {5,  "STRONGHOLDS",            "strongholds"},
    {6,  "UNDERGROUND_ORES",       "underground_ores"},
    {7,  "UNDERGROUND_DECORATION", "underground_decoration"},
    {8,  "FLUID_SPRINGS",          "fluid_springs"},
    {9,  "VEGETAL_DECORATION",     "vegetal_decoration"},
    {10, "TOP_LAYER_MODIFICATION", "top_layer_modification"},
};

inline constexpr std::size_t kDecorationStepCount =
    sizeof(kDecorationSteps) / sizeof(kDecorationSteps[0]);

}  // namespace mc::levelgen

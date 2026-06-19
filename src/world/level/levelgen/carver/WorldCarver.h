#pragma once

#include "../NoiseGeneratorSettings.h"
#include "../NoiseRouter.h"
#include "../RandomSource.h"
#include "../../chunk/LevelChunk.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace mc::levelgen::carver {

// Applies the vanilla overworld configured carvers:
// minecraft:cave, minecraft:cave_extra_underground, minecraft:canyon.
//
// This is the carver stage after buildSurface and before structures/features.
using TopMaterialGetter = std::function<std::optional<std::uint32_t>(LevelChunk&, int, int, int, bool)>;

// `fluidUpdateMarks`, when non-null, receives the chunk-local postprocess marks
// vanilla records while carving (WorldCarver.java:147-149: carved fluid blocks
// when aquifer.shouldScheduleFluidUpdate(); :155-158: fluid top material), in
// carve order. They feed LevelChunk.postProcessGeneration at FULL status.
void applyOverworldCarvers(
    LevelChunk& chunk,
    std::int64_t worldSeed,
    const NoiseGeneratorSettings& settings,
    const NoiseRouter& router,
    std::shared_ptr<PositionalRandomFactory> aquiferRandom,
    const std::function<int(int, int)>& preliminarySurface,
    const TopMaterialGetter& topMaterial,
    std::vector<mc::BlockPos>* fluidUpdateMarks = nullptr);

} // namespace mc::levelgen::carver

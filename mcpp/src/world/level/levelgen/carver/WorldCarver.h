#pragma once

#include "../NoiseGeneratorSettings.h"
#include "../NoiseRouter.h"
#include "../RandomSource.h"
#include "../../chunk/LevelChunk.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace mc::levelgen::carver {

// Applies the vanilla overworld configured carvers:
// minecraft:cave, minecraft:cave_extra_underground, minecraft:canyon.
//
// This is the carver stage after buildSurface and before structures/features.
using TopMaterialGetter = std::function<std::optional<std::uint32_t>(LevelChunk&, int, int, int, bool)>;

void applyOverworldCarvers(
    LevelChunk& chunk,
    std::int64_t worldSeed,
    const NoiseGeneratorSettings& settings,
    const NoiseRouter& router,
    std::shared_ptr<PositionalRandomFactory> aquiferRandom,
    const std::function<int(int, int)>& preliminarySurface,
    const TopMaterialGetter& topMaterial);

} // namespace mc::levelgen::carver

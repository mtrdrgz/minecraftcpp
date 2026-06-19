#pragma once

#include "Climate.h"
#include "NoiseRouter.h"
#include <string>
#include <vector>

namespace mc::levelgen {

class BiomeSource {
public:
    explicit BiomeSource(const NoiseRouter& router);

    std::string getBiomeAt(int blockX, int blockY, int blockZ) const;
    std::string getNoiseBiome(int quartX, int quartY, int quartZ) const;

    // Mirrors MultiNoiseBiomeSource.collectPossibleBiomes() for the built-in
    // overworld preset: first distinct biome id from the preset parameter list,
    // in encounter order. ChunkGenerator feeds exactly this source set to
    // overworld preset: the first distinct biome id from the preset parameter
    // list, in encounter order. ChunkGenerator feeds exactly this source set to
    // FeatureSorter before decoration.
    const std::vector<std::string>& possibleBiomes() const noexcept { return m_possibleBiomes; }
    static std::vector<std::string> collectOverworldPossibleBiomes();

    bool hasCompleteOverworldPreset() const noexcept { return m_overworldPresetComplete; }
    size_t parameterCount() const noexcept { return m_parameters.values().size(); }

private:
    NoiseRouter m_router;
    Climate::ParameterList<std::string> m_parameters;
    std::vector<std::string> m_possibleBiomes;
    bool m_overworldPresetComplete = false;
};

} // namespace mc::levelgen

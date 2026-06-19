#pragma once

#include "NoiseGeneratorSettings.h"
#include "Noises.h"
#include "RandomSource.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace mc::levelgen {

// Port foundation for net.minecraft.world.level.levelgen.RandomState.
class RandomState {
public:
    RandomState(NoiseGeneratorSettings settings, uint64_t seed);

    std::shared_ptr<NormalNoise> getOrCreateNoise(const std::string& noiseName);
    std::shared_ptr<PositionalRandomFactory> getOrCreateRandomFactory(const std::string& name);

    std::shared_ptr<PositionalRandomFactory> random() const { return m_random; }
    std::shared_ptr<PositionalRandomFactory> aquiferRandom() const { return m_aquiferRandom; }
    std::shared_ptr<PositionalRandomFactory> oreRandom() const { return m_oreRandom; }
    std::shared_ptr<RandomSource> terrainRandom() const;

    const NoiseGeneratorSettings& settings() const { return m_settings; }
    uint64_t seed() const { return m_seed; }

private:
    NoiseGeneratorSettings m_settings;
    uint64_t m_seed = 0;
    std::shared_ptr<PositionalRandomFactory> m_random;
    std::shared_ptr<PositionalRandomFactory> m_aquiferRandom;
    std::shared_ptr<PositionalRandomFactory> m_oreRandom;
    std::unordered_map<std::string, std::shared_ptr<NormalNoise>> m_noiseInstances;
    std::unordered_map<std::string, std::shared_ptr<PositionalRandomFactory>> m_positionalRandoms;
};

} // namespace mc::levelgen

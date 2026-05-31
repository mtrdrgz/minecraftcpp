#include "RandomState.h"

#include <utility>

namespace mc::levelgen {

namespace {
    std::shared_ptr<RandomSource> newRootRandom(const NoiseGeneratorSettings& settings, uint64_t seed) {
        int64_t signedSeed = static_cast<int64_t>(seed);
        return settings.useLegacyRandomSource
            ? std::static_pointer_cast<RandomSource>(std::make_shared<LegacyRandomSource>(signedSeed))
            : std::static_pointer_cast<RandomSource>(std::make_shared<XoroshiroRandomSource>(signedSeed));
    }
}

RandomState::RandomState(NoiseGeneratorSettings settings, uint64_t seed)
    : m_settings(std::move(settings)), m_seed(seed) {
    std::shared_ptr<RandomSource> root = newRootRandom(m_settings, m_seed);
    m_random = root->forkPositional();
    m_aquiferRandom = m_random->fromHashOf("minecraft:aquifer")->forkPositional();
    m_oreRandom = m_random->fromHashOf("minecraft:ore")->forkPositional();
}

std::shared_ptr<NormalNoise> RandomState::getOrCreateNoise(const std::string& noiseName) {
    std::string key = noiseName;
    if (key.starts_with("minecraft:")) {
        key = key.substr(10);
    }

    auto it = m_noiseInstances.find(key);
    if (it != m_noiseInstances.end()) {
        return it->second;
    }

    std::shared_ptr<NormalNoise> instance = Noises::instantiate(*m_random, key);
    m_noiseInstances.emplace(key, instance);
    return instance;
}

std::shared_ptr<PositionalRandomFactory> RandomState::getOrCreateRandomFactory(const std::string& name) {
    std::string id = Noises::identifier(name);
    auto it = m_positionalRandoms.find(id);
    if (it != m_positionalRandoms.end()) {
        return it->second;
    }

    std::shared_ptr<PositionalRandomFactory> factory = m_random->fromHashOf(id)->forkPositional();
    m_positionalRandoms.emplace(id, factory);
    return factory;
}

std::shared_ptr<RandomSource> RandomState::terrainRandom() const {
    if (m_settings.useLegacyRandomSource) {
        return std::make_shared<LegacyRandomSource>(static_cast<int64_t>(m_seed));
    }

    return m_random->fromHashOf("minecraft:terrain");
}

} // namespace mc::levelgen

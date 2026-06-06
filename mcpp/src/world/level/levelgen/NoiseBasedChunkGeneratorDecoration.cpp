#include "NoiseBasedChunkGenerator.h"

namespace mc::levelgen {

std::string NoiseBasedChunkGenerator::getNoiseBiome(int quartX, int quartY, int quartZ) const {
    return m_biomeSource ? m_biomeSource->getNoiseBiome(quartX, quartY, quartZ) : std::string{};
}

void NoiseBasedChunkGenerator::initializeDecorationFeatures(const feature::BiomeFeatures& biomeFeatures) {
    m_decorationSourceBiomes.clear();
    m_decorationFeaturesPerStep.clear();
    m_decorationFeaturesReady = false;

    if (!m_biomeSource) {
        return;
    }

    // Java ChunkGenerator memoizes:
    // FeatureSorter.buildFeaturesPerStep(List.copyOf(biomeSource.possibleBiomes()), ...)
    m_decorationSourceBiomes = m_biomeSource->possibleBiomes();
    m_decorationFeaturesPerStep = feature::FeatureSorter::buildFeaturesPerStep(
        m_decorationSourceBiomes,
        biomeFeatures,
        true);
    m_decorationFeaturesReady = true;
}

} // namespace mc::levelgen

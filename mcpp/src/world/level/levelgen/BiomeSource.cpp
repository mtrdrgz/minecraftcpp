#include "BiomeSource.h"
#include "NoiseBasedChunkGenerator.h"
#include "OverworldBiomeBuilder.h"

#include <algorithm>

namespace mc::levelgen {

namespace {
std::vector<std::string> distinctBiomeIdsInEncounterOrder(const std::vector<Climate::ParameterList<std::string>::Entry>& entries) {
    std::vector<std::string> out;
    for (const auto& entry : entries) {
        const std::string& biome = entry.second;
        if (std::find(out.begin(), out.end(), biome) == out.end()) {
            out.push_back(biome);
        }
    }
    return out;
}
} // namespace

BiomeSource::BiomeSource(const NoiseRouter& router) : m_router(router) {
    m_parameters = Climate::ParameterList<std::string>(buildOverworldBiomePreset());
    m_possibleBiomes = distinctBiomeIdsInEncounterOrder(m_parameters.values());
    m_overworldPresetComplete = true;
}

std::vector<std::string> BiomeSource::collectOverworldPossibleBiomes() {
    return distinctBiomeIdsInEncounterOrder(buildOverworldBiomePreset());
}

std::string BiomeSource::getBiomeAt(int blockX, int blockY, int blockZ) const {
    return getNoiseBiome(blockX >> 2, blockY >> 2, blockZ >> 2);
}

std::string BiomeSource::getNoiseBiome(int quartX, int quartY, int quartZ) const {
    if (!m_router.temperature || !m_router.vegetation || !m_router.continents ||
        !m_router.erosion || !m_router.depth || !m_router.ridges || m_parameters.empty()) {
        return "";
    }

    const int sampleX = quartX << 2;
    const int sampleY = quartY << 2;
    const int sampleZ = quartZ << 2;
    DensityFunctionContext context{ sampleX, sampleY, sampleZ };
    const Climate::TargetPoint target = Climate::target(
        static_cast<float>(m_router.temperature->compute(context)),
        static_cast<float>(m_router.vegetation->compute(context)),
        static_cast<float>(m_router.continents->compute(context)),
        static_cast<float>(m_router.erosion->compute(context)),
        static_cast<float>(m_router.depth->compute(context)),
        static_cast<float>(m_router.ridges->compute(context)));

    return m_parameters.findValue(target);
}

std::string NoiseBasedChunkGenerator::getNoiseBiome(int quartX, int quartY, int quartZ) const {
    return m_biomeSource ? m_biomeSource->getNoiseBiome(quartX, quartY, quartZ) : std::string{};
}

void NoiseBasedChunkGenerator::initializeDecorationFeatures(const feature::BiomeFeatures& biomeFeatures) {
    m_decorationSourceBiomes.clear();
    m_decorationFeaturesPerStep.clear();
    m_decorationFeaturesReady = false;
    if (!m_biomeSource) return;
    m_decorationSourceBiomes = m_biomeSource->possibleBiomes();
    m_decorationFeaturesPerStep = feature::FeatureSorter::buildFeaturesPerStep(m_decorationSourceBiomes, biomeFeatures, true);
    m_decorationFeaturesReady = true;
}

} // namespace mc::levelgen

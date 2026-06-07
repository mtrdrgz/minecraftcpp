#pragma once

#include "../chunk/LevelChunk.h"
#include "BiomeManager.h"
#include "BiomeSource.h"
#include "NoiseGeneratorSettings.h"
#include "NoiseRouter.h"
#include "RandomSource.h"
#include "SurfaceRules.h"
#include "feature/BiomeFeatures.h"
#include "feature/FeatureSorter.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mc::levelgen {

// Port of net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator.
// fillFromNoise follows Java's cell interpolation order with the full density router.
// buildSurface delegates to the partial SurfaceSystem port with Java Climate /
// MultiNoiseBiomeSource-shaped biome lookup for overworld terrain.
class NoiseBasedChunkGenerator {
public:
    explicit NoiseBasedChunkGenerator(uint64_t seed);
    NoiseBasedChunkGenerator(NoiseGeneratorSettings settings, uint64_t seed);
    ~NoiseBasedChunkGenerator();


    int getBaseHeight(int blockX, int blockZ) const;
    void fillFromNoise(LevelChunk& chunk) const;
    void buildSurface(LevelChunk& chunk) const;
    // Surface build with an explicit biome getter, overriding the BiomeManager
    // zoomer. Used to certify the per-biome surface rules in isolation (force one
    // biome over real terrain and compare against vanilla). Same path as the
    // default overload otherwise.
    void buildSurface(LevelChunk& chunk,
                      const std::function<std::string(int, int, int)>& biomeOverride) const;
    void applyCarvers(LevelChunk& chunk) const;

    // Block-resolution biome at a world position (BiomeManager zoomer), as used by
    // gameplay biome queries.
    std::string getBiome(int blockX, int blockY, int blockZ) const;

    // Quart-resolution noise biome as stored in Java LevelChunkSection biome
    // containers. Biome decoration uses these, not the block-resolution zoomer.
    std::string getNoiseBiome(int quartX, int quartY, int quartZ) const;

    // Java ChunkGenerator memoizes FeatureSorter.buildFeaturesPerStep over
    // List.copyOf(biomeSource.possibleBiomes()). The C++ generator stores the
    // same source biome set and sorted per-step feature data once worldgen JSON
    // has been loaded by the engine.
    void initializeDecorationFeatures(const feature::BiomeFeatures& biomeFeatures);
    bool hasDecorationFeatures() const noexcept { return m_decorationFeaturesReady; }
    const std::vector<std::string>& decorationSourceBiomes() const noexcept { return m_decorationSourceBiomes; }
    const std::vector<feature::FeatureSorter::StepFeatureData>& decorationFeaturesPerStep() const noexcept { return m_decorationFeaturesPerStep; }

    int getSeaLevel()  const { return m_settings.seaLevel; }
    int getMinY()      const { return m_settings.noiseSettings.minY; }
    int getGenDepth()  const { return m_settings.noiseSettings.height; }

private:
    uint32_t stateIdFor(const char* blockName, uint32_t fallback = 0) const;
    double   sampleFinalDensity(int blockX, int blockY, int blockZ) const;
    int      samplePreliminarySurfaceLevel(int blockX, int blockZ) const;

    NoiseGeneratorSettings m_settings;
    uint64_t               m_seed = 0;
    NoiseRouter            m_router;
    std::shared_ptr<PositionalRandomFactory> m_aquiferRandom;
    std::shared_ptr<PositionalRandomFactory> m_oreRandom;

    SurfaceRules::RuleSourcePtr m_surfaceRuleSource;
    std::unique_ptr<BiomeSource> m_biomeSource;
    std::unique_ptr<BiomeManager> m_biomeManager;
    std::vector<std::string> m_decorationSourceBiomes;
    std::vector<feature::FeatureSorter::StepFeatureData> m_decorationFeaturesPerStep;
    bool m_decorationFeaturesReady = false;
};

} // namespace mc::levelgen

#pragma once

#include "../chunk/LevelChunk.h"
#include "BiomeManager.h"
#include "BiomeSource.h"
#include "NoiseGeneratorSettings.h"
#include "NoiseRouter.h"
#include "RandomSource.h"
#include "SurfaceRules.h"
#include <cstdint>
#include <memory>

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
    void applyCarvers(LevelChunk& chunk) const;

    // Block-resolution biome at a world position (BiomeManager zoomer), as used by
    // the decoration step (applyBiomeDecoration) and gameplay biome queries.
    std::string getBiome(int blockX, int blockY, int blockZ) const;

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
};

} // namespace mc::levelgen

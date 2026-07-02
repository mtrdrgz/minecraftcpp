#pragma once

#include "../chunk/LevelChunk.h"
#include "Beardifier.h"
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
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc::levelgen {

class RandomState;
class SurfaceSystem;

// Port of net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator.
// fillFromNoise follows Java's cell interpolation order with the full density router.
// buildSurface delegates to the partial SurfaceSystem port with Java Climate /
// MultiNoiseBiomeSource-shaped biome lookup for overworld terrain.
class NoiseBasedChunkGenerator {
public:
    explicit NoiseBasedChunkGenerator(uint64_t seed);
    NoiseBasedChunkGenerator(NoiseGeneratorSettings settings, uint64_t seed);
    ~NoiseBasedChunkGenerator();


    // 1:1 port of NoiseBasedChunkGenerator.getBaseHeight(x, z, WORLD_SURFACE_WG, ...):
    // iterateNoiseColumn over a single-cell NoiseChunk (cell-interpolated density +
    // aquifer + ore veins), testing Heightmap.Types.WORLD_SURFACE_WG.isOpaque()
    // (= !state.isAir(); fluids COUNT — over oceans this returns sea level, not the
    // ocean floor). Returns firstFree (the y ABOVE the topmost opaque block), i.e.
    // Java's getFirstFreeHeight; subtract 1 for getFirstOccupiedHeight.
    int getBaseHeight(int blockX, int blockZ) const;
    // `fluidUpdateMarks`, when non-null, collects the postprocess marks vanilla
    // records during the NOISE step (NoiseBasedChunkGenerator.java:442-446: every
    // placed state with a non-empty fluid where aquifer.shouldScheduleFluidUpdate()).
    // `beardifier`, when non-null and non-empty, is added per block to the final
    // density (NoiseChunk: add(finalDensity, BeardifierMarker)), adapting terrain to
    // nearby structure pieces (villages: beard_thin). nullptr leaves the density
    // bit-identical, so the certified no-structure terrain parity is preserved.
    void fillFromNoise(LevelChunk& chunk, std::vector<mc::BlockPos>* fluidUpdateMarks = nullptr,
                       const Beardifier* beardifier = nullptr) const;
    void buildSurface(LevelChunk& chunk) const;
    // Surface build with an explicit biome getter, overriding the BiomeManager
    // zoomer. Used to certify the per-biome surface rules in isolation (force one
    // biome over real terrain and compare against vanilla). Same path as the
    // default overload otherwise.
    void buildSurface(LevelChunk& chunk,
                      const std::function<std::string(int, int, int)>& biomeOverride) const;
    // `fluidUpdateMarks`, when non-null, collects the postprocess marks the Java
    // carvers record on the carved chunk (WorldCarver.java:147-149 carved fluid
    // blocks under aquifer.shouldScheduleFluidUpdate, :155-158 fluid top material).
    // The FULL-status postprocess pass (bubble columns) consumes them.
    void applyCarvers(LevelChunk& chunk, std::vector<mc::BlockPos>* fluidUpdateMarks = nullptr) const;

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

    // 1:1 port of getBaseHeight(x, z, OCEAN_FLOOR_WG, ...): same iterateNoiseColumn,
    // testing MATERIAL_MOTION_BLOCKING (= state.blocksMotion(); fluids do NOT count —
    // over oceans this returns the ocean floor). Used by ocean structures
    // (buried_treasure, shipwreck, ocean_ruin).
    int getOceanFloorHeight(int blockX, int blockZ) const;

    // 1:1 port of NoiseBasedChunkGenerator.getBaseColumn: the full noise-only block
    // column (defaultBlock / water / lava / ore / air) from minY (index 0) upward.
    // Ruined portals scan it to pick their buried Y.
    std::vector<uint32_t> getBaseColumn(int blockX, int blockZ) const;

private:
    // 1:1 port of NoiseBasedChunkGenerator.iterateNoiseColumn: walks one block
    // column of the noise-only world top-down at cell resolution (a 1-cell-wide
    // NoiseChunk: interpolated density from the 8 cell corners, the real aquifer
    // anchored at the CONTAINING CHUNK's origin, ore veins), returning y+1 of the
    // first state matching `tester`, and/or filling `column` (index = y - minY).
    std::optional<int> iterateNoiseColumn(int blockX, int blockZ,
                                          const std::function<bool(uint32_t)>* tester,
                                          std::vector<uint32_t>* column) const;

    // Both WG heightmaps for a column in ONE iterateNoiseColumn pass, memoized:
    // the heightmaps are pure functions of (x, z) and structure work (beardifier
    // assembly, TERRAIN_MATCHING placement) queries thousands of columns.
    // Values are the Java getBaseHeight (first-free) for each predicate.
    std::pair<int, int> columnHeights(int blockX, int blockZ) const;
    mutable std::unordered_map<int64_t, std::pair<int, int>> m_heightCache;
    mutable std::mutex m_heightCacheMutex;
    uint32_t stateIdFor(const char* blockName, uint32_t fallback = 0) const;
    double   sampleFinalDensity(int blockX, int blockY, int blockZ) const;
    int      samplePreliminarySurfaceLevel(int blockX, int blockZ) const;

    NoiseGeneratorSettings m_settings;
    uint64_t               m_seed = 0;
    NoiseRouter            m_router;
    std::shared_ptr<PositionalRandomFactory> m_aquiferRandom;
    std::shared_ptr<PositionalRandomFactory> m_oreRandom;

    SurfaceRules::RuleSourcePtr m_surfaceRuleSource;
    mutable std::unique_ptr<RandomState> m_surfaceRandomState;
    mutable std::unique_ptr<SurfaceSystem> m_surfaceSystem;
    std::unique_ptr<BiomeSource> m_biomeSource;
    std::unique_ptr<BiomeManager> m_biomeManager;
    std::vector<std::string> m_decorationSourceBiomes;
    std::vector<feature::FeatureSorter::StepFeatureData> m_decorationFeaturesPerStep;
    bool m_decorationFeaturesReady = false;
};

} // namespace mc::levelgen

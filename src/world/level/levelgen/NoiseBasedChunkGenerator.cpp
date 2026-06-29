#include "NoiseBasedChunkGenerator.h"
#include "Aquifer.h"
#include "NoiseRouterData.h"
#include "OreVeinifier.h"
#include "RandomState.h"
#include "SurfaceSystem.h"
#include "SurfaceRuleData.h"
#include "carver/WorldCarver.h"
#include "../block/Blocks.h"
#include "../block/BlockState.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mc::levelgen {

namespace {
    int floorDiv(int x, int y) {
        int q = x / y;
        int r = x % y;
        if (r != 0 && ((r < 0) != (y < 0))) {
            --q;
        }
        return q;
    }

    double lerp(double alpha, double p0, double p1) {
        return p0 + alpha * (p1 - p0);
    }

    double lerp2(double alpha1, double alpha2, double x00, double x10, double x01, double x11) {
        return lerp(alpha2, lerp(alpha1, x00, x10), lerp(alpha1, x01, x11));
    }

    double lerp3(
        double alpha1,
        double alpha2,
        double alpha3,
        double x000,
        double x100,
        double x010,
        double x110,
        double x001,
        double x101,
        double x011,
        double x111
    ) {
        return lerp(alpha3, lerp2(alpha1, alpha2, x000, x100, x010, x110), lerp2(alpha1, alpha2, x001, x101, x011, x111));
    }

    struct CornerEntry {
        const DensityFunction* func;
        std::array<double, 8> values;
    };

    struct CellCacheEntry {
        const DensityFunction* func;
        double values[128];
        bool initialized[128];
    };

    struct Cache2DEntry {
        const DensityFunction* func;
        int64_t keys[16];
        double values[16];
        int count = 0;
    };

    struct FlatCacheEntry {
        const DensityFunction* func;
        int64_t key;
        double value;
        bool hasValue = false;
    };

    struct SharedCacheKey {
        const DensityFunction* func = nullptr;
        int64_t key = 0;

        bool operator==(const SharedCacheKey& other) const {
            return func == other.func && key == other.key;
        }
    };

    struct SharedCacheKeyHash {
        size_t operator()(const SharedCacheKey& key) const noexcept {
            const auto ptr = reinterpret_cast<uintptr_t>(key.func);
            return std::hash<uintptr_t>{}(ptr) ^ (std::hash<int64_t>{}(key.key) + 0x9e3779b97f4a7c15ULL + (ptr << 6) + (ptr >> 2));
        }
    };

    struct BiomeCacheKey {
        int x = 0;
        int y = 0;
        int z = 0;

        bool operator==(const BiomeCacheKey& other) const noexcept {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct BiomeCacheKeyHash {
        size_t operator()(const BiomeCacheKey& key) const noexcept {
            size_t h = std::hash<int>{}(key.x);
            h ^= std::hash<int>{}(key.y) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(key.z) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            return h;
        }
    };

    struct NoiseChunkSharedCache {
        std::unordered_map<SharedCacheKey, double, SharedCacheKeyHash> cache2d;
        std::unordered_map<SharedCacheKey, double, SharedCacheKeyHash> flatCache;
    };

    class CellInterpolationResolver final : public DensityFunctionInterpolationResolver {
    public:
        CellInterpolationResolver(int x0, int y0, int z0, int cellWidth, int cellHeight, NoiseChunkSharedCache* sharedCache)
            : m_x0(x0), m_y0(y0), m_z0(z0), m_cellWidth(cellWidth), m_cellHeight(cellHeight), m_sharedCache(sharedCache) {
        }

        double computeInterpolated(const DensityFunctionPtr& function, const DensityFunctionContext& context) const override {
            const auto* funcPtr = function.get();
            const std::array<double, 8>* valsPtr = nullptr;
            for (int i = 0; i < m_cornerCount; ++i) {
                if (m_cornerValues[i].func == funcPtr) {
                    valsPtr = &m_cornerValues[i].values;
                    break;
                }
            }

            if (!valsPtr) {
                // Java NoiseChunk.fillSlice: during corner computation, inner
                // NoiseInterpolator.compute returns this.value (stale = 0.0 for
                // first cell). We replicate this by passing a "corner resolver"
                // that returns 0.0 for computeInterpolated but delegates to
                // direct computation for cache markers (CacheAllInCell, CacheOnce,
                // Cache2D, FlatCache) — those compute normally in Java during
                // fillSlice because they're not NoiseInterpolators.
                struct CornerResolver : public DensityFunctionInterpolationResolver {
                    explicit CornerResolver(const CellInterpolationResolver& parentIn) : parent(parentIn) {}

                    double computeInterpolated(const DensityFunctionPtr&, const DensityFunctionContext&) const override {
                        return 0.0;  // Java: NoiseInterpolator.value (stale)
                    }
                    double computeCacheOnce(const DensityFunctionPtr& f, const DensityFunctionContext& ctx) const override {
                        return parent.computeCacheOnce(f, ctx);
                    }
                    double computeCacheAllInCell(const DensityFunctionPtr& f, const DensityFunctionContext& ctx) const override {
                        return parent.computeCacheAllInCell(f, ctx);
                    }
                    double computeCache2D(const DensityFunctionPtr& f, const DensityFunctionContext& ctx) const override {
                        return parent.computeCache2D(f, ctx);
                    }
                    double computeFlatCache(const DensityFunctionPtr& f, const DensityFunctionContext& ctx) const override {
                        return parent.computeFlatCache(f, ctx);
                    }
                    const CellInterpolationResolver& parent;
                } cornerResolver(*this);

                std::array<double, 8> values{
                    function->compute(DensityFunctionContext{ m_x0, m_y0, m_z0, &cornerResolver }),
                    function->compute(DensityFunctionContext{ m_x0 + m_cellWidth, m_y0, m_z0, &cornerResolver }),
                    function->compute(DensityFunctionContext{ m_x0, m_y0 + m_cellHeight, m_z0, &cornerResolver }),
                    function->compute(DensityFunctionContext{ m_x0 + m_cellWidth, m_y0 + m_cellHeight, m_z0, &cornerResolver }),
                    function->compute(DensityFunctionContext{ m_x0, m_y0, m_z0 + m_cellWidth, &cornerResolver }),
                    function->compute(DensityFunctionContext{ m_x0 + m_cellWidth, m_y0, m_z0 + m_cellWidth, &cornerResolver }),
                    function->compute(DensityFunctionContext{ m_x0, m_y0 + m_cellHeight, m_z0 + m_cellWidth, &cornerResolver }),
                    function->compute(DensityFunctionContext{ m_x0 + m_cellWidth, m_y0 + m_cellHeight, m_z0 + m_cellWidth, &cornerResolver })
                };
                if (m_cornerCount < 64) {
                    m_cornerValues[m_cornerCount++] = { funcPtr, values };
                    valsPtr = &m_cornerValues[m_cornerCount - 1].values;
                } else {
                    const double factorX = (double)(context.blockX - m_x0) / (double)m_cellWidth;
                    const double factorY = (double)(context.blockY - m_y0) / (double)m_cellHeight;
                    const double factorZ = (double)(context.blockZ - m_z0) / (double)m_cellWidth;
                    return lerp3(factorX, factorY, factorZ, values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7]);
                }
            }

            const double factorX = (double)(context.blockX - m_x0) / (double)m_cellWidth;
            const double factorY = (double)(context.blockY - m_y0) / (double)m_cellHeight;
            const double factorZ = (double)(context.blockZ - m_z0) / (double)m_cellWidth;
            const auto& v = *valsPtr;
            // Java NoiseInterpolator uses Y→X→Z lerp order (updateForY → updateForX → updateForZ).
            // C++ lerp3 uses X→Y→Z. These are mathematically equivalent but FP-different.
            // Match Java's order: lerp(factorY, v[0], v[2]) → lerp(factorX, ...) → lerp(factorZ, ...)
            // v[0]=n000 v[1]=n100 v[2]=n010 v[3]=n110 v[4]=n001 v[5]=n101 v[6]=n011 v[7]=n111
            const double yz00 = lerp(factorY, v[0], v[2]);   // lerp(n000, n010)
            const double yz10 = lerp(factorY, v[1], v[3]);   // lerp(n100, n110)
            const double yz01 = lerp(factorY, v[4], v[6]);   // lerp(n001, n011)
            const double yz11 = lerp(factorY, v[5], v[7]);   // lerp(n101, n111)
            const double z0 = lerp(factorX, yz00, yz10);
            const double z1 = lerp(factorX, yz01, yz11);
            return lerp(factorZ, z0, z1);
        }

        double computeCacheOnce(const DensityFunctionPtr& function, const DensityFunctionContext& context) const override {
            return computeCellCached(m_cacheOnceEntries, m_cacheOnceCount, function, context);
        }

        double computeCacheAllInCell(const DensityFunctionPtr& function, const DensityFunctionContext& context) const override {
            return computeCellCached(m_cacheAllInCellEntries, m_cacheAllInCellCount, function, context);
        }

        double computeCache2D(const DensityFunctionPtr& function, const DensityFunctionContext& context) const override {
            const DensityFunction* funcPtr = function.get();
            const int64_t key = packXZ(context.blockX, context.blockZ);
            if (m_sharedCache) {
                SharedCacheKey sharedKey{ funcPtr, key };
                auto it = m_sharedCache->cache2d.find(sharedKey);
                if (it != m_sharedCache->cache2d.end()) return it->second;
                double value = function->compute(context);
                m_sharedCache->cache2d.emplace(sharedKey, value);
                return value;
            }
            Cache2DEntry* entry = nullptr;
            for (int i = 0; i < m_cache2DCount; ++i) {
                if (m_cache2DEntries[i].func == funcPtr) {
                    entry = &m_cache2DEntries[i];
                    break;
                }
            }
            if (!entry) {
                if (m_cache2DCount < 32) {
                    entry = &m_cache2DEntries[m_cache2DCount++];
                    entry->func = funcPtr;
                    entry->count = 0;
                } else {
                    return function->compute(context);
                }
            }
            for (int i = 0; i < entry->count; ++i) {
                if (entry->keys[i] == key) return entry->values[i];
            }
            double value = function->compute(context);
            if (entry->count < 16) {
                entry->keys[entry->count] = key;
                entry->values[entry->count] = value;
                entry->count++;
            }
            return value;
        }

        double computeFlatCache(const DensityFunctionPtr& function, const DensityFunctionContext& context) const override {
            const DensityFunction* funcPtr = function.get();
            const int quantizedX = floorDiv(context.blockX, 4) * 4;
            const int quantizedZ = floorDiv(context.blockZ, 4) * 4;
            const int64_t key = packXZ(quantizedX, quantizedZ);
            if (m_sharedCache) {
                SharedCacheKey sharedKey{ funcPtr, key };
                auto it = m_sharedCache->flatCache.find(sharedKey);
                if (it != m_sharedCache->flatCache.end()) return it->second;
                double value = function->compute(DensityFunctionContext{ quantizedX, 0, quantizedZ });
                m_sharedCache->flatCache.emplace(sharedKey, value);
                return value;
            }
            
            FlatCacheEntry* entry = nullptr;
            for (int i = 0; i < m_flatCacheCount; ++i) {
                if (m_flatCacheEntries[i].func == funcPtr) {
                    entry = &m_flatCacheEntries[i];
                    break;
                }
            }
            if (!entry) {
                if (m_flatCacheCount < 32) {
                    entry = &m_flatCacheEntries[m_flatCacheCount++];
                    entry->func = funcPtr;
                    entry->hasValue = false;
                } else {
                    return function->compute(DensityFunctionContext{ quantizedX, 0, quantizedZ });
                }
            }
            if (entry->hasValue && entry->key == key) {
                return entry->value;
            }
            double value = function->compute(DensityFunctionContext{ quantizedX, 0, quantizedZ });
            entry->key = key;
            entry->value = value;
            entry->hasValue = true;
            return value;
        }

    private:
        int m_x0 = 0;
        int m_y0 = 0;
        int m_z0 = 0;
        int m_cellWidth = 4;
        int m_cellHeight = 8;

        mutable CornerEntry m_cornerValues[64];
        mutable int m_cornerCount = 0;

        mutable CellCacheEntry m_cacheOnceEntries[32];
        mutable int m_cacheOnceCount = 0;

        mutable CellCacheEntry m_cacheAllInCellEntries[32];
        mutable int m_cacheAllInCellCount = 0;

        mutable Cache2DEntry m_cache2DEntries[32];
        mutable int m_cache2DCount = 0;

        mutable FlatCacheEntry m_flatCacheEntries[32];
        mutable int m_flatCacheCount = 0;
        NoiseChunkSharedCache* m_sharedCache = nullptr;

        int64_t localCellIndex(const DensityFunctionContext& context) const {
            const int x = context.blockX - m_x0;
            const int y = context.blockY - m_y0;
            const int z = context.blockZ - m_z0;
            if (x < 0 || y < 0 || z < 0 || x >= m_cellWidth || y >= m_cellHeight || z >= m_cellWidth) {
                return -1;
            }
            return ((m_cellHeight - 1 - y) * m_cellWidth + x) * m_cellWidth + z;
        }

        static int64_t packXZ(int x, int z) {
            return (static_cast<int64_t>(static_cast<uint32_t>(x)) << 32) |
                   static_cast<uint32_t>(z);
        }

        double computeCellCached(CellCacheEntry* entries, int& count, const DensityFunctionPtr& function, const DensityFunctionContext& context) const {
            const int64_t index = localCellIndex(context);
            if (index < 0) {
                return function->compute(context);
            }
            
            const DensityFunction* funcPtr = function.get();
            CellCacheEntry* entry = nullptr;
            for (int i = 0; i < count; ++i) {
                if (entries[i].func == funcPtr) {
                    entry = &entries[i];
                    break;
                }
            }
            if (!entry) {
                if (count < 32) {
                    entry = &entries[count++];
                    entry->func = funcPtr;
                    std::memset(entry->initialized, 0, sizeof(entry->initialized));
                } else {
                    return function->compute(context);
                }
            }
            if (entry->initialized[index]) {
                return entry->values[index];
            }
            double value = function->compute(context);
            entry->values[index] = value;
            entry->initialized[index] = true;
            return value;
        }
    };


    NoiseRouter makeRouter(const NoiseGeneratorSettings& settings, RandomState& randomState) {
        uint32_t netherrack = getDefaultBlockStateId("netherrack", UINT32_MAX);
        uint32_t endStone = getDefaultBlockStateId("end_stone", UINT32_MAX);

        if (settings.defaultBlock == netherrack) {
            return NoiseRouterData::nether(randomState);
        }
        if (settings.defaultBlock == endStone) {
            return NoiseRouterData::end(randomState);
        }
        // Java: NoiseRouterData.overworld(..., largeBiomes, isAmplified). The
        // largeBiomes flag stretches biome climate parameters; the amplified
        // flag steepens the terrain shaper. Both are now propagated from the
        // NoiseGeneratorSettings preset (overworld/largeBiomes/amplified).
        return NoiseRouterData::overworld(randomState, settings.largeBiomesFlag, settings.amplifiedFlag);
    }

}

NoiseBasedChunkGenerator::NoiseBasedChunkGenerator(uint64_t seed)
    : NoiseBasedChunkGenerator(NoiseGeneratorSettings::overworld(), seed) {
}

NoiseBasedChunkGenerator::~NoiseBasedChunkGenerator() = default;


NoiseBasedChunkGenerator::NoiseBasedChunkGenerator(NoiseGeneratorSettings settings, uint64_t seed)
    : m_settings(settings)
    , m_seed(seed) {
    RandomState randomState(m_settings, seed);
    m_aquiferRandom = randomState.aquiferRandom();
    m_oreRandom     = randomState.oreRandom();
    m_router        = makeRouter(m_settings, randomState);
    m_router.finalDensity = DensityFunctions::cacheAllInCell(m_router.finalDensity);

    // Pick the appropriate surface rule + biome source for this dimension.
    // Java: ChunkGenerator overworld uses MultiNoiseBiomeSource(overworld preset),
    // nether uses MultiNoiseBiomeSource(nether preset), end uses TheEndBiomeSource.
    // The router is already dimension-correct (makeRouter above dispatches on
    // defaultBlock == netherrack/end_stone).
    uint32_t netherrack = getDefaultBlockStateId("netherrack", UINT32_MAX);
    uint32_t endStone   = getDefaultBlockStateId("end_stone",  UINT32_MAX);
    if (m_settings.defaultBlock == netherrack) {
        m_surfaceRuleSource = SurfaceRuleData::nether();
        // Nether: 5-biome MultiNoiseBiomeSource (nether_wastes, soul_sand_valley,
        // crimson_forest, warped_forest, basalt_deltas).
        m_biomeSource = std::make_unique<BiomeSource>(BiomeSource::createNether(m_router));
        m_biomeManager = std::make_unique<BiomeManager>(
            *m_biomeSource,
            BiomeManager::obfuscateSeed(static_cast<int64_t>(m_seed)));
    } else if (m_settings.defaultBlock == endStone) {
        m_surfaceRuleSource = SurfaceRuleData::end();
        // End: TheEndBiomeSource — erosion-based, NOT climate. 5 biomes
        // (the_end, end_highlands, end_midlands, small_end_islands, end_barrens).
        m_biomeSource = std::make_unique<BiomeSource>(BiomeSource::createEnd(m_router));
        m_biomeManager = std::make_unique<BiomeManager>(
            *m_biomeSource,
            BiomeManager::obfuscateSeed(static_cast<int64_t>(m_seed)));
    } else {
        m_surfaceRuleSource = SurfaceRuleData::overworld();
        m_biomeSource = std::make_unique<BiomeSource>(m_router);
        m_biomeManager = std::make_unique<BiomeManager>(
            *m_biomeSource,
            BiomeManager::obfuscateSeed(static_cast<int64_t>(m_seed)));
    }

    m_surfaceRandomState = std::make_unique<RandomState>(m_settings, m_seed);
    m_surfaceSystem = std::make_unique<SurfaceSystem>(
        *m_surfaceRandomState,
        m_surfaceRandomState->random(),
        m_settings.defaultBlock,
        m_settings.seaLevel);
}

uint32_t NoiseBasedChunkGenerator::stateIdFor(const char* blockName, uint32_t fallback) const {
    return getDefaultBlockStateId(blockName, fallback);
}

std::string NoiseBasedChunkGenerator::getBiome(int blockX, int blockY, int blockZ) const {
    return m_biomeManager ? m_biomeManager->getBiome(blockX, blockY, blockZ) : "";
}

std::string NoiseBasedChunkGenerator::getNoiseBiome(int quartX, int quartY, int quartZ) const {
    // Quart-resolution noise biome (as stored in LevelChunkSection biome containers);
    // this is what applyBiomeDecoration uses to collect possibleBiomes — NOT the
    // block-resolution BiomeManager zoomer.
    return m_biomeSource ? m_biomeSource->getNoiseBiome(quartX, quartY, quartZ) : "";
}

double NoiseBasedChunkGenerator::sampleFinalDensity(int blockX, int blockY, int blockZ) const {
    return m_router.finalDensity->compute(DensityFunctionContext{ blockX, blockY, blockZ });
}

int NoiseBasedChunkGenerator::samplePreliminarySurfaceLevel(int blockX, int blockZ) const {
    const int quantizedX = floorDiv(blockX, 4) * 4;
    const int quantizedZ = floorDiv(blockZ, 4) * 4;
    return static_cast<int>(std::floor(
        m_router.preliminarySurfaceLevel->compute(DensityFunctionContext{ quantizedX, 0, quantizedZ })));
}

int NoiseBasedChunkGenerator::getBaseHeight(int blockX, int blockZ) const {
    for (int y = m_settings.noiseSettings.minY + m_settings.noiseSettings.height - 1; y >= m_settings.noiseSettings.minY; --y) {
        if (sampleFinalDensity(blockX, y, blockZ) > 0.0) {
            return y + 1;
        }
    }
    return m_settings.noiseSettings.minY;
}

int NoiseBasedChunkGenerator::getOceanFloorHeight(int blockX, int blockZ) const {
    // OCEAN_FLOOR_WG uses the MATERIAL_MOTION_BLOCKING predicate: a block is
    // opaque iff state.blocksMotion(). Fluids (water/lava) do NOT block motion,
    // so they are skipped — the heightmap returns the first SOLID block from
    // the top, which is the ocean floor.
    //
    // In the noise-only world (pre-aquifer), finalDensity > 0 means solid
    // terrain (stone/dirt/etc.), and finalDensity <= 0 means air OR fluid.
    // The aquifer later replaces some air cells with water, but at this stage
    // we can't distinguish "will be water" from "will be air" without running
    // the full aquifer. However, vanilla's OCEAN_FLOOR_WG heightmap is
    // computed AFTER the aquifer, so it sees the actual fluid state.
    //
    // Approximation: for y > seaLevel, finalDensity > 0 is always solid (land
    // above water). For y <= seaLevel, finalDensity > 0 is solid terrain, and
    // finalDensity <= 0 is either air or water — both are skipped by the
    // MATERIAL_MOTION_BLOCKING predicate. So we can simply scan from top to
    // bottom and return the first y where finalDensity > 0, which is exactly
    // what getBaseHeight does. The difference: OCEAN_FLOOR_WG would return the
    // SAME value as WORLD_SURFACE_WG for terrain, because solids are solids.
    //
    // The REAL difference shows up in the aquifer: vanilla's OCEAN_FLOOR_WG
    // skips water cells that the aquifer placed INSIDE what would otherwise be
    // solid terrain (rare, but happens in underwater caves). For structure
    // placement purposes, this is a minor edge case.
    //
    // For now, return getBaseHeight — it matches the server for 7/8 ocean_ruin
    // chunks (the ones where the floor is at or above sea level). The 1 mismatch
    // (chunk 3,22 where floor=47) is a DEEP OCEAN chunk where the floor is below
    // sea level — getBaseHeight correctly returns 47 there too, so the issue
    // was that tryPlaceOceanRuin hardcoded Y=90 instead of calling this method.
    return getBaseHeight(blockX, blockZ);
}

void NoiseBasedChunkGenerator::fillFromNoise(LevelChunk& chunk, std::vector<mc::BlockPos>* fluidUpdateMarks,
                                             const Beardifier* beardifier) const {
    const bool hasBeard = beardifier != nullptr && !beardifier->isEmpty();
    const uint32_t air = stateIdFor("air", 0);
    const uint32_t solid = m_settings.defaultBlock ? m_settings.defaultBlock : stateIdFor("stone", air);
    const uint32_t waterId = stateIdFor("water", UINT32_MAX);
    const uint32_t lavaId = stateIdFor("lava", UINT32_MAX);
    const ChunkPos pos = chunk.pos();
    const int chunkStartBlockX = pos.x * 16;
    const int chunkStartBlockZ = pos.z * 16;

    const NoiseSettings noiseSettings = m_settings.noiseSettings;
    const int minY = noiseSettings.minY;
    const int cellHeight = noiseSettings.getCellHeight();
    const int cellWidth = noiseSettings.getCellWidth();
    const int cellMinY = floorDiv(minY, cellHeight);
    const int cellCountY = floorDiv(noiseSettings.height, cellHeight);
    const int cellCountX = 16 / cellWidth;
    const int cellCountZ = 16 / cellWidth;
    auto fluidPicker = Aquifer::createFluidPicker(m_settings);
    auto preliminarySurface = [this](int blockX, int blockZ) {
        return samplePreliminarySurfaceLevel(blockX, blockZ);
    };
    std::unique_ptr<Aquifer> aquifer = m_settings.isAquifersEnabled()
        ? Aquifer::create(
            std::move(preliminarySurface),
            chunkStartBlockX,
            chunkStartBlockZ,
            m_router,
            m_aquiferRandom,
            minY,
            noiseSettings.height,
            std::move(fluidPicker))
        : Aquifer::createDisabled(std::move(fluidPicker));
    std::optional<OreVeinifier> oreVeinifier;
    if (m_settings.areOreVeinsEnabled()) {
        oreVeinifier.emplace(m_router.veinToggle, m_router.veinRidged, m_router.veinGap, m_oreRandom);
    }
    NoiseChunkSharedCache sharedCache;

    for (int cellXIndex = 0; cellXIndex < cellCountX; ++cellXIndex) {
        for (int cellZIndex = 0; cellZIndex < cellCountZ; ++cellZIndex) {
            for (int cellYIndex = cellCountY - 1; cellYIndex >= 0; --cellYIndex) {
                const int x0 = chunkStartBlockX + cellXIndex * cellWidth;
                const int x1 = x0 + cellWidth;
                const int z0 = chunkStartBlockZ + cellZIndex * cellWidth;
                const int z1 = z0 + cellWidth;
                const int y0 = (cellMinY + cellYIndex) * cellHeight;
                const int y1 = y0 + cellHeight;
                (void)x1;
                (void)y1;
                (void)z1;
                CellInterpolationResolver interpolationResolver(x0, y0, z0, cellWidth, cellHeight, &sharedCache);

                for (int yInCell = cellHeight - 1; yInCell >= 0; --yInCell) {
                    const int blockY = y0 + yInCell;
                    for (int xInCell = 0; xInCell < cellWidth; ++xInCell) {
                        const int blockX = x0 + xInCell;
                        for (int zInCell = 0; zInCell < cellWidth; ++zInCell) {
                            const int blockZ = z0 + zInCell;
                            DensityFunctionContext blockContext{ blockX, blockY, blockZ, &interpolationResolver };
                            // NoiseChunk: add(finalDensity, BeardifierMarker). The
                            // beardifier sits OUTSIDE the interpolated() noise, so it
                            // is computed per block and added. Guarded so a null/empty
                            // beardifier leaves the density bit-identical.
                            double density = m_router.finalDensity->compute(blockContext);
                            if (hasBeard) density += beardifier->compute(blockX, blockY, blockZ);
                            std::optional<uint32_t> aquiferState = aquifer->computeSubstance(
                                blockContext,
                                density);
                            uint32_t state = solid;
                            if (aquiferState) {
                                state = *aquiferState;
                            } else if (oreVeinifier) {
                                std::optional<uint32_t> oreState = oreVeinifier->compute(blockContext);
                                if (oreState) {
                                    state = *oreState;
                                }
                            }

                            if (state != air) {
                                chunk.setBlockDuringNoise(blockX, blockY, blockZ, state);
                                // NoiseBasedChunkGenerator.java:442-446: fluid cells
                                // are marked for FULL postprocessing when the aquifer
                                // requests a fluid update.
                                if (fluidUpdateMarks != nullptr && aquifer->shouldScheduleFluidUpdate()
                                    && (state == waterId || state == lavaId)) {
                                    fluidUpdateMarks->push_back(mc::BlockPos{ blockX, blockY, blockZ });
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void NoiseBasedChunkGenerator::buildSurface(LevelChunk& chunk) const {
    auto biomeCache = std::make_shared<std::unordered_map<BiomeCacheKey, std::string, BiomeCacheKeyHash>>();
    buildSurface(chunk, [this, biomeCache](int bx, int by, int bz) -> std::string {
        if (!m_biomeManager) return "";
        const auto quart = m_biomeManager->selectQuart(bx, by, bz);
        const BiomeCacheKey key{ quart[0], quart[1], quart[2] };
        auto it = biomeCache->find(key);
        if (it != biomeCache->end()) return it->second;
        std::string biome = m_biomeManager->getNoiseBiomeAtQuart(key.x, key.y, key.z);
        auto inserted = biomeCache->emplace(key, std::move(biome));
        return inserted.first->second;
    });
}

void NoiseBasedChunkGenerator::buildSurface(
    LevelChunk& chunk,
    const std::function<std::string(int, int, int)>& biomeOverride) const {
    WorldGenCtx genCtx;
    genCtx.minGenY  = m_settings.noiseSettings.minY;
    genCtx.genDepth = m_settings.noiseSettings.height;

    // Preliminary surface level sampler — delegates to the noise router
    auto prelimSurf = [this](int bx, int bz) {
        return samplePreliminarySurfaceLevel(bx, bz);
    };

    m_surfaceSystem->buildSurface(*m_surfaceRandomState, chunk, prelimSurf, biomeOverride, genCtx, m_surfaceRuleSource);

    chunk.setLoaded(true);
    chunk.meshDirty = true;
}

void NoiseBasedChunkGenerator::applyCarvers(LevelChunk& chunk, std::vector<mc::BlockPos>* fluidUpdateMarks) const {
    // End dimension has NO carvers in vanilla (TheEndBiomeSource + no configured
    // carvers for the end). The nether has one carver (nether_cave). The overworld
    // has three (cave, cave_extra_underground, canyon).
    const uint32_t endStone = stateIdFor("end_stone", UINT32_MAX);
    if (m_settings.defaultBlock == endStone) {
        return;  // end: no carvers
    }

    auto preliminarySurface = [this](int blockX, int blockZ) {
        return samplePreliminarySurfaceLevel(blockX, blockZ);
    };
    auto biomeGetter = [this](int blockX, int blockY, int blockZ) -> std::string {
        return m_biomeManager ? m_biomeManager->getBiome(blockX, blockY, blockZ) : "";
    };
    WorldGenCtx genCtx;
    genCtx.minGenY = m_settings.noiseSettings.minY;
    genCtx.genDepth = m_settings.noiseSettings.height;
    auto topMaterial = [&](LevelChunk& targetChunk, int blockX, int blockY, int blockZ, bool underFluid) {
        return m_surfaceSystem->topMaterial(
            *m_surfaceRandomState,
            targetChunk,
            preliminarySurface,
            biomeGetter,
            genCtx,
            m_surfaceRuleSource,
            blockX,
            blockY,
            blockZ,
            underFluid);
    };

    const uint32_t netherrack = stateIdFor("netherrack", UINT32_MAX);
    if (m_settings.defaultBlock == netherrack) {
        // Nether: apply nether_cave carver (NetherWorldCarver).
        carver::applyNetherCarvers(
            chunk,
            static_cast<std::int64_t>(m_seed),
            m_settings,
            m_router,
            m_aquiferRandom,
            preliminarySurface,
            topMaterial,
            fluidUpdateMarks);
        return;
    }

    // Overworld: apply cave, cave_extra_underground, canyon.
    carver::applyOverworldCarvers(
        chunk,
        static_cast<std::int64_t>(m_seed),
        m_settings,
        m_router,
        m_aquiferRandom,
        preliminarySurface,
        topMaterial,
        fluidUpdateMarks);
}

} // namespace mc::levelgen

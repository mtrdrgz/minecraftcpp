#include "NoiseBasedChunkGenerator.h"
#include "Aquifer.h"
#include "NoiseRouterData.h"
#include "OreVeinifier.h"
#include "RandomState.h"
#include "SurfaceSystem.h"
#include "SurfaceRuleData.h"
#include "../block/Blocks.h"
#include "../block/BlockState.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <unordered_map>

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

    class CellInterpolationResolver final : public DensityFunctionInterpolationResolver {
    public:
        CellInterpolationResolver(int x0, int y0, int z0, int cellWidth, int cellHeight)
            : m_x0(x0), m_y0(y0), m_z0(z0), m_cellWidth(cellWidth), m_cellHeight(cellHeight) {
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
                std::array<double, 8> values{
                    function->compute(DensityFunctionContext{ m_x0, m_y0, m_z0 }),
                    function->compute(DensityFunctionContext{ m_x0 + m_cellWidth, m_y0, m_z0 }),
                    function->compute(DensityFunctionContext{ m_x0, m_y0 + m_cellHeight, m_z0 }),
                    function->compute(DensityFunctionContext{ m_x0 + m_cellWidth, m_y0 + m_cellHeight, m_z0 }),
                    function->compute(DensityFunctionContext{ m_x0, m_y0, m_z0 + m_cellWidth }),
                    function->compute(DensityFunctionContext{ m_x0 + m_cellWidth, m_y0, m_z0 + m_cellWidth }),
                    function->compute(DensityFunctionContext{ m_x0, m_y0 + m_cellHeight, m_z0 + m_cellWidth }),
                    function->compute(DensityFunctionContext{ m_x0 + m_cellWidth, m_y0 + m_cellHeight, m_z0 + m_cellWidth })
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
            return lerp3(factorX, factorY, factorZ, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
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
        return NoiseRouterData::overworld(randomState, false, false);
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

    // Pick the appropriate surface rule for this dimension
    uint32_t netherrack = getDefaultBlockStateId("netherrack", UINT32_MAX);
    uint32_t endStone   = getDefaultBlockStateId("end_stone",  UINT32_MAX);
    if (m_settings.defaultBlock == netherrack) {
        m_surfaceRuleSource = SurfaceRuleData::nether();
    } else if (m_settings.defaultBlock == endStone) {
        m_surfaceRuleSource = SurfaceRuleData::end();
    } else {
        m_surfaceRuleSource = SurfaceRuleData::overworld();
        m_biomeSource = std::make_unique<BiomeSource>(m_router);
        m_biomeManager = std::make_unique<BiomeManager>(
            *m_biomeSource,
            BiomeManager::obfuscateSeed(static_cast<int64_t>(m_seed)));
    }
}

uint32_t NoiseBasedChunkGenerator::stateIdFor(const char* blockName, uint32_t fallback) const {
    return getDefaultBlockStateId(blockName, fallback);
}

double NoiseBasedChunkGenerator::sampleFinalDensity(int blockX, int blockY, int blockZ) const {
    return m_router.finalDensity->compute(DensityFunctionContext{ blockX, blockY, blockZ });
}

int NoiseBasedChunkGenerator::samplePreliminarySurfaceLevel(int blockX, int blockZ) const {
    return static_cast<int>(m_router.preliminarySurfaceLevel->compute(DensityFunctionContext{ blockX, 0, blockZ }));
}

int NoiseBasedChunkGenerator::getBaseHeight(int blockX, int blockZ) const {
    for (int y = m_settings.noiseSettings.minY + m_settings.noiseSettings.height - 1; y >= m_settings.noiseSettings.minY; --y) {
        if (sampleFinalDensity(blockX, y, blockZ) > 0.0) {
            return y + 1;
        }
    }
    return m_settings.noiseSettings.minY;
}

void NoiseBasedChunkGenerator::fillFromNoise(LevelChunk& chunk) const {
    const uint32_t air = stateIdFor("air", 0);
    const uint32_t solid = m_settings.defaultBlock ? m_settings.defaultBlock : stateIdFor("stone", air);
    const uint32_t bedrock = stateIdFor("bedrock", solid);
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
                CellInterpolationResolver interpolationResolver(x0, y0, z0, cellWidth, cellHeight);

                for (int yInCell = cellHeight - 1; yInCell >= 0; --yInCell) {
                    const int blockY = y0 + yInCell;
                    for (int xInCell = 0; xInCell < cellWidth; ++xInCell) {
                        const int blockX = x0 + xInCell;
                        for (int zInCell = 0; zInCell < cellWidth; ++zInCell) {
                            const int blockZ = z0 + zInCell;
                            DensityFunctionContext blockContext{ blockX, blockY, blockZ, &interpolationResolver };
                            const double density = m_router.finalDensity->compute(blockContext);
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

                            if (blockY <= CHUNK_MIN_Y + 4 && solid != air) {
                                state = bedrock;
                            }

                            if (state != air) {
                                chunk.setBlock(blockX, blockY, blockZ, state);
                            }
                        }
                    }
                }
            }
        }
    }
}

void NoiseBasedChunkGenerator::buildSurface(LevelChunk& chunk) const {
    // Ensure heightmap is accurate before SurfaceSystem reads it (for steep condition)
    chunk.computeHeightmap();

    // Build a fresh RandomState to pass into the surface system.
    // In Java, the server keeps one persistent RandomState per world; here we
    // recreate it per chunk (cheap since noise instances are just seeded Perlin trees).
    RandomState randomState(m_settings, m_seed);

    // noiseRandom is used for clay bands and surface-depth variance
    auto noiseRandom = randomState.getOrCreateRandomFactory("minecraft:terrain");

    SurfaceSystem system(randomState, noiseRandom, m_settings.defaultBlock, m_settings.seaLevel);

    WorldGenCtx genCtx;
    genCtx.minGenY  = m_settings.noiseSettings.minY;
    genCtx.genDepth = m_settings.noiseSettings.height;

    // Preliminary surface level sampler — delegates to the noise router
    auto prelimSurf = [this](int bx, int bz) {
        return samplePreliminarySurfaceLevel(bx, bz);
    };

    auto biomeGetter = [this](int bx, int by, int bz) -> std::string {
        return m_biomeManager ? m_biomeManager->getBiome(bx, by, bz) : "";
    };

    system.buildSurface(randomState, chunk, prelimSurf, biomeGetter, genCtx, m_surfaceRuleSource);


    // Vegetation decoration (trees) — called here after surface is final


    chunk.computeHeightmap();
    chunk.setLoaded(true);
    chunk.meshDirty = true;
}

} // namespace mc::levelgen

#include "SurfaceSystem.h"
#include "RandomState.h"
#include "Noises.h"
#include "../block/Blocks.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mc::levelgen {

namespace {

// Sentinel value from Java's DimensionType.WAY_BELOW_MIN_Y
static constexpr int WAY_BELOW_MIN_Y = -4064;

const PerlinSimplexNoise& frozenTemperatureNoise() {
    static const PerlinSimplexNoise noise = [] {
        LegacyRandomSource random(3456L);
        return PerlinSimplexNoise(random, { -2, -1, 0 });
    }();
    return noise;
}

const PerlinSimplexNoise& biomeInfoNoise() {
    static const PerlinSimplexNoise noise = [] {
        LegacyRandomSource random(2345L);
        return PerlinSimplexNoise(random, { 0 });
    }();
    return noise;
}

float frozenTemperatureModifier(int blockX, int blockZ, float baseTemperature) {
    const double groundValueLargeVariation = frozenTemperatureNoise().getValue(blockX * 0.05, blockZ * 0.05, false) * 7.0;
    const double groundValueEdgeVariation = biomeInfoNoise().getValue(blockX * 0.2, blockZ * 0.2, false);
    const double icePatches = groundValueLargeVariation + groundValueEdgeVariation;
    if (icePatches < 0.3) {
        const double groundValueSmallVariation = biomeInfoNoise().getValue(blockX * 0.09, blockZ * 0.09, false);
        if (groundValueSmallVariation < 0.8) {
            return 0.2f;
        }
    }

    return baseTemperature;
}

float frozenOceanBaseTemperature(const std::string& biome) {
    return biome == "minecraft:deep_frozen_ocean" ? 0.5f : 0.0f;
}

bool shouldMeltFrozenOceanIcebergSlightly(const std::string& biome, int blockX, int blockZ) {
    return frozenTemperatureModifier(blockX, blockZ, frozenOceanBaseTemperature(biome)) > 0.1f;
}

} // anonymous namespace

SurfaceSystem::SurfaceSystem(RandomState& randomState,
                              std::shared_ptr<PositionalRandomFactory> noiseRandom,
                              uint32_t defaultBlock,
                              int seaLevel)
    : m_defaultBlock(defaultBlock)
    , m_seaLevel(seaLevel)
    , m_noiseRandom(std::move(noiseRandom))
{
    m_waterStateId = getDefaultBlockStateId("water", 0);
    m_lavaStateId  = getDefaultBlockStateId("lava",  0);

    m_clayBandsOffsetNoise   = randomState.getOrCreateNoise(Noises::CLAY_BANDS_OFFSET);
    m_surfaceNoise           = randomState.getOrCreateNoise(Noises::SURFACE);
    m_surfaceSecondaryNoise  = randomState.getOrCreateNoise(Noises::SURFACE_SECONDARY);
    m_badlandsPillarNoise    = randomState.getOrCreateNoise(Noises::BADLANDS_PILLAR);
    m_badlandsPillarRoofNoise= randomState.getOrCreateNoise(Noises::BADLANDS_PILLAR_ROOF);
    m_badlandsSurfaceNoise   = randomState.getOrCreateNoise(Noises::BADLANDS_SURFACE);
    m_icebergPillarNoise     = randomState.getOrCreateNoise(Noises::ICEBERG_PILLAR);
    m_icebergPillarRoofNoise = randomState.getOrCreateNoise(Noises::ICEBERG_PILLAR_ROOF);
    m_icebergSurfaceNoise    = randomState.getOrCreateNoise(Noises::ICEBERG_SURFACE);

    // Clay bands seeded via hash of "minecraft:clay_bands"
    auto bandRng = m_noiseRandom->fromHashOf("minecraft:clay_bands");
    m_clayBands = generateBands(*bandRng);
}

// ---------- buildSurface ----------

void SurfaceSystem::buildSurface(
    RandomState& randomState,
    LevelChunk& chunk,
    const std::function<int(int, int)>& prelimSurfFn,
    const std::function<std::string(int, int, int)>& biomeGetter,
    const WorldGenCtx& genCtx,
    const SurfaceRules::RuleSourcePtr& ruleSource
) {
    const ChunkPos chunkPos = chunk.pos();
    const int minBlockX     = chunkPos.x * 16;
    const int minBlockZ     = chunkPos.z * 16;
    const int endY          = CHUNK_MIN_Y; // protoChunk.getMinY()

    const uint32_t airId = getDefaultBlockStateId("air", 0);

    // Build the surface context
    SurfaceRules::Context ctx;
    ctx.system       = this;
    ctx.chunk        = &chunk;
    ctx.randomState  = &randomState;
    ctx.prelimSurfFn = prelimSurfFn;
    ctx.biomeGetter  = biomeGetter;
    ctx.genCtx       = genCtx;
    ctx.initConditions();

    // "Compile" the rule tree once — each source creates a concrete rule bound to ctx
    SurfaceRules::SurfaceRulePtr rule = ruleSource->apply(ctx);

    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            const int blockX = minBlockX + x;
            const int blockZ = minBlockZ + z;

            // startingHeight = Java's (getHeight(WORLD_SURFACE_WG, x, z) + 1)
            // (SurfaceSystem.java:110). ChunkAccess.getHeight returns
            // heightmap.getFirstAvailable(x,z) - 1 == the TOPMOST stored y
            // (ChunkAccess.java:182-194) — NOT the WorldGenRegion stored+1 semantics.
            // Our heightmap() returns the same topmost non-air y, so
            // startingHeight = heightmap + 1. (A former +2 here sampled the zoomed
            // surface biome one block too high, flipping fiddle-border columns:
            // frozen_river vs frozen_ocean decides the frozenOceanExtension pillars.)
            const int startingHeight = chunk.heightmap(x, z) + 1;

            // Biome-specific pre-passes (SurfaceSystem.java:112-115)
            const std::string biome = biomeGetter(blockX, startingHeight, blockZ);
            if (biome == "minecraft:eroded_badlands") {
                erodedBadlandsExtension(chunk, blockX, blockZ, startingHeight);
            }

            // Re-read height after possible extension (SurfaceSystem.java:117)
            const int height = chunk.heightmap(x, z) + 1;

            ctx.updateXZ(blockX, blockZ);
            int stoneAboveDepth = 0;
            int waterHeight     = std::numeric_limits<int>::min();
            int nextCeilingStoneY = std::numeric_limits<int>::max();

            for (int y = height; y >= endY; --y) {
                uint32_t blockHere = chunk.getBlock(blockX, y, blockZ);

                if (blockHere == airId) {
                    stoneAboveDepth = 0;
                    waterHeight     = std::numeric_limits<int>::min();
                } else if (blockHere == m_waterStateId || blockHere == m_lavaStateId) {
                    // fluid block
                    if (waterHeight == std::numeric_limits<int>::min()) {
                        waterHeight = y + 1;
                    }
                } else {
                    // solid block
                    if (nextCeilingStoneY >= y) {
                        // Find next non-stone block below to establish ceiling of this run
                        nextCeilingStoneY = WAY_BELOW_MIN_Y;
                        for (int ly = y - 1; ly >= endY - 1; --ly) {
                            if (!isStone(chunk.getBlock(blockX, ly, blockZ))) {
                                nextCeilingStoneY = ly + 1;
                                break;
                            }
                        }
                    }

                    ++stoneAboveDepth;
                    int stoneBelowDepth = y - nextCeilingStoneY + 1;
                    ctx.updateY(stoneAboveDepth, stoneBelowDepth, waterHeight, blockX, y, blockZ);

                    if (blockHere == m_defaultBlock) {
                        auto result = rule->tryApply(blockX, y, blockZ);
                        if (result.has_value() && *result != blockHere) {
                            chunk.setBlock(blockX, y, blockZ, *result);
                        }
                    }
                }
            }

            if (biome == "minecraft:frozen_ocean" || biome == "minecraft:deep_frozen_ocean") {
                frozenOceanExtension(ctx.getMinSurfaceLevel(), biome, chunk, blockX, blockZ, startingHeight);
            }
        }
    }
}

std::optional<uint32_t> SurfaceSystem::topMaterial(
    RandomState& randomState,
    LevelChunk& chunk,
    const std::function<int(int, int)>& prelimSurfFn,
    const std::function<std::string(int, int, int)>& biomeGetter,
    const WorldGenCtx& genCtx,
    const SurfaceRules::RuleSourcePtr& ruleSource,
    int blockX,
    int blockY,
    int blockZ,
    bool underFluid
) {
    SurfaceRules::Context ctx;
    ctx.system       = this;
    ctx.chunk        = &chunk;
    ctx.randomState  = &randomState;
    ctx.prelimSurfFn = prelimSurfFn;
    ctx.biomeGetter  = biomeGetter;
    ctx.genCtx       = genCtx;
    ctx.initConditions();

    SurfaceRules::SurfaceRulePtr rule = ruleSource->apply(ctx);
    ctx.updateXZ(blockX, blockZ);
    ctx.updateY(
        1,
        1,
        underFluid ? blockY + 1 : std::numeric_limits<int>::min(),
        blockX,
        blockY,
        blockZ);
    return rule->tryApply(blockX, blockY, blockZ);
}

// ---------- Helper methods ----------

int SurfaceSystem::getSurfaceDepth(int blockX, int blockZ) {
    double noiseVal = m_surfaceNoise->getValue(blockX, 0.0, blockZ);
    auto rng = m_noiseRandom->at(blockX, 0, blockZ);
    return (int)(noiseVal * 2.75 + 3.0 + rng->nextDouble() * 0.25);
}

double SurfaceSystem::getSurfaceSecondary(int blockX, int blockZ) {
    return m_surfaceSecondaryNoise->getValue(blockX, 0.0, blockZ);
}

uint32_t SurfaceSystem::getBand(int worldX, int y, int worldZ) {
    int offset = (int)std::round(m_clayBandsOffsetNoise->getValue(worldX, 0.0, worldZ) * 4.0);
    int index  = (y + offset + (int)m_clayBands.size()) % (int)m_clayBands.size();
    return m_clayBands[index];
}

bool SurfaceSystem::isStone(uint32_t stateId) const noexcept {
    return stateId != 0 && stateId != m_waterStateId && stateId != m_lavaStateId;
}

// ---------- erodedBadlandsExtension ----------
// Port of SurfaceSystem.erodedBadlandsExtension

void SurfaceSystem::erodedBadlandsExtension(LevelChunk& chunk, int blockX, int blockZ, int height) {
    double pillarBuffer = std::min(
        std::abs(m_badlandsSurfaceNoise->getValue(blockX, 0.0, blockZ) * 8.25),
        m_badlandsPillarNoise->getValue(blockX * 0.2, 0.0, blockZ * 0.2) * 15.0
    );
    if (pillarBuffer <= 0.0) return;

    double pillarFloor    = std::abs(m_badlandsPillarRoofNoise->getValue(blockX * 0.75, 0.0, blockZ * 0.75) * 1.5);
    double extensionTop   = 64.0 + std::min(pillarBuffer * pillarBuffer * 2.5, std::ceil(pillarFloor * 50.0) + 24.0);
    int    startY         = (int)extensionTop;
    if (height > startY) return;

    // Check for existing default block or water — abort if found
    for (int y = startY; y >= CHUNK_MIN_Y; --y) {
        uint32_t s = chunk.getBlock(blockX, y, blockZ);
        if (s == m_defaultBlock) break;
        if (s == m_waterStateId) return;
    }

    // Fill air from startY down to the first non-air block
    for (int y = startY; y >= CHUNK_MIN_Y && chunk.getBlock(blockX, y, blockZ) == 0; --y) {
        chunk.setBlock(blockX, y, blockZ, m_defaultBlock);
    }
}

// ---------- frozenOceanExtension ----------
// Port of SurfaceSystem.frozenOceanExtension

void SurfaceSystem::frozenOceanExtension(int minSurfaceLevel, const std::string& biome, LevelChunk& chunk,
                                          int blockX, int blockZ, int height) {
    double iceberg = std::min(
        std::abs(m_icebergSurfaceNoise->getValue(blockX, 0.0, blockZ) * 8.25),
        m_icebergPillarNoise->getValue(blockX * 1.28, 0.0, blockZ * 1.28) * 15.0
    );
    if (std::getenv("MCPP_DBG_ICEBERG") != nullptr) {
        std::fprintf(stderr, "ICEBERG\t%d\t%d\t%s\th=%d\ticeberg=%.17g\tsurf=%.17g\tpillar=%.17g\tmelt=%d\n",
                     blockX, blockZ, biome.c_str(), height, iceberg,
                     m_icebergSurfaceNoise->getValue(blockX, 0.0, blockZ),
                     m_icebergPillarNoise->getValue(blockX * 1.28, 0.0, blockZ * 1.28),
                     (int)shouldMeltFrozenOceanIcebergSlightly(biome, blockX, blockZ));
    }
    if (iceberg <= 1.8) return;

    double icebergRoof = std::abs(m_icebergPillarRoofNoise->getValue(blockX * 1.17, 0.0, blockZ * 1.17) * 1.5);
    double top         = std::min(iceberg * iceberg * 1.2, std::ceil(icebergRoof * 40.0) + 14.0);
    if (shouldMeltFrozenOceanIcebergSlightly(biome, blockX, blockZ)) {
        top -= 2.0;
    }
    double extensionBottom;
    if (top > 2.0) {
        extensionBottom = m_seaLevel - top - 7.0;
        top += m_seaLevel;
    } else {
        top = 0.0;
        extensionBottom = 0.0;
    }

    const uint32_t airId      = getDefaultBlockStateId("air",        0);
    const uint32_t packedIce  = getDefaultBlockStateId("packed_ice",  m_defaultBlock);
    const uint32_t snowBlock  = getDefaultBlockStateId("snow_block",  m_defaultBlock);

    auto rng         = m_noiseRandom->at(blockX, 0, blockZ);
    int maxSnowDepth = 2 + rng->nextInt(4);
    int minSnowY     = m_seaLevel + 18 + rng->nextInt(10);
    int snowDepth    = 0;

    int startScan = std::max(height, (int)top + 1);
    for (int y = startScan; y >= minSurfaceLevel; --y) {
        uint32_t cur = chunk.getBlock(blockX, y, blockZ);
        bool fillHere = false;
        if (cur == airId && y < (int)top && rng->nextDouble() > 0.01) {
            fillHere = true;
        } else if (cur == m_waterStateId &&
                   y > (int)extensionBottom && y < m_seaLevel &&
                   extensionBottom != 0.0 && rng->nextDouble() > 0.15) {
            fillHere = true;
        }
        if (!fillHere) continue;

        if (snowDepth <= maxSnowDepth && y > minSnowY) {
            chunk.setBlock(blockX, y, blockZ, snowBlock);
            ++snowDepth;
        } else {
            chunk.setBlock(blockX, y, blockZ, packedIce);
        }
    }
}

// ---------- Clay band generation ----------

std::array<uint32_t, 192> SurfaceSystem::generateBands(RandomSource& random) {
    const uint32_t terracotta      = getDefaultBlockStateId("terracotta",             0);
    const uint32_t orange          = getDefaultBlockStateId("orange_terracotta",      terracotta);
    const uint32_t yellow          = getDefaultBlockStateId("yellow_terracotta",      terracotta);
    const uint32_t brown           = getDefaultBlockStateId("brown_terracotta",       terracotta);
    const uint32_t red             = getDefaultBlockStateId("red_terracotta",         terracotta);
    const uint32_t white           = getDefaultBlockStateId("white_terracotta",       terracotta);
    const uint32_t lightGray       = getDefaultBlockStateId("light_gray_terracotta",  terracotta);

    std::array<uint32_t, 192> bands;
    bands.fill(terracotta);

    // Scattered orange bands — Java: for(int i=0; i<length; i++) { i+=nextInt(5)+1; ... }
    // The for-loop increment (i++) fires after the body, giving a total advance of 2..6.
    for (int i = 0; i < (int)bands.size(); ++i) {
        i += random.nextInt(5) + 1;
        if (i < (int)bands.size()) {
            bands[i] = orange;
        }
    }

    makeBands(random, bands, 1, yellow);
    makeBands(random, bands, 2, brown);
    makeBands(random, bands, 1, red);

    // White + light-grey accent bands
    int whiteBandCount = random.nextInt(7) + 9; // nextIntBetweenInclusive(9, 15)
    int placed = 0;
    for (int start = 0; placed < whiteBandCount && start < (int)bands.size();
         start += random.nextInt(16) + 4) {
        bands[start] = white;
        if (start - 1 > 0 && random.nextBoolean()) {
            bands[start - 1] = lightGray;
        }
        if (start + 1 < (int)bands.size() && random.nextBoolean()) {
            bands[start + 1] = lightGray;
        }
        ++placed;
    }
    return bands;
}

void SurfaceSystem::makeBands(RandomSource& random, std::array<uint32_t, 192>& bands,
                               int baseWidth, uint32_t stateId) {
    int bandCount = random.nextInt(10) + 6; // nextIntBetweenInclusive(6, 15)
    for (int i = 0; i < bandCount; ++i) {
        int width = baseWidth + random.nextInt(3);
        int start = random.nextInt((int)bands.size());
        for (int p = 0; start + p < (int)bands.size() && p < width; ++p) {
            bands[start + p] = stateId;
        }
    }
}

} // namespace mc::levelgen

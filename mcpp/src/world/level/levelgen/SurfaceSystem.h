#pragma once

// Port of net.minecraft.world.level.levelgen.SurfaceSystem

#include "SurfaceRules.h"
#include "Noise.h"
#include "RandomSource.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace mc::levelgen {

class RandomState;

class SurfaceSystem {
public:
    // randomState  — provides all named noises
    // noiseRandom  — positional factory for clay bands and surface-depth variance
    //                (Java: randomState.getOrCreateRandomFactory("minecraft:terrain"))
    // defaultBlock — stone for overworld, netherrack for nether, etc.
    // seaLevel     — Y of the sea surface
    SurfaceSystem(RandomState& randomState,
                  std::shared_ptr<PositionalRandomFactory> noiseRandom,
                  uint32_t defaultBlock,
                  int seaLevel);

    // Port of SurfaceSystem.buildSurface — applies the surface rule tree to the chunk.
    // biomeGetter returns a biome key string (e.g. "minecraft:plains"), or "" if unknown.
    void buildSurface(
        RandomState& randomState,
        LevelChunk& chunk,
        const std::function<int(int, int)>& prelimSurfFn,
        const std::function<std::string(int, int, int)>& biomeGetter,
        const WorldGenCtx& genCtx,
        const SurfaceRules::RuleSourcePtr& ruleSource
    );

    int    getSurfaceDepth(int blockX, int blockZ);
    double getSurfaceSecondary(int blockX, int blockZ);
    uint32_t getBand(int worldX, int y, int worldZ);
    int    getSeaLevel() const noexcept { return m_seaLevel; }

private:
    bool isStone(uint32_t stateId) const noexcept;

    void erodedBadlandsExtension(LevelChunk& chunk, int blockX, int blockZ, int height);
    void frozenOceanExtension(int minSurfaceLevel, LevelChunk& chunk,
                               int blockX, int blockZ, int height);

    static std::array<uint32_t, 192> generateBands(RandomSource& random);
    static void makeBands(RandomSource& random, std::array<uint32_t, 192>& bands,
                          int baseWidth, uint32_t stateId);

    uint32_t m_defaultBlock = 0;
    int      m_seaLevel     = 63;

    // Fluid IDs used by isStone()
    uint32_t m_waterStateId = 0;
    uint32_t m_lavaStateId  = 0;

    std::shared_ptr<PositionalRandomFactory> m_noiseRandom;

    std::shared_ptr<NormalNoise> m_clayBandsOffsetNoise;
    std::shared_ptr<NormalNoise> m_surfaceNoise;
    std::shared_ptr<NormalNoise> m_surfaceSecondaryNoise;
    std::shared_ptr<NormalNoise> m_badlandsPillarNoise;
    std::shared_ptr<NormalNoise> m_badlandsPillarRoofNoise;
    std::shared_ptr<NormalNoise> m_badlandsSurfaceNoise;
    std::shared_ptr<NormalNoise> m_icebergPillarNoise;
    std::shared_ptr<NormalNoise> m_icebergPillarRoofNoise;
    std::shared_ptr<NormalNoise> m_icebergSurfaceNoise;

    std::array<uint32_t, 192> m_clayBands{};
};

} // namespace mc::levelgen

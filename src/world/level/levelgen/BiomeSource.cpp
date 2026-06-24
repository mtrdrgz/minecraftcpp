#include "BiomeSource.h"
#include "OverworldBiomeBuilder.h"

#include <set>

namespace mc::levelgen {

namespace {
std::vector<std::string> distinctBiomeIdsInEncounterOrder(const std::vector<Climate::ParameterList<std::string>::Entry>& entries) {
    std::set<std::string> seen;
    std::vector<std::string> out;
    for (const auto& entry : entries) {
        const std::string& biome = entry.second;
        if (seen.insert(biome).second) {
            out.push_back(biome);
        }
    }
    return out;
}
} // namespace

BiomeSource::BiomeSource(const NoiseRouter& router) : BiomeSource(router, Dimension::Overworld) {}

BiomeSource::BiomeSource(const NoiseRouter& router, Dimension dim)
    : m_router(router), m_dimension(dim) {
    if (dim == Dimension::Nether) {
        // Port of MultiNoiseBiomeSource.createFromPreset(NETHER). The nether
        // preset (5 biomes) is built by buildNetherBiomePreset() — verbatim
        // from MultiNoiseBiomeSourceParameterList + the nether preset JSON.
        m_parameters = Climate::ParameterList<std::string>(buildNetherBiomePreset());
        m_possibleBiomes = distinctBiomeIdsInEncounterOrder(m_parameters.values());
        m_overworldPresetComplete = false;
    } else {
        // Overworld: 65-biome climate parameter list.
        m_parameters = Climate::ParameterList<std::string>(buildOverworldBiomePreset());
        m_possibleBiomes = distinctBiomeIdsInEncounterOrder(m_parameters.values());
        m_overworldPresetComplete = true;
    }
}

BiomeSource BiomeSource::createNether(const NoiseRouter& router) {
    return BiomeSource(router, Dimension::Nether);
}

// ── End biome source ───────────────────────────────────────────────────────
// Port of net.minecraft.world.level.biome.TheEndBiomeSource.java.
// The End does NOT use Climate parameters. Instead:
//   1. If the chunk is within radius 64 of the origin (chunkX² + chunkZ² ≤ 4096),
//      return minecraft:the_end (the central island).
//   2. Otherwise, sample the EROSION density function at a per-chunk sample
//      point and pick highlands / midlands / small_end_islands / end_barrens
//      based on the value:
//        > 0.25        → end_highlands
//        >= -0.0625    → end_midlands
//        < -0.21875    → small_end_islands
//        else          → end_barrens
// The sample point is ((chunkX*2+1)*8, blockY, (chunkZ*2+1)*8) — the chunk
// center in block coords, exactly as Java's TheEndBiomeSource.getNoiseBiome.
BiomeSource::BiomeSource(const NoiseRouter& router, EndBiomes endBiomes)
    : m_router(router), m_dimension(Dimension::End), m_endBiomes(std::move(endBiomes)) {
    m_possibleBiomes = {
        m_endBiomes.theEnd,
        m_endBiomes.highlands,
        m_endBiomes.midlands,
        m_endBiomes.smallIslands,
        m_endBiomes.barrens
    };
    m_overworldPresetComplete = false;
}

BiomeSource BiomeSource::createEnd(const NoiseRouter& router) {
    EndBiomes endBiomes;
    endBiomes.theEnd        = "minecraft:the_end";
    endBiomes.highlands     = "minecraft:end_highlands";
    endBiomes.midlands      = "minecraft:end_midlands";
    endBiomes.smallIslands  = "minecraft:small_end_islands";
    endBiomes.barrens       = "minecraft:end_barrens";
    return BiomeSource(router, std::move(endBiomes));
}

std::vector<std::string> BiomeSource::collectOverworldPossibleBiomes() {
    return distinctBiomeIdsInEncounterOrder(buildOverworldBiomePreset());
}

std::string BiomeSource::getBiomeAt(int blockX, int blockY, int blockZ) const {
    return getNoiseBiome(blockX >> 2, blockY >> 2, blockZ >> 2);
}

std::string BiomeSource::getNoiseBiome(int quartX, int quartY, int quartZ) const {
    if (m_dimension == Dimension::End) {
        // TheEndBiomeSource.getNoiseBiome — erosion-based, NOT climate.
        // Java: blockX = QuartPos.toBlock(quartX) = quartX << 2
        const int blockX = quartX << 2;
        const int blockY = quartY << 2;
        const int blockZ = quartZ << 2;
        // SectionPos.blockToSectionCoord(blockX) = blockX >> 4
        const int chunkX = blockX >> 4;
        const int chunkZ = blockZ >> 4;
        // Central island: chunkX² + chunkZ² ≤ 4096 (64² blocks = 4² chunks radius)
        if (static_cast<int64_t>(chunkX) * chunkX + static_cast<int64_t>(chunkZ) * chunkZ <= 4096L) {
            return m_endBiomes.theEnd;
        }
        // Sample erosion at the chunk-center block coords.
        if (!m_router.erosion) return m_endBiomes.barrens;
        const int weirdBlockX = (chunkX * 2 + 1) * 8;
        const int weirdBlockZ = (chunkZ * 2 + 1) * 8;
        DensityFunctionContext ctx{ weirdBlockX, blockY, weirdBlockZ, nullptr };
        const double heightValue = m_router.erosion->compute(ctx);
        if (heightValue > 0.25)         return m_endBiomes.highlands;
        if (heightValue >= -0.0625)     return m_endBiomes.midlands;
        if (heightValue < -0.21875)     return m_endBiomes.smallIslands;
        return m_endBiomes.barrens;
    }

    // Overworld + Nether: Climate R-tree lookup (MultiNoiseBiomeSource).
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

} // namespace mc::levelgen

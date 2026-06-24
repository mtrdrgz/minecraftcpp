#pragma once

#include "Climate.h"
#include "NoiseRouter.h"
#include <string>
#include <vector>

namespace mc::levelgen {

// Port of net.minecraft.world.level.biome.BiomeSource + the two concrete
// sources used by the vanilla terrain generator:
//   * MultiNoiseBiomeSource (overworld + nether) — climate-parameter R-tree
//   * TheEndBiomeSource — erosion-noise + distance-from-origin
//
// The overworld preset is loaded in the default ctor; the nether preset is
// loaded via createNether(); the end source is a separate code path
// (createEnd()) because TheEndBiomeSource does NOT use Climate parameters.
class BiomeSource {
public:
    enum class Dimension { Overworld, Nether, End };

    // Overworld: builds the 65-biome climate parameter list.
    explicit BiomeSource(const NoiseRouter& router);

    // Nether: builds the 5-biome nether climate parameter list.
    // Port of MultiNoiseBiomeSource.createFromPreset(NETHER) — the nether
    // preset lives in buildNetherBiomePreset() (OverworldBiomeBuilder.cpp).
    static BiomeSource createNether(const NoiseRouter& router);

    // End: NOT a MultiNoiseBiomeSource. Port of TheEndBiomeSource.java — uses
    // the erosion density function + distance-from-origin to pick one of 5 end
    // biomes. See EndBiomeSource.cpp for the algorithm.
    static BiomeSource createEnd(const NoiseRouter& router);

    std::string getBiomeAt(int blockX, int blockY, int blockZ) const;
    std::string getNoiseBiome(int quartX, int quartY, int quartZ) const;

    // Mirrors MultiNoiseBiomeSource.collectPossibleBiomes() for the built-in
    // overworld preset: first distinct biome id from the preset parameter list,
    // in encounter order. ChunkGenerator feeds exactly this source set to
    // FeatureSorter before decoration.
    const std::vector<std::string>& possibleBiomes() const noexcept { return m_possibleBiomes; }
    static std::vector<std::string> collectOverworldPossibleBiomes();

    bool hasCompleteOverworldPreset() const noexcept { return m_overworldPresetComplete; }
    size_t parameterCount() const noexcept { return m_parameters.values().size(); }

private:
    // Overworld/Nether ctor (climate-based).
    BiomeSource(const NoiseRouter& router, Dimension dim);

    // End ctor (erosion-based).
    struct EndBiomes {
        std::string theEnd;
        std::string highlands;
        std::string midlands;
        std::string smallIslands;
        std::string barrens;
    };
    BiomeSource(const NoiseRouter& router, EndBiomes endBiomes);

    NoiseRouter m_router;
    Climate::ParameterList<std::string> m_parameters;
    std::vector<std::string> m_possibleBiomes;
    bool m_overworldPresetComplete = false;
    Dimension m_dimension = Dimension::Overworld;

    // End-only state (empty for Overworld/Nether).
    EndBiomes m_endBiomes;
};

} // namespace mc::levelgen

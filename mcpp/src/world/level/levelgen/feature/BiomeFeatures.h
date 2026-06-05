#pragma once

// Per-biome ordered placed-feature lists, keyed by GenerationStep.Decoration,
// ported 1:1 from data/minecraft/worldgen/biome/*.json (the `features` array).
//
// The list order and a feature's index within a step are exactly what
// ChunkGenerator.applyBiomeDecoration uses to seed it:
//   setFeatureSeed(decorationSeed, indexWithinStep, step).
// This is pure registry data (no behaviour), so it is loaded from the asset JSON
// the same way BlockTags is — keeping the engine free of embedded Mojang data.

#include "GenerationStep.h"

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace mc::levelgen::feature {

class BiomeFeatures {
public:
    // Reads every biome/*.json under `dir`, recording its features[step][i] keys.
    static BiomeFeatures loadFromDirectory(const std::string& dir);
    static BiomeFeatures loadFromJsonEntries(const std::vector<std::pair<std::string, std::string>>& entries);

    bool hasBiome(const std::string& biome) const;

    // The ordered placed-feature keys (e.g. "minecraft:flower_plains") that this
    // biome contributes to `step`. Empty (static) if the biome/step is absent.
    const std::vector<std::string>& featuresForStep(const std::string& biome, int step) const;

    // Whether `biome` lists `featureKey` in `step` (the minecraft:biome filter).
    bool biomeHasFeature(const std::string& biome, int step, const std::string& featureKey) const;

    std::size_t biomeCount() const { return m_biomes.size(); }

private:
    std::map<std::string, std::array<std::vector<std::string>, GenerationStep::COUNT>> m_biomes;
};

} // namespace mc::levelgen::feature

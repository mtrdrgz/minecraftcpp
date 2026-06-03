#pragma once

#include "Biome.h"

#include <map>
#include <string>

namespace mc::biome {

// Loads the biome registry from the 26.1.2 worldgen data
// (data/minecraft/worldgen/biome/*.json). The same JSON the game deserialises
// at runtime, so this is the faithful runtime port of the biome registry load.
class BiomeRegistry {
public:
    // Parse a single biome from JSON text. `id` is the registry id, e.g.
    // "minecraft:plains".
    static Biome parseBiome(const std::string& id, const std::string& jsonText);

    // Load every *.json under `dir` as minecraft:<filename-stem>.
    static BiomeRegistry loadFromDirectory(const std::string& dir);

    void add(Biome biome);
    bool contains(const std::string& id) const;
    const Biome& get(const std::string& id) const;   // throws std::out_of_range if absent
    const Biome* find(const std::string& id) const;   // nullptr if absent
    std::size_t size() const { return m_biomes.size(); }
    const std::map<std::string, Biome>& all() const { return m_biomes; }

private:
    std::map<std::string, Biome> m_biomes;
};

} // namespace mc::biome

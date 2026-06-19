#pragma once

// Port of the runtime biome data model (net.minecraft.world.level.biome.Biome
// and its sub-records) as loaded from the 26.1.2 worldgen registry JSON
// (data/minecraft/worldgen/biome/*.json). This is the data the game itself
// deserialises at runtime; the C++ loader (BiomeRegistry) reads the same files.
//
// 26.1.2 note: most colour/audio/gameplay knobs moved out of `effects` into a
// new biome `attributes` map (keys like "minecraft:visual/sky_color"). Both are
// modelled here exactly as they appear in the data.

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mc::biome {

enum class TemperatureModifier { NONE, FROZEN };
enum class GrassColorModifier { NONE, DARK_FOREST, SWAMP };

// Climate.ClimateSettings (flattened into the biome JSON top level).
struct ClimateSettings {
    bool hasPrecipitation = false;
    float temperature = 0.0f;
    TemperatureModifier temperatureModifier = TemperatureModifier::NONE;
    float downfall = 0.0f;
};

// BiomeSpecialEffects (the `effects` object). In 26.1.2 this only carries the
// colour-grading fields; everything else moved to `attributes`.
struct SpecialEffects {
    std::uint32_t waterColor = 0;                 // always present (0xRRGGBB)
    std::optional<std::uint32_t> foliageColor;
    std::optional<std::uint32_t> grassColor;
    std::optional<std::uint32_t> dryFoliageColor;
    GrassColorModifier grassColorModifier = GrassColorModifier::NONE;
};

struct AmbientParticle {
    std::string type;            // e.g. "minecraft:ash"
    float probability = 0.0f;
};

// The 26.1.2 biome `attributes` map. Worldgen/visual-relevant fields are
// modelled explicitly; audio entries are preserved verbatim (raw JSON text) as
// they are not needed for terrain generation.
struct BiomeAttributes {
    std::optional<std::uint32_t> skyColor;
    std::optional<std::uint32_t> fogColor;
    std::optional<std::uint32_t> waterFogColor;
    // 26.1.2 attributes can be value-modifiers ({"argument":x,"modifier":"multiply"})
    // rather than constants; water_fog_end_distance is always a modifier, kept raw.
    std::optional<std::string>   waterFogEndDistanceRaw;
    std::vector<AmbientParticle> ambientParticles;
    std::optional<bool>          canPillagerPatrolSpawn;
    std::optional<bool>          increasedFireBurnout;
    std::optional<bool>          snowGolemMelts;
    std::optional<float>         musicVolume;
    std::optional<std::string>   ambientSoundsRaw;     // "minecraft:audio/ambient_sounds"
    std::optional<std::string>   backgroundMusicRaw;   // "minecraft:audio/background_music"
};

// One MobSpawnSettings.SpawnerData entry.
struct SpawnerData {
    std::string type;            // entity id, e.g. "minecraft:sheep"
    int weight = 0;
    int minCount = 0;
    int maxCount = 0;
};

struct MobSpawnCost {
    double energyBudget = 0.0;
    double charge = 0.0;
};

// MobSpawnSettings (flattened: creature_spawn_probability + spawners +
// spawn_costs). Spawners are keyed by their serialised category name
// (monster, creature, ambient, axolotls, underground_water_creature,
// water_creature, water_ambient, misc) to match the JSON exactly.
struct MobSpawnSettings {
    float creatureGenerationProbability = 0.1f;
    std::map<std::string, std::vector<SpawnerData>> spawners;
    std::map<std::string, MobSpawnCost> mobSpawnCosts;
};

// BiomeGenerationSettings: the configured carvers and the per-decoration-step
// placed-feature lists. `features[step][i]` is a placed_feature id; the outer
// index is GenerationStep.Decoration.ordinal() (0..10).
struct GenerationSettings {
    std::vector<std::string> carvers;
    std::vector<std::vector<std::string>> features;
};

struct Biome {
    std::string id;              // "minecraft:plains"
    ClimateSettings climate;
    SpecialEffects effects;
    BiomeAttributes attributes;
    GenerationSettings generation;
    MobSpawnSettings mobSpawns;
};

} // namespace mc::biome

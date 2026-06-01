#include "BiomeRegistry.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mc::biome {
namespace {

using nlohmann::json;

std::uint32_t parseColor(const json& v) {
    if (v.is_number_integer()) {
        return static_cast<std::uint32_t>(v.get<std::int64_t>());
    }
    std::string s = v.get<std::string>();
    if (!s.empty() && s.front() == '#') {
        s.erase(s.begin());
    }
    return static_cast<std::uint32_t>(std::stoul(s, nullptr, 16));
}

TemperatureModifier parseTemperatureModifier(const std::string& s) {
    if (s == "frozen") return TemperatureModifier::FROZEN;
    return TemperatureModifier::NONE;
}

GrassColorModifier parseGrassColorModifier(const std::string& s) {
    if (s == "dark_forest") return GrassColorModifier::DARK_FOREST;
    if (s == "swamp") return GrassColorModifier::SWAMP;
    return GrassColorModifier::NONE;
}

// `carvers` / a feature step may be a single id, an inline list of ids, or a
// "#tag". The 65 vanilla overworld/nether/end biomes always use inline lists of
// ids; we still accept a bare string for robustness.
void readIdList(const json& v, std::vector<std::string>& out) {
    if (v.is_array()) {
        for (const auto& e : v) {
            out.push_back(e.get<std::string>());
        }
    } else if (v.is_string()) {
        out.push_back(v.get<std::string>());
    }
}

} // namespace

Biome BiomeRegistry::parseBiome(const std::string& id, const std::string& jsonText) {
    const json j = json::parse(jsonText);
    Biome b;
    b.id = id;

    // ── Climate (flattened) ───────────────────────────────────────────────
    b.climate.hasPrecipitation = j.at("has_precipitation").get<bool>();
    b.climate.temperature = j.at("temperature").get<float>();
    b.climate.downfall = j.at("downfall").get<float>();
    if (j.contains("temperature_modifier")) {
        b.climate.temperatureModifier = parseTemperatureModifier(j.at("temperature_modifier").get<std::string>());
    }

    // ── Special effects (colour grading) ──────────────────────────────────
    const json& e = j.at("effects");
    b.effects.waterColor = parseColor(e.at("water_color"));
    if (e.contains("foliage_color")) b.effects.foliageColor = parseColor(e.at("foliage_color"));
    if (e.contains("grass_color")) b.effects.grassColor = parseColor(e.at("grass_color"));
    if (e.contains("dry_foliage_color")) b.effects.dryFoliageColor = parseColor(e.at("dry_foliage_color"));
    if (e.contains("grass_color_modifier")) {
        b.effects.grassColorModifier = parseGrassColorModifier(e.at("grass_color_modifier").get<std::string>());
    }

    // ── 26.1.2 attributes map ─────────────────────────────────────────────
    if (j.contains("attributes")) {
        for (const auto& [key, val] : j.at("attributes").items()) {
            if (key == "minecraft:visual/sky_color") b.attributes.skyColor = parseColor(val);
            else if (key == "minecraft:visual/fog_color") b.attributes.fogColor = parseColor(val);
            else if (key == "minecraft:visual/water_fog_color") b.attributes.waterFogColor = parseColor(val);
            else if (key == "minecraft:visual/water_fog_end_distance") b.attributes.waterFogEndDistanceRaw = val.dump();
            else if (key == "minecraft:visual/ambient_particles") {
                for (const auto& p : val) {
                    AmbientParticle ap;
                    ap.type = p.at("particle").at("type").get<std::string>();
                    ap.probability = p.at("probability").get<float>();
                    b.attributes.ambientParticles.push_back(ap);
                }
            } else if (key == "minecraft:gameplay/can_pillager_patrol_spawn") b.attributes.canPillagerPatrolSpawn = val.get<bool>();
            else if (key == "minecraft:gameplay/increased_fire_burnout") b.attributes.increasedFireBurnout = val.get<bool>();
            else if (key == "minecraft:gameplay/snow_golem_melts") b.attributes.snowGolemMelts = val.get<bool>();
            else if (key == "minecraft:audio/music_volume") b.attributes.musicVolume = val.get<float>();
            else if (key == "minecraft:audio/ambient_sounds") b.attributes.ambientSoundsRaw = val.dump();
            else if (key == "minecraft:audio/background_music") b.attributes.backgroundMusicRaw = val.dump();
        }
    }

    // ── Generation settings: carvers + per-step placed features ────────────
    readIdList(j.at("carvers"), b.generation.carvers);
    for (const auto& step : j.at("features")) {
        std::vector<std::string> stepFeatures;
        readIdList(step, stepFeatures);
        b.generation.features.push_back(std::move(stepFeatures));
    }

    // ── Mob spawn settings (flattened) ────────────────────────────────────
    if (j.contains("creature_spawn_probability")) {
        b.mobSpawns.creatureGenerationProbability = j.at("creature_spawn_probability").get<float>();
    }
    for (const auto& [category, arr] : j.at("spawners").items()) {
        std::vector<SpawnerData> entries;
        for (const auto& s : arr) {
            SpawnerData d;
            d.type = s.at("type").get<std::string>();
            d.weight = s.at("weight").get<int>();
            d.minCount = s.at("minCount").get<int>();
            d.maxCount = s.at("maxCount").get<int>();
            entries.push_back(d);
        }
        b.mobSpawns.spawners[category] = std::move(entries);
    }
    for (const auto& [entity, cost] : j.at("spawn_costs").items()) {
        MobSpawnCost c;
        c.energyBudget = cost.at("energy_budget").get<double>();
        c.charge = cost.at("charge").get<double>();
        b.mobSpawns.mobSpawnCosts[entity] = c;
    }

    return b;
}

BiomeRegistry BiomeRegistry::loadFromDirectory(const std::string& dir) {
    namespace fs = std::filesystem;
    BiomeRegistry registry;
    if (!fs::is_directory(dir)) {
        throw std::runtime_error("biome directory not found: " + dir);
    }
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }
        std::ifstream in(entry.path());
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string id = "minecraft:" + entry.path().stem().string();
        registry.add(parseBiome(id, ss.str()));
    }
    return registry;
}

void BiomeRegistry::add(Biome biome) {
    const std::string id = biome.id;
    m_biomes[id] = std::move(biome);
}

bool BiomeRegistry::contains(const std::string& id) const {
    return m_biomes.find(id) != m_biomes.end();
}

const Biome& BiomeRegistry::get(const std::string& id) const {
    return m_biomes.at(id);
}

const Biome* BiomeRegistry::find(const std::string& id) const {
    auto it = m_biomes.find(id);
    return it == m_biomes.end() ? nullptr : &it->second;
}

} // namespace mc::biome

// Parity test for the biome registry loader (mc::biome::BiomeRegistry).
//
// Ground truth is the canonical 26.1.2 worldgen data
// (data/minecraft/worldgen/biome/*.json). Two layers:
//   * default `[dir]`: load all biomes, assert count and hardcoded spot-checks
//     for representative biomes (climate, colours, carvers, feature steps,
//     spawners) plus structural invariants.
//   * `--dump <dir>`: emit a normalised, deterministic text view of every biome
//     so it can be diffed against an independent parser (see
//     tools/biome_registry_reference.py) to validate field mapping end-to-end.

#include "BiomeRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::biome;

namespace {

bool g_ok = true;
void check(bool cond, const std::string& label) {
    if (!cond) {
        g_ok = false;
        std::cerr << "FAIL: " << label << '\n';
    }
}
void checkNear(double a, double b, const std::string& label) {
    check(std::fabs(a - b) < 1e-6, label + " (got " + std::to_string(a) + ")");
}

std::string colorHex(std::uint32_t c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%06x", c & 0xFFFFFFu);
    return buf;
}
std::string optColor(const std::optional<std::uint32_t>& c) { return c ? colorHex(*c) : "-"; }

const char* tempModName(TemperatureModifier m) { return m == TemperatureModifier::FROZEN ? "frozen" : "none"; }
const char* grassModName(GrassColorModifier m) {
    switch (m) {
        case GrassColorModifier::DARK_FOREST: return "dark_forest";
        case GrassColorModifier::SWAMP: return "swamp";
        default: return "none";
    }
}

std::string join(const std::vector<std::string>& v, char sep = ',') {
    std::string out;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

// Deterministic, high-precision normalised dump for cross-validation.
void dump(const BiomeRegistry& reg) {
    for (const auto& [id, b] : reg.all()) {
        std::printf("%s\tCLIMATE\t%d\t%.9g\t%.9g\t%s\n", id.c_str(),
                    b.climate.hasPrecipitation ? 1 : 0, b.climate.temperature, b.climate.downfall,
                    tempModName(b.climate.temperatureModifier));
        std::printf("%s\tEFFECTS\t%s\t%s\t%s\t%s\t%s\n", id.c_str(), colorHex(b.effects.waterColor).c_str(),
                    optColor(b.effects.foliageColor).c_str(), optColor(b.effects.grassColor).c_str(),
                    optColor(b.effects.dryFoliageColor).c_str(), grassModName(b.effects.grassColorModifier));
        std::string particles;
        for (const auto& p : b.attributes.ambientParticles) {
            if (!particles.empty()) particles += ',';
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s:%.9g", p.type.c_str(), p.probability);
            particles += buf;
        }
        std::string wfogEnd = b.attributes.waterFogEndDistanceRaw ? *b.attributes.waterFogEndDistanceRaw : "-";
        std::printf("%s\tATTR\tsky=%s\tfog=%s\twfog=%s\twfogEnd=%s\tparticles=%s\n", id.c_str(),
                    optColor(b.attributes.skyColor).c_str(), optColor(b.attributes.fogColor).c_str(),
                    optColor(b.attributes.waterFogColor).c_str(), wfogEnd.c_str(),
                    particles.empty() ? "-" : particles.c_str());
        std::printf("%s\tCARVERS\t%s\n", id.c_str(), join(b.generation.carvers).c_str());
        for (std::size_t step = 0; step < b.generation.features.size(); ++step) {
            std::printf("%s\tFEAT\t%zu\t%s\n", id.c_str(), step, join(b.generation.features[step]).c_str());
        }
        for (const auto& [cat, entries] : b.mobSpawns.spawners) {
            std::string s;
            for (const auto& e : entries) {
                if (!s.empty()) s += ',';
                char buf[96];
                std::snprintf(buf, sizeof(buf), "%s:%d:%d:%d", e.type.c_str(), e.weight, e.minCount, e.maxCount);
                s += buf;
            }
            std::printf("%s\tSPAWN\t%s\t%s\n", id.c_str(), cat.c_str(), s.empty() ? "-" : s.c_str());
        }
        std::printf("%s\tSPAWNPROB\t%.9g\n", id.c_str(), b.mobSpawns.creatureGenerationProbability);
        for (const auto& [entity, cost] : b.mobSpawns.mobSpawnCosts) {
            std::printf("%s\tCOST\t%s\t%.9g\t%.9g\n", id.c_str(), entity.c_str(), cost.energyBudget, cost.charge);
        }
    }
}

void spotChecks(const BiomeRegistry& reg) {
    check(reg.size() == 65, "biome count == 65 (got " + std::to_string(reg.size()) + ")");

    // Structural invariant: a biome may have 0 feature steps (e.g. end_barrens),
    // but when present each step list is well-formed and the registry is keyed by
    // a namespaced id.
    for (const auto& [id, b] : reg.all()) {
        check(id.rfind("minecraft:", 0) == 0, id + " is namespaced");
        check(b.id == id, id + " id matches key");
    }

    // plains
    {
        const Biome& p = reg.get("minecraft:plains");
        check(p.climate.hasPrecipitation, "plains has_precipitation");
        checkNear(p.climate.temperature, 0.8, "plains temperature");
        checkNear(p.climate.downfall, 0.4, "plains downfall");
        check(p.climate.temperatureModifier == TemperatureModifier::NONE, "plains temp modifier none");
        check(p.effects.waterColor == 0x3f76e4, "plains water_color");
        check(p.attributes.skyColor.has_value() && *p.attributes.skyColor == 0x78a7ff, "plains sky_color");
        check(p.generation.carvers == std::vector<std::string>({ "minecraft:cave", "minecraft:cave_extra_underground", "minecraft:canyon" }), "plains carvers");
        check(p.generation.features.size() == 11, "plains 11 feature steps");
        // vegetal_decoration step (index 9) contains trees_plains + flowers
        const auto& veg = p.generation.features.at(9);
        check(std::find(veg.begin(), veg.end(), "minecraft:trees_plains") != veg.end(), "plains has trees_plains");
        check(std::find(veg.begin(), veg.end(), "minecraft:flower_plains") != veg.end(), "plains has flower_plains");
    }
    // swamp: grass_color_modifier = swamp, foliage + dry_foliage colours
    {
        const Biome& s = reg.get("minecraft:swamp");
        check(s.effects.grassColorModifier == GrassColorModifier::SWAMP, "swamp grass_color_modifier");
        check(s.effects.foliageColor.has_value() && *s.effects.foliageColor == 0x6a7039, "swamp foliage_color");
        check(s.effects.waterColor == 0x617b64, "swamp water_color");
    }
    // frozen_ocean: temperature_modifier = frozen
    {
        const Biome& f = reg.get("minecraft:frozen_ocean");
        check(f.climate.temperatureModifier == TemperatureModifier::FROZEN, "frozen_ocean temp modifier frozen");
    }
    // dark_forest: grass_color_modifier = dark_forest
    {
        const Biome& d = reg.get("minecraft:dark_forest");
        check(d.effects.grassColorModifier == GrassColorModifier::DARK_FOREST, "dark_forest grass_color_modifier");
    }
    // nether_wastes: ambient particles? no; but has ambient sounds + spawn costs
    {
        const Biome& n = reg.get("minecraft:nether_wastes");
        check(!n.climate.hasPrecipitation, "nether_wastes no precipitation");
        check(n.attributes.ambientSoundsRaw.has_value(), "nether_wastes ambient sounds");
    }
    // soul_sand_valley: ambient ash particles + spawn costs
    {
        const Biome& s = reg.get("minecraft:soul_sand_valley");
        check(!s.attributes.ambientParticles.empty(), "soul_sand_valley has ambient particles");
        check(s.attributes.ambientParticles.front().type == "minecraft:ash", "soul_sand_valley ash particle");
        check(s.mobSpawns.mobSpawnCosts.count("minecraft:skeleton") == 1, "soul_sand_valley skeleton spawn cost");
        checkNear(s.mobSpawns.mobSpawnCosts.at("minecraft:skeleton").charge, 0.7, "soul_sand_valley skeleton charge");
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string dir = "26.1.2/data/minecraft/worldgen/biome";
    bool dumpMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--dump") dumpMode = true;
        else dir = a;
    }

    BiomeRegistry reg;
    try {
        reg = BiomeRegistry::loadFromDirectory(dir);
    } catch (const std::exception& ex) {
        std::cerr << "load error: " << ex.what() << '\n';
        return 2;
    }

    if (dumpMode) {
        dump(reg);
        return 0;
    }

    spotChecks(reg);
    if (!g_ok) {
        std::cerr << "Biome registry parity checks FAILED\n";
        return 1;
    }
    std::cout << "Biome registry parity checks passed (" << reg.size() << " biomes)\n";
    return 0;
}

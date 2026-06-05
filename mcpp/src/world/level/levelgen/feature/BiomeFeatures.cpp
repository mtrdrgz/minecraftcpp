#include "BiomeFeatures.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mc::levelgen::feature {

namespace {
std::string normalizeId(std::string id) {
    if (id.find(':') == std::string::npos) id = "minecraft:" + id;
    return id;
}

std::string stemFromPath(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t begin = slash == std::string::npos ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    const std::size_t end = dot == std::string::npos || dot < begin ? path.size() : dot;
    return path.substr(begin, end - begin);
}

bool parseBiomeFeatures(const std::string& text,
                        std::array<std::vector<std::string>, GenerationStep::COUNT>& steps) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(text);
    } catch (const std::exception&) {
        return false;
    }
    const auto features = j.find("features");
    if (features == j.end() || !features->is_array()) return false;

    int step = 0;
    for (const auto& stepArr : *features) {
        if (step >= GenerationStep::COUNT) break;
        if (stepArr.is_array()) {
            for (const auto& v : stepArr) {
                if (v.is_string()) steps[step].push_back(normalizeId(v.get<std::string>()));
            }
        }
        ++step;
    }
    return true;
}
} // namespace

BiomeFeatures BiomeFeatures::loadFromDirectory(const std::string& dir) {
    namespace fs = std::filesystem;
    BiomeFeatures out;
    if (!fs::is_directory(dir)) {
        throw std::runtime_error("biome directory not found: " + dir);
    }
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        std::ifstream in(entry.path());
        std::stringstream ss;
        ss << in.rdbuf();

        const std::string biome = "minecraft:" + entry.path().stem().string();
        auto& steps = out.m_biomes[biome];
        if (!parseBiomeFeatures(ss.str(), steps)) {
            out.m_biomes.erase(biome);
        }
    }
    return out;
}

BiomeFeatures BiomeFeatures::loadFromJsonEntries(const std::vector<std::pair<std::string, std::string>>& entries) {
    BiomeFeatures out;
    for (const auto& [path, text] : entries) {
        if (!path.ends_with(".json")) {
            continue;
        }
        const std::string biome = "minecraft:" + stemFromPath(path);
        auto& steps = out.m_biomes[biome];
        if (!parseBiomeFeatures(text, steps)) {
            out.m_biomes.erase(biome);
        }
    }
    return out;
}

bool BiomeFeatures::hasBiome(const std::string& biome) const {
    return m_biomes.find(biome) != m_biomes.end();
}

const std::vector<std::string>& BiomeFeatures::featuresForStep(const std::string& biome, int step) const {
    static const std::vector<std::string> empty;
    auto it = m_biomes.find(biome);
    if (it == m_biomes.end() || step < 0 || step >= GenerationStep::COUNT) return empty;
    return it->second[step];
}

bool BiomeFeatures::biomeHasFeature(const std::string& biome, int step, const std::string& featureKey) const {
    const auto& list = featuresForStep(biome, step);
    for (const auto& k : list) {
        if (k == featureKey) return true;
    }
    return false;
}

} // namespace mc::levelgen::feature

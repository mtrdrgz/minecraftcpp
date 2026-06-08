#include "BiomeFeatures.h"

#include <nlohmann/json.hpp>

#include <algorithm>
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
                        std::array<std::vector<std::string>, GenerationStep::COUNT>& steps,
                        int& stepCount) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(text);
    } catch (const std::exception&) {
        return false;
    }
    const auto features = j.find("features");
    if (features == j.end() || !features->is_array()) return false;

    stepCount = std::min(static_cast<int>(features->size()), static_cast<int>(GenerationStep::COUNT));
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
    if (!fs::is_directory(dir)) {
        throw std::runtime_error("biome directory not found: " + dir);
    }

    BiomeFeatures out;
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const fs::path& file : files) {
        std::ifstream in(file);
        if (!in) continue;
        std::ostringstream ss;
        ss << in.rdbuf();

        BiomeEntry biome;
        if (!parseBiomeFeatures(ss.str(), biome.steps, biome.stepCount)) {
            continue;
        }

        fs::path rel = fs::relative(file, dir);
        std::string id = rel.generic_string();
        if (id.ends_with(".json")) id.resize(id.size() - 5);
        out.m_biomes.emplace(normalizeId(id), std::move(biome));
    }
    return out;
}

BiomeFeatures BiomeFeatures::loadFromJsonEntries(const std::vector<std::pair<std::string, std::string>>& entries) {
    BiomeFeatures out;
    for (const auto& [path, text] : entries) {
        BiomeEntry biome;
        if (!parseBiomeFeatures(text, biome.steps, biome.stepCount)) {
            continue;
        }

        std::string id = path;
        constexpr std::string_view prefix = "data/minecraft/worldgen/biome/";
        if (id.rfind(prefix, 0) == 0) {
            id.erase(0, prefix.size());
        }
        if (id.ends_with(".json")) id.resize(id.size() - 5);
        out.m_biomes.emplace(normalizeId(id), std::move(biome));
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
    return it->second.steps[step];
}

bool BiomeFeatures::biomeHasFeature(const std::string& biome, int step, const std::string& featureKey) const {
    const auto& list = featuresForStep(biome, step);
    for (const auto& k : list) {
        if (k == featureKey) return true;
    }
    return false;
}

int BiomeFeatures::stepCountForBiome(const std::string& biome) const {
    auto it = m_biomes.find(biome);
    return it == m_biomes.end() ? 0 : it->second.stepCount;
}

} // namespace mc::levelgen::feature

#include "BiomeDecorator.h"

#include "FeatureSorter.h"
#include "TreeGen.h"
#include "../../block/Blocks.h"
#include "../../block/BlockState.h"
#include "../BiomeSource.h"
#include "../RandomSource.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mc::levelgen::feature {

namespace {
JsonAssetReader& jsonAssetReader() {
    static JsonAssetReader reader;
    return reader;
}

const std::vector<FeatureSorter::StepFeatureData>& overworldFeaturesPerStep(const BiomeFeatures& biomeFeatures) {
    static const BiomeFeatures* cachedBiomeFeatures = nullptr;
    static std::vector<FeatureSorter::StepFeatureData> cachedFeatures;
    if (cachedBiomeFeatures != &biomeFeatures) {
        cachedFeatures = FeatureSorter::buildFeaturesPerStep(
            mc::levelgen::BiomeSource::collectOverworldPossibleBiomes(),
            biomeFeatures,
            true);
        cachedBiomeFeatures = &biomeFeatures;
    }
    return cachedFeatures;
}

std::optional<std::string> readJson(const std::string& worldgenDir, const std::string& rel) {
    const std::filesystem::path fsPath = std::filesystem::path(worldgenDir) / rel;
    std::ifstream in(fsPath);
    if (in) {
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
    if (jsonAssetReader()) {
        std::string assetPath = "data/minecraft/worldgen/" + rel;
        std::replace(assetPath.begin(), assetPath.end(), '\\', '/');
        return jsonAssetReader()(assetPath);
    }
    return std::nullopt;
}

std::string keyToPathStem(std::string key) {
    if (key.starts_with("minecraft:")) key.erase(0, 10);
    return key;
}

int sampleIntProvider(const nlohmann::json& value, WorldgenRandom& rng) {
    if (value.is_number_integer()) {
        return value.get<int>();
    }
    if (!value.is_object()) {
        return 0;
    }
    const std::string type = value.value("type", "");
    if (type == "minecraft:constant") {
        return value.value("value", 0);
    }
    if (type == "minecraft:uniform") {
        const int lo = value.value("min_inclusive", 0);
        const int hi = value.value("max_inclusive", lo);
        return lo + rng.nextInt(std::max(hi - lo + 1, 1));
    }
    if (type == "minecraft:clamped") {
        const int sampled = sampleIntProvider(value.at("source"), rng);
        const int lo = value.value("min_inclusive", sampled);
        const int hi = value.value("max_inclusive", sampled);
        return std::clamp(sampled, lo, hi);
    }
    if (type == "minecraft:weighted_list") {
        const auto& dist = value.at("distribution");
        int totalWeight = 0;
        for (const auto& entry : dist) totalWeight += entry.value("weight", 0);
        if (totalWeight <= 0) return 0;
        int roll = rng.nextInt(totalWeight);
        for (const auto& entry : dist) {
            roll -= entry.value("weight", 0);
            if (roll < 0) return sampleIntProvider(entry.at("data"), rng);
        }
    }
    return 0;
}

int heightAt(LevelChunk& chunk, int wx, int wz) {
    const int lx = ((wx % 16) + 16) % 16;
    const int lz = ((wz % 16) + 16) % 16;
    return chunk.heightmap(lx, lz) + 1;
}

bool surfaceWaterDepthOk(LevelChunk& chunk, int wx, int wz, int maxWaterDepth) {
    const int top = heightAt(chunk, wx, wz) - 1;
    int oceanFloor = top;
    while (oceanFloor > CHUNK_MIN_Y) {
        const uint32_t s = chunk.getBlock(wx, oceanFloor, wz);
        const BlockState* bs = getBlockState(s);
        if (!bs || !bs->isFluid()) break;
        --oceanFloor;
    }
    return top - oceanFloor <= maxWaterDepth;
}

bool wouldTreeSaplingSurvive(LevelChunk& chunk, int wx, int wy, int wz) {
    const uint32_t below = chunk.getBlock(wx, wy - 1, wz);
    const BlockState* bs = getBlockState(below);
    if (!bs || !bs->block) return false;
    const std::string& name = bs->block->name;
    return name == "grass_block" || name == "dirt" || name == "coarse_dirt" || name == "podzol" ||
           name == "rooted_dirt" || name == "moss_block" || name == "mud" || name == "farmland";
}

TreeConfig* builtinTreeConfigForConfiguredFeature(const std::string& key) {
    static TreeConfig oak = makeOakConfig();
    static TreeConfig fancyOak = makeFancyOakConfig();
    static TreeConfig birch = makeBirchConfig();
    static TreeConfig spruce = makeSpruceConfig();
    static TreeConfig pine = makePineConfig();
    static TreeConfig acacia = makeAcaciaConfig();

    if (key == "minecraft:oak" || key == "minecraft:oak_checked" || key.starts_with("minecraft:oak_bees_")) return &oak;
    if (key == "minecraft:fancy_oak" || key == "minecraft:fancy_oak_checked" || key.starts_with("minecraft:fancy_oak_bees_")) return &fancyOak;
    if (key == "minecraft:birch" || key == "minecraft:birch_checked" || key.starts_with("minecraft:birch_bees_")) return &birch;
    if (key == "minecraft:spruce" || key == "minecraft:spruce_checked" || key == "minecraft:spruce_on_snow") return &spruce;
    if (key == "minecraft:pine" || key == "minecraft:pine_checked" || key == "minecraft:pine_on_snow") return &pine;
    if (key == "minecraft:acacia" || key == "minecraft:acacia_checked") return &acacia;
    return nullptr;
}

std::string featureKeyFromPlacedFeatureValue(const nlohmann::json& featureValue) {
    if (featureValue.is_string()) return featureValue.get<std::string>();
    if (featureValue.is_object()) {
        const auto it = featureValue.find("feature");
        if (it != featureValue.end() && it->is_string()) return it->get<std::string>();
    }
    return {};
}

TreeConfig* selectTreeConfigFromConfiguredFeature(const std::string& worldgenDir, const std::string& configuredKey,
                                                  WorldgenRandom& rng) {
    if (TreeConfig* direct = builtinTreeConfigForConfiguredFeature(configuredKey)) return direct;

    const auto text = readJson(worldgenDir, "configured_feature/" + keyToPathStem(configuredKey) + ".json");
    if (!text) return nullptr;

    nlohmann::json j;
    try { j = nlohmann::json::parse(*text); } catch (...) { return nullptr; }
    const std::string type = j.value("type", "");
    if (type == "minecraft:tree") {
        return builtinTreeConfigForConfiguredFeature(configuredKey);
    }
    if (type != "minecraft:random_selector") return nullptr;

    const auto& cfg = j.at("config");
    for (const auto& weighted : cfg.value("features", nlohmann::json::array())) {
        const float chance = weighted.value("chance", 0.0f);
        if (rng.nextFloat() < chance) {
            const std::string selected = featureKeyFromPlacedFeatureValue(weighted.at("feature"));
            return builtinTreeConfigForConfiguredFeature(selected);
        }
    }
    return builtinTreeConfigForConfiguredFeature(featureKeyFromPlacedFeatureValue(cfg.at("default")));
}

bool applyPlacementModifiers(LevelChunk& chunk,
                             const std::function<std::string(int, int, int)>& biomeGetter,
                             const BiomeFeatures& biomeFeatures,
                             const nlohmann::json& placed,
                             const std::string& placedFeatureKey,
                             int stepIndex,
                             WorldgenRandom& rng,
                             std::vector<std::array<int, 3>>& positions) {
    for (const auto& modifier : placed.at("placement")) {
        const std::string type = modifier.value("type", "");
        std::vector<std::array<int, 3>> out;
        if (type == "minecraft:count") {
            const int count = sampleIntProvider(modifier.at("count"), rng);
            for (const auto& p : positions) for (int i = 0; i < count; ++i) out.push_back(p);
        } else if (type == "minecraft:rarity_filter") {
            const int chance = modifier.value("chance", 1);
            for (const auto& p : positions) if (chance <= 1 || rng.nextInt(chance) == 0) out.push_back(p);
        } else if (type == "minecraft:in_square") {
            for (const auto& p : positions) out.push_back({p[0] + rng.nextInt(16), p[1], p[2] + rng.nextInt(16)});
        } else if (type == "minecraft:heightmap") {
            for (const auto& p : positions) {
                const int y = heightAt(chunk, p[0], p[2]);
                if (y > CHUNK_MIN_Y) out.push_back({p[0], y, p[2]});
            }
        } else if (type == "minecraft:surface_water_depth_filter") {
            const int maxDepth = modifier.value("max_water_depth", 0);
            for (const auto& p : positions) if (surfaceWaterDepthOk(chunk, p[0], p[2], maxDepth)) out.push_back(p);
        } else if (type == "minecraft:block_predicate_filter") {
            for (const auto& p : positions) if (wouldTreeSaplingSurvive(chunk, p[0], p[1], p[2])) out.push_back(p);
        } else if (type == "minecraft:biome") {
            for (const auto& p : positions) {
                const std::string biome = biomeGetter(p[0], p[1], p[2]);
                if (biomeFeatures.biomeHasFeature(biome, stepIndex, placedFeatureKey)) out.push_back(p);
            }
        } else {
            return false;
        }
        positions.swap(out);
        if (positions.empty()) return true;
    }
    return true;
}

int placeTreePlacedFeature(LevelChunk& chunk, std::int64_t worldSeed,
                           const std::function<std::string(int, int, int)>& biomeGetter,
                           const BiomeFeatures& biomeFeatures,
                           const std::string& worldgenDir,
                           const std::function<LevelChunk*(int, int)>& chunkAt,
                           const std::string& placedFeatureKey,
                           int globalFeatureIndex,
                           int stepIndex) {
    const auto placedText = readJson(worldgenDir, "placed_feature/" + keyToPathStem(placedFeatureKey) + ".json");
    if (!placedText) return 0;
    nlohmann::json placed;
    try { placed = nlohmann::json::parse(*placedText); } catch (...) { return 0; }

    const ChunkPos pos = chunk.pos();
    const int minX = pos.x * 16;
    const int minZ = pos.z * 16;

    WorldgenRandom rng(RandomSource::create(0));
    const std::int64_t decorationSeed = rng.setDecorationSeed(worldSeed, minX, minZ);
    rng.setFeatureSeed(decorationSeed, globalFeatureIndex, stepIndex);

    std::vector<std::array<int, 3>> positions;
    positions.push_back({minX, 0, minZ});
    if (!applyPlacementModifiers(chunk, biomeGetter, biomeFeatures, placed, placedFeatureKey, stepIndex, rng, positions)) {
        return 0;
    }
    if (positions.empty()) return 0;

    TreeWorld world{chunk, minX, minZ, &chunkAt};
    const std::string configured = featureKeyFromPlacedFeatureValue(placed.at("feature"));
    int placedCount = 0;
    for (const auto& p : positions) {
        TreeConfig* cfg = selectTreeConfigFromConfiguredFeature(worldgenDir, configured, rng);
        if (!cfg) continue;
        if (placeTree(world, rng, p[0], p[1], p[2], *cfg)) ++placedCount;
    }
    if (placedCount > 0) chunk.computeHeightmap();
    return placedCount;
}

const std::array<std::string_view, 12>& supportedTreePlacedFeatureKeys() {
    static constexpr std::array<std::string_view, 12> keys{
        "minecraft:trees_plains",
        "minecraft:trees_flower_forest",
        "minecraft:trees_taiga",
        "minecraft:trees_grove",
        "minecraft:trees_snowy",
        "minecraft:trees_savanna",
        "minecraft:trees_windswept_savanna",
        "minecraft:trees_birch",
        "minecraft:trees_windswept_forest",
        "minecraft:trees_windswept_hills",
        "minecraft:trees_water",
        "minecraft:trees_old_growth_pine_taiga"
    };
    return keys;
}
} // namespace

void setJsonAssetReader(JsonAssetReader reader) {
    jsonAssetReader() = std::move(reader);
}

void applyBiomeDecoration(LevelChunk& chunk, std::int64_t worldSeed,
                          const std::function<std::string(int, int, int)>& biomeGetter,
                          const BiomeFeatures& biomeFeatures,
                          const mc::block::BlockTags& tags,
                          const std::string& worldgenDir,
                          const std::function<LevelChunk*(int, int)>& chunkAt) {
    (void)tags;
    const auto& features = overworldFeaturesPerStep(biomeFeatures);
    constexpr int step = GenerationStep::VEGETAL_DECORATION;
    int placedAny = 0;
    if (step < static_cast<int>(features.size())) {
        for (std::string_view keyView : supportedTreePlacedFeatureKeys()) {
            const std::string key(keyView);
            const int index = features[step].indexOf(key);
            if (index >= 0) {
                placedAny += placeTreePlacedFeature(chunk, worldSeed, biomeGetter, biomeFeatures, worldgenDir, chunkAt, key, index, step);
            }
        }
    }
    (void)placedAny;
    chunk.decorated = true;
}

} // namespace mc::levelgen::feature

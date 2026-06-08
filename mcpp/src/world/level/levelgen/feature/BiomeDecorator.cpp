#include "BiomeDecorator.h"

#include "FeatureSorter.h"
#include "../BiomeSource.h"

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
    (void)worldSeed;
    (void)biomeGetter;
    (void)tags;
    (void)worldgenDir;
    (void)chunkAt;

    // Feature placement is intentionally disabled until PlacedFeature and all of
    // its modifiers/configured-feature types are ported. The old runtime path still
    // scanned the full 3x3 chunk biome volume per decorated chunk just to compute
    // selected indices that were never placed. That made Singleplayer entry very
    // slow for no visible result. Keep the vanilla global FeatureSorter cache warm,
    // but skip per-chunk biome scanning until actual placement is implemented.
    (void)overworldFeaturesPerStep(biomeFeatures);

    chunk.decorated = true;
}

} // namespace mc::levelgen::feature

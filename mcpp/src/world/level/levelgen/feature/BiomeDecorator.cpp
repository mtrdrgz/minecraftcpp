#include "BiomeDecorator.h"

#include "FeatureSorter.h"
#include "../BiomeSource.h"

#include <set>
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

std::vector<std::string> collectPossibleBiomesAroundChunk(
    const LevelChunk& chunk,
    const std::function<std::string(int, int, int)>& biomeGetter) {
    std::set<std::string> found;
    const ChunkPos center = chunk.pos();

    for (int chunkZ = center.z - 1; chunkZ <= center.z + 1; ++chunkZ) {
        for (int chunkX = center.x - 1; chunkX <= center.x + 1; ++chunkX) {
            for (int sectionY = 0; sectionY < CHUNK_SECTION_COUNT; ++sectionY) {
                const int baseY = CHUNK_MIN_Y + sectionY * 16;
                for (int qz = 0; qz < 4; ++qz) {
                    for (int qy = 0; qy < 4; ++qy) {
                        for (int qx = 0; qx < 4; ++qx) {
                            const std::string biome = biomeGetter(
                                chunkX * 16 + qx * 4,
                                baseY + qy * 4,
                                chunkZ * 16 + qz * 4);
                            if (!biome.empty()) {
                                found.insert(biome);
                            }
                        }
                    }
                }
            }
        }
    }
    return std::vector<std::string>(found.begin(), found.end());
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
    (void)tags;
    (void)worldgenDir;
    (void)chunkAt;

    const auto& featuresPerStep = overworldFeaturesPerStep(biomeFeatures);
    const std::vector<std::string> possibleBiomes = collectPossibleBiomesAroundChunk(chunk, biomeGetter);

    for (int step = 0; step < static_cast<int>(featuresPerStep.size()); ++step) {
        const std::vector<int> selectedFeatureIndices = FeatureSorter::selectFeatureIndicesForStep(
            possibleBiomes,
            biomeFeatures,
            featuresPerStep[static_cast<std::size_t>(step)],
            step);
        for (int globalIndexOfFeature : selectedFeatureIndices) {
            (void)globalIndexOfFeature;
        }
    }

    chunk.meshDirty = true;
}

} // namespace mc::levelgen::feature

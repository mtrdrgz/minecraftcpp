#pragma once

#include "DecorationPlan.h"
#include "../RandomSource.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mc::levelgen::feature {

struct DecorationSeedCall {
    int step = 0;
    int featureIndex = 0;
    std::string featureKey;

    // This is the seed value set by Java WorldgenRandom.setFeatureSeed:
    // decorationSeed + featureIndex + 10000 * step, with Java long wrapping.
    std::int64_t featureSeed = 0;
};

struct DecorationDriverResult {
    std::int64_t decorationSeed = 0;
    std::vector<FeatureSorter::StepFeatureData> featuresPerStep;
    std::vector<DecorationSeedCall> calls;
};

class DecorationDriver {
public:
    // Build the exact seed-order plan for a future ChunkGenerator.applyBiomeDecoration
    // port, without placing any blocks.
    //
    // allPossibleBiomes must correspond to Java biomeSource.possibleBiomes(). It is
    // used to build FeatureSorter.StepFeatureData globally.
    //
    // chunkPossibleBiomes must correspond to Java's possibleBiomes set collected
    // from sections in the surrounding 3x3 chunk region for the target chunk.
    //
    // originBlockX/originBlockZ must be SectionPos.of(centerChunk, minSectionY).origin()
    // coordinates; for normal chunks this is centerChunk.x * 16 and centerChunk.z * 16.
    static DecorationDriverResult buildNoopPlan(std::int64_t worldSeed,
                                                int originBlockX,
                                                int originBlockZ,
                                                const std::vector<std::string>& allPossibleBiomes,
                                                const std::vector<std::string>& chunkPossibleBiomes,
                                                const BiomeFeatures& biomeFeatures) {
        DecorationDriverResult result;
        result.featuresPerStep = FeatureSorter::buildFeaturesPerStep(allPossibleBiomes, biomeFeatures, true);

        WorldgenRandom random(RandomSource::create(0));
        result.decorationSeed = random.setDecorationSeed(worldSeed, originBlockX, originBlockZ);

        const std::vector<DecorationCall> plan = DecorationPlan::build(chunkPossibleBiomes, biomeFeatures, result.featuresPerStep);
        result.calls.reserve(plan.size());
        for (const DecorationCall& call : plan) {
            random.setFeatureSeed(result.decorationSeed, call.featureIndex, call.step);
            result.calls.push_back(DecorationSeedCall{
                call.step,
                call.featureIndex,
                call.featureKey,
                javaFeatureSeed(result.decorationSeed, call.featureIndex, call.step)
            });
        }

        return result;
    }

private:
    static std::int64_t javaFeatureSeed(std::int64_t decorationSeed, int featureIndex, int step) {
        const std::uint64_t a = static_cast<std::uint64_t>(decorationSeed);
        const std::uint64_t b = static_cast<std::uint64_t>(static_cast<std::int64_t>(featureIndex));
        const std::uint64_t c = static_cast<std::uint64_t>(static_cast<std::int64_t>(10000 * step));
        return static_cast<std::int64_t>(a + b + c);
    }
};

} // namespace mc::levelgen::feature

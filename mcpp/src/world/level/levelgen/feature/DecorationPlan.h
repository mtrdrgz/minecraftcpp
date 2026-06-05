#pragma once

#include "FeatureSorter.h"

#include <algorithm>
#include <string>
#include <vector>

namespace mc::levelgen::feature {

// Data-only representation of the feature calls that Java
// ChunkGenerator.applyBiomeDecoration would make for a chunk after possible
// biomes have been collected from the surrounding 3x3 chunk region.
//
// This does not place any block. It is the safe bridge between the current
// no-approximation runtime and the future feature ports: every entry here is a
// feature seed call that should eventually become:
//
//   random.setFeatureSeed(decorationSeed, featureIndex, step);
//   placedFeature.placeWithBiomeCheck(...);
struct DecorationCall {
    int step = 0;
    int featureIndex = 0;
    std::string featureKey;
};

class DecorationPlan {
public:
    static std::vector<DecorationCall> build(const std::vector<std::string>& possibleBiomes,
                                             const BiomeFeatures& biomeFeatures,
                                             const std::vector<FeatureSorter::StepFeatureData>& featuresPerStep) {
        std::vector<DecorationCall> out;

        for (int step = 0; step < static_cast<int>(featuresPerStep.size()); ++step) {
            const auto& stepData = featuresPerStep[static_cast<std::size_t>(step)];
            const std::vector<int> indices = FeatureSorter::selectFeatureIndicesForStep(possibleBiomes, biomeFeatures, stepData, step);
            for (const int idx : indices) {
                if (idx < 0 || idx >= static_cast<int>(stepData.features.size())) continue;
                out.push_back(DecorationCall{ step, idx, stepData.features[static_cast<std::size_t>(idx)] });
            }
        }

        return out;
    }
};

} // namespace mc::levelgen::feature

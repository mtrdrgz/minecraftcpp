#pragma once

// 1:1 ports of net.minecraft.world.level.levelgen.feature.RandomSelectorFeature
// (RandomSelectorFeature.java:16-31) and SimpleRandomSelectorFeature
// (SimpleRandomSelectorFeature.java:17-28). Both delegate to nested
// PlacedFeature.place (PlacedFeature.java:place — the NO-biome-check entry).
//
// RNG:
//   random_selector: one nextFloat per weighted entry until one passes
//     (short-circuit; the chosen/default feature then runs on the same random)
//   simple_random_selector: one nextInt(features.size()) then the chosen feature

#include "../placement/PlacementContext.h"
#include "../placement/PlacedFeature.h"
#include "../RandomSource.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace mc::levelgen::feature {

struct WeightedPlacedFeatureEntry {
    float chance = 0.0f;
    std::shared_ptr<mc::levelgen::placement::PlacedFeature> feature;
};

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeRandomSelectorPlacer(
        std::vector<WeightedPlacedFeatureEntry> features,
        std::shared_ptr<mc::levelgen::placement::PlacedFeature> defaultFeature,
        int minGenY, int genDepth) {
    return [features = std::move(features), defaultFeature = std::move(defaultFeature),
            minGenY, genDepth](WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        // Debug attempt trace (MCPP_TRACE_SELECTOR=1): origin + chosen entry +
        // result + the WorldgenRandom low-level draw counter (next() calls).
        static const bool trace = std::getenv("MCPP_TRACE_SELECTOR") != nullptr;
        auto drawCount = [&random]() -> std::int64_t {
            if (auto* wr = dynamic_cast<WorldgenRandom*>(&random)) return wr->getCount();
            return -1;
        };
        int idx = 0;
        for (const WeightedPlacedFeatureEntry& feature : features) {
            if (random.nextFloat() < feature.chance) {
                const bool ok = feature.feature->place(level, random, origin, minGenY, genDepth);
                if (trace) std::cerr << "SEL\t" << origin.x << "," << origin.y << "," << origin.z
                                     << "\tentry=" << idx << "\tok=" << ok << "\tdraws=" << drawCount() << "\n";
                return ok;
            }
            ++idx;
        }
        const bool ok = defaultFeature->place(level, random, origin, minGenY, genDepth);
        if (trace) std::cerr << "SEL\t" << origin.x << "," << origin.y << "," << origin.z
                             << "\tentry=default\tok=" << ok << "\tdraws=" << drawCount() << "\n";
        return ok;
    };
}

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeSimpleRandomSelectorPlacer(
        std::vector<std::shared_ptr<mc::levelgen::placement::PlacedFeature>> features,
        int minGenY, int genDepth) {
    return [features = std::move(features), minGenY, genDepth](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        const int index = random.nextInt(static_cast<std::int32_t>(features.size()));
        return features[static_cast<std::size_t>(index)]->place(level, random, origin, minGenY, genDepth);
    };
}

} // namespace mc::levelgen::feature

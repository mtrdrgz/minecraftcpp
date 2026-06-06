#pragma once

#include "BiomeFeatures.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mc::levelgen::feature {

class FeatureSorter {
public:
    struct StepFeatureData {
        std::vector<std::string> features;
        std::map<std::string, int> indexMapping;

        int indexOf(const std::string& feature) const {
            const auto it = indexMapping.find(feature);
            return it == indexMapping.end() ? -1 : it->second;
        }
    };

    static std::vector<StepFeatureData> buildFeaturesPerStep(const std::vector<std::string>& featureSources,
                                                              const BiomeFeatures& biomeFeatures,
                                                              bool tryReducingError = true) {
        struct FeatureData {
            int featureIndex = 0;
            int step = 0;
            std::string feature;

            bool operator<(const FeatureData& other) const {
                if (step != other.step) return step < other.step;
                return featureIndex < other.featureIndex;
            }
        };

        std::map<std::string, int> featureIndex;
        int nextFeatureIndex = 0;
        std::map<FeatureData, std::set<FeatureData>> edges;
        int maxStep = 0;

        for (const std::string& source : featureSources) {
            std::vector<FeatureData> featureList;
            maxStep = std::max(maxStep, biomeFeatures.stepCountForBiome(source));

            for (int step = 0; step < GenerationStep::COUNT; ++step) {
                const auto& stepFeatures = biomeFeatures.featuresForStep(source, step);

                for (const std::string& feature : stepFeatures) {
                    auto [it, inserted] = featureIndex.emplace(feature, nextFeatureIndex);
                    if (inserted) ++nextFeatureIndex;
                    featureList.push_back(FeatureData{ it->second, step, feature });
                }
            }

            for (std::size_t i = 0; i < featureList.size(); ++i) {
                auto& outgoing = edges[featureList[i]];
                if (i + 1 < featureList.size()) {
                    outgoing.insert(featureList[i + 1]);
                }
            }
        }

        std::set<FeatureData> discovered;
        std::set<FeatureData> visiting;
        std::vector<FeatureData> sortedFeatures;

        for (const auto& [feature, _] : edges) {
            if (!visiting.empty()) {
                throw std::logic_error("FeatureSorter DFS finished with non-empty visiting set");
            }
            if (!discovered.contains(feature) && dfs(edges, discovered, visiting, sortedFeatures, feature)) {
                (void)tryReducingError;
                throw std::logic_error("Feature order cycle found");
            }
        }

        std::reverse(sortedFeatures.begin(), sortedFeatures.end());

        std::vector<StepFeatureData> out(static_cast<std::size_t>(maxStep));
        for (const FeatureData& data : sortedFeatures) {
            if (data.step >= 0 && data.step < maxStep) {
                out[static_cast<std::size_t>(data.step)].features.push_back(data.feature);
            }
        }
        for (StepFeatureData& stepData : out) {
            for (std::size_t i = 0; i < stepData.features.size(); ++i) {
                stepData.indexMapping.emplace(stepData.features[i], static_cast<int>(i));
            }
        }
        return out;
    }

    static std::vector<int> selectFeatureIndicesForStep(const std::vector<std::string>& possibleBiomes,
                                                         const BiomeFeatures& biomeFeatures,
                                                         const StepFeatureData& stepFeatureData,
                                                         int step) {
        std::set<int> possible;
        for (const std::string& biome : possibleBiomes) {
            for (const std::string& feature : biomeFeatures.featuresForStep(biome, step)) {
                const int idx = stepFeatureData.indexOf(feature);
                if (idx >= 0) possible.insert(idx);
            }
        }
        return std::vector<int>(possible.begin(), possible.end());
    }

private:
    template <typename FeatureData>
    static bool dfs(const std::map<FeatureData, std::set<FeatureData>>& edges,
                    std::set<FeatureData>& discovered,
                    std::set<FeatureData>& visiting,
                    std::vector<FeatureData>& reverseTopologicalOrder,
                    const FeatureData& current) {
        if (discovered.contains(current)) return false;
        if (visiting.contains(current)) return true;

        visiting.insert(current);
        const auto it = edges.find(current);
        if (it != edges.end()) {
            for (const FeatureData& next : it->second) {
                if (dfs(edges, discovered, visiting, reverseTopologicalOrder, next)) return true;
            }
        }

        visiting.erase(current);
        discovered.insert(current);
        reverseTopologicalOrder.push_back(current);
        return false;
    }
};

} // namespace mc::levelgen::feature

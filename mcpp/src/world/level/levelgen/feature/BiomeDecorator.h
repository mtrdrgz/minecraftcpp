#pragma once

// Biome decoration entry point.
//
// Strict 1:1 cleanup note:
// The previous runtime implementation mixed some real source-backed code with
// heuristic fallbacks for ores, underwater vegetation, cave plants, bamboo,
// coral, vegetation patches, root systems and other feature types. That made the
// generated world look fuller, but it also broke the project rule that worldgen
// must be ported from 26.1.2 source/data instead of approximated.
//
// applyBiomeDecoration is intentionally a no-op until the Java-equivalent
// foundation is restored:
//   * WorldGenRegion-backed placement instead of single-chunk views;
//   * Java FeatureSorter / global feature indices;
//   * exact placed_feature modifiers and block predicates;
//   * configured_feature ports implemented one by one from 26.1.2/src;
//   * parity tests for each re-enabled generation step.
//
// Keep this API stable so callers can remain wired to the future vanilla-style
// decoration stage while the runtime stays free of approximate feature output.

#include "../../block/BlockTags.h"
#include "../../chunk/LevelChunk.h"
#include "BiomeFeatures.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace mc::levelgen::feature {

using JsonAssetReader = std::function<std::optional<std::string>(std::string_view path)>;

// Optional fallback used by standalone builds whose worldgen JSON is embedded in
// the MCAS asset pack. Filesystem data remains the first choice when present.
void setJsonAssetReader(JsonAssetReader reader);

// worldgenDir is the data/minecraft/worldgen root. Currently no-op by design;
// re-enable only with source-backed, parity-tested Feature / Placement ports.
void applyBiomeDecoration(LevelChunk& chunk, std::int64_t worldSeed,
                          const std::function<std::string(int, int, int)>& biomeGetter,
                          const BiomeFeatures& biomeFeatures,
                          const mc::block::BlockTags& tags,
                          const std::string& worldgenDir,
                          const std::function<LevelChunk*(int, int)>& chunkAt = {});

} // namespace mc::levelgen::feature

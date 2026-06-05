#include "BiomeDecorator.h"

#include <utility>

namespace mc::levelgen::feature {

namespace {
JsonAssetReader& jsonAssetReader() {
    static JsonAssetReader reader;
    return reader;
}
} // namespace

void setJsonAssetReader(JsonAssetReader reader) {
    jsonAssetReader() = std::move(reader);
}

void applyBiomeDecoration(LevelChunk&, std::int64_t,
                          const std::function<std::string(int, int, int)>&,
                          const BiomeFeatures&,
                          const mc::block::BlockTags&,
                          const std::string&,
                          const std::function<LevelChunk*(int, int)>&) {
    // Intentionally disabled for strict 1:1 worldgen cleanup.
    //
    // This file previously mixed some source-backed pieces with many visual or
    // heuristic fallbacks: simplified underwater placers, multiface/cave vines,
    // root systems, vegetation patches, coral, bamboo, nether vegetation, etc.
    // That made the world look fuller, but it also generated non-vanilla blocks
    // and hid missing ports behind approximations. Runtime biome decoration should
    // be re-enabled feature-by-feature only after each configured_feature,
    // placed_feature modifier and block predicate is ported from 26.1.2 source/data
    // and has parity coverage. Until then, the terrain pipeline remains cleaner:
    // base terrain/surface can be audited without false vegetation/ore noise.
}

} // namespace mc::levelgen::feature

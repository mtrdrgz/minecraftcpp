#include "OreGen.h"

namespace mc::levelgen::feature {

// Intentionally empty.
//
// Ore generation is now driven by the vanilla configured_feature / placed_feature
// JSON path in BiomeDecorator.cpp. The previous hard-coded OreGen table was a
// pre-data-driven approximation and must not participate in runtime worldgen: it
// used manual counts/ranges such as emerald count=100 and a custom per-chunk RNG,
// which breaks the project's 1:1 parity rule. Keep this translation unit present
// only so existing build files that still list OreGen.cpp continue to compile;
// any future ore fixes should port OreFeature / placement data from 26.1.2 source
// and data packs instead of reviving this file.

} // namespace mc::levelgen::feature

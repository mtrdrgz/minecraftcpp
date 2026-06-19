#pragma once

#include "../../chunk/LevelChunk.h"
#include <cstdint>
#include <functional>
#include <string>

namespace mc::levelgen::feature {

// Deprecated compatibility stub.
//
// Runtime ore generation must go through the data-driven configured_feature /
// placed_feature path in BiomeDecorator.cpp. This legacy API used to expose a
// hard-coded approximation table and a custom ore RNG; keep the symbol declared
// only to make accidental call sites obvious during review, not as a supported
// worldgen path.
[[deprecated("Use data-driven BiomeDecorator ore features, not legacy OreGen")]]
void decorateOres(LevelChunk& chunk, uint64_t worldSeed, const std::function<std::string(int, int, int)>& biomeGetter);

} // namespace mc::levelgen::feature

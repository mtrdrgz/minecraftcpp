#pragma once

// Faithful port of net.minecraft.world.level.chunk.ChunkGenerator.applyBiomeDecoration
// for a single chunk. Unlike the older heuristic decorateSurface(), this drives the
// REAL pipeline end-to-end:
//   * WorldgenRandom decoration RNG: setDecorationSeed(seed, minX, minZ) then,
//     per feature, setFeatureSeed(decorationSeed, indexWithinStep, step);
//   * Java GenerationStep.Decoration ordering and per-biome feature lists
//     (BiomeFeatures, from the biome JSON) so each feature's seed index is exact;
//   * the ported PlacedFeature -> Feature placement, writing through a WorldGenLevel
//     view of the chunk, gated by the real minecraft:biome filter and canSurvive.
//
// Scope note: features whose C++ port doesn't exist yet still consume their seed
// index (so the ported features' seeds match vanilla) but are skipped. Placement
// is clamped to the single chunk (no neighbour WorldGenRegion yet). This header is
// kept free of the placement/heightprovider headers on purpose, so it can be
// included alongside the surface generator without the VerticalAnchor clash.

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

// worldgenDir is the data/minecraft/worldgen root (holding placed_feature/ and
// configured_feature/); features are loaded from it data-drivenly and cached.
//
// chunkAt (optional): resolves a chunk by chunk coords so features (notably trees)
// can write across the active chunk's borders into loaded neighbours instead of
// being clipped to a 16×16 box. If empty, decoration is clamped to `chunk` (unit
// tests). Writes to a position whose owning chunk isn't loaded are dropped.
void applyBiomeDecoration(LevelChunk& chunk, std::int64_t worldSeed,
                          const std::function<std::string(int, int, int)>& biomeGetter,
                          const BiomeFeatures& biomeFeatures,
                          const mc::block::BlockTags& tags,
                          const std::string& worldgenDir,
                          const std::function<LevelChunk*(int, int)>& chunkAt = {});

} // namespace mc::levelgen::feature

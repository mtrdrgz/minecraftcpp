#pragma once

// Biome decoration entry point: thin delegation onto the CERTIFIED whole-chunk
// decoration machinery (EngineDecoration.h, which lives in the same TU the
// full_chunk_decorate_parity gate certifies byte-for-byte against the real
// no-structures server). Holds the engine decoration context as a static,
// created lazily on first use with (dataDir, worldSeed, &chunkMap).
//
// THREADING: main-thread only (the caches/hooks behind the context are not
// thread-safe).
//
// Dev-build data source: the context loads tags/biomes/climate from DISK
// (…/26.1.2/data/minecraft — the engine runs from the repo root, the same
// directory the parity harness uses). The embedded-assets path is deferred to
// the packaging task; setJsonAssetReader is kept so that wiring stays stable.

#include "../../chunk/LevelChunk.h"
#include "../../../../core/Math.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc::levelgen { class RandomSource; }

namespace mc::levelgen::feature {

using JsonAssetReader = std::function<std::optional<std::string>(std::string_view path)>;

// Optional fallback used by standalone builds whose worldgen JSON is embedded in
// the MCAS asset pack. NOT consumed yet (decoration requires disk data for now);
// kept so the packaging task can wire it without touching callers.
void setJsonAssetReader(JsonAssetReader reader);

// Create (or re-create, when seed/map changed) the engine decoration context.
// `dataMinecraftDir` is the …/data/minecraft root on disk; empty disables
// decoration with a clear log line. Returns whether decoration is available.
bool ensureEngineDecoration(const std::string& dataMinecraftDir, std::uint64_t worldSeed,
                            std::unordered_map<std::int64_t, std::unique_ptr<LevelChunk>>* chunks);

// Drop the context (call when leaving/restarting a world: the chunk map is
// cleared and every per-chunk decoration state must go with it).
void resetEngineDecoration();

// Call once per chunk right after applyCarvers, on the MAIN thread, after the
// chunk is in the engine chunk map and before any decoration can reach it.
// `genMarks` = postprocess marks collected from fillFromNoise + applyCarvers.
void freezeWorldgenHeights(LevelChunk& chunk, const std::vector<BlockPos>* genMarks);

// Decorate one chunk (its 8 neighbours must exist in the map and have been
// frozen) and run its FULL-promotion postprocess. No-op when the context is
// unavailable. The caller manages the chunk.decorated flag and remeshing.
void applyBiomeDecoration(LevelChunk& chunk);

// Begin the Java FEATURES turn for a chunk before structure placement. This primes
// non-WG heightmaps exactly once; applyBiomeDecoration continues the same turn.
void beginFeatureTurn(LevelChunk& chunk);

// Called by FeaturePoolElement.place during structure placement. The random is
// already seeded by ChunkGenerator.applyFeaturesAndStructures for the structure
// step/index, so this function must not reseed it.
bool placeStructurePoolFeature(const std::string& featureId, mc::levelgen::RandomSource& random,
                               BlockPos origin, ChunkPos decoratingChunk);

// Install the per-step structure placement hook (Java applyBiomeDecoration:
// structures of generation step k place before step k's features). The engine's
// runStructures installs it; applyBiomeDecoration invokes it per step.
void setStructureStepHook(std::function<void(int, int, int)> hook);

} // namespace mc::levelgen::feature

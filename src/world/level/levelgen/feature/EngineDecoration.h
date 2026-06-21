#pragma once

// Engine entry points into the CERTIFIED whole-chunk decoration machinery that
// lives in FullChunkDecorateParityTest.cpp (compiled into the engine with
// MCPP_DECORATE_NO_MAIN; the parity executable keeps its main and certifies the
// exact same code paths byte-for-byte against the no-structures server).
//
// Model (see the "engine integration API" block at the bottom of the TU):
//  * engineDecorationCreate loads tags / fluid tags / biomes / per-biome climate
//    from `dataDir` (the …/data/minecraft root, same directory the parity
//    harness uses), builds the global FeatureSorter step data and the
//    fail-closed placed/configured-feature loaders, and binds a long-lived
//    WorldGenRegion-style level over the ENGINE'S OWN chunk map. Returns
//    nullptr (with a log line) on failure. The chunk-map key packing MUST be
//    (int64(uint32(cx)) << 32) | uint32(cz) — verified identical to
//    Minecraft::chunkKey.
//  * engineFreezeWgHeights MUST be called exactly once per generated chunk,
//    right after applyCarvers and before the chunk is reachable by any
//    decoration turn. It snapshots the chunk's frozen *_WG heightmap pair and
//    injects the chunk's noise+carver postprocess marks (pass the marks
//    collected by fillFromNoise/applyCarvers).
//  * engineDecorateChunk runs one chunk's FEATURES turn (all 8 neighbours MUST
//    exist in the map and have been frozen) followed by the chunk's
//    FULL-promotion postprocess pass.
//
// THREADING: main-thread only. The caches/hooks (and the parity globals they
// route through) are not thread-safe.
//
// KNOWN DELTA (documented, not a bug): the engine decorates chunks in
// neighbour-availability order, not the ground truth's fixed xz batch order, so
// cross-chunk overlap cells at borders are last-writer-wins and may differ from
// the byte-certified dumps. Content per feature is the certified code; a future
// engine-dump gate will pin the streaming order.

#include "../../chunk/LevelChunk.h"
#include "../../../../core/Math.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc::levelgen { class RandomSource; }

namespace mc::levelgen::feature {

struct EngineDecorationContext;   // opaque (defined in the decoration TU)

EngineDecorationContext* engineDecorationCreate(
    const std::string& dataDir, std::uint64_t worldSeed,
    std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>>* chunks);

void engineDecorationDestroy(EngineDecorationContext* ctx);

// Call once per chunk right after applyCarvers (before any decoration can reach
// the chunk). `genMarks` = the fluid-update postprocess marks collected from
// fillFromNoise + applyCarvers for THIS chunk (may be nullptr).
void engineFreezeWgHeights(EngineDecorationContext* ctx, mc::LevelChunk* chunk, int cx, int cz,
                           const std::vector<mc::BlockPos>* genMarks);

// Decorate chunk (cx,cz) of the engine chunk map (8 neighbours MUST exist and be
// frozen), then run its FULL-promotion postprocess pass.
void engineDecorateChunk(EngineDecorationContext* ctx, int cx, int cz);

// Start the chunk's FEATURES turn without running biome features yet. This mirrors
// ChunkStatusTasks.generateFeatures heightmap priming before ChunkGenerator places
// structures for each step. Safe to call more than once for the same chunk.
void engineBeginFeatureTurn(EngineDecorationContext* ctx, int cx, int cz);

// FeaturePoolElement.place: run one placed_feature at the exact jigsaw position
// using the caller's already-seeded structure FEATURES random.
bool enginePlaceStructurePoolFeature(EngineDecorationContext* ctx, const std::string& featureId,
                                     mc::levelgen::RandomSource& random, mc::BlockPos origin,
                                     int decoratingCx, int decoratingCz);

} // namespace mc::levelgen::feature

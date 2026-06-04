#pragma once

#include "../../chunk/LevelChunk.h"
#include <cstdint>
#include <functional>
#include <string>

namespace mc::levelgen::structure {

// World view used by structure placement. Unlike a single LevelChunk, this lets a
// structure read/write across chunk boundaries: the caller routes world (x,y,z) to
// the owning loaded chunk and silently drops writes outside the loaded area. This
// is what keeps a 21x21 desert pyramid from being clipped to its origin chunk.
//   getBlock(x,y,z)      -> state id at world pos (0/air if unavailable)
//   setBlock(x,y,z,id)   -> write state id at world pos (no-op if unavailable)
//   heightAt(x,z)        -> surface Y (topmost solid) at world column
struct StructureWorld {
    std::function<uint32_t(int, int, int)>      getBlock;
    std::function<void(int, int, int, uint32_t)> setBlock;
    std::function<int(int, int)>                 heightAt;
};

// Places at most one "spaced" structure whose grid cell resolves to `active`
// (desert pyramid / swamp hut / igloo / ruined portal / stronghold room), then
// makes a best-effort pass at underground monster-room dungeons. Deterministic in
// worldSeed. The piece shapes are hand-built approximations — not yet the faithful
// template/jigsaw ports — but the spacing/biome gating mirrors the structure_set
// JSON, and writes span loaded neighbours via `world`.
void generateStructures(ChunkPos active, uint64_t worldSeed,
                        const StructureWorld& world,
                        const std::function<std::string(int, int, int)>& biomeGetter);

} // namespace mc::levelgen::structure

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

// Generates vanilla data-driven jigsaw structures whose structure_set grid resolves
// to `active`. Unsupported structure families deliberately no-op rather than
// placing hand-built approximations.
void generateStructures(ChunkPos active, uint64_t worldSeed,
                        const StructureWorld& world,
                        const std::function<std::string(int, int, int)>& biomeGetter,
                        const std::string& dataMinecraftDir = {});

} // namespace mc::levelgen::structure

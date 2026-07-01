#pragma once

#include "../../chunk/LevelChunk.h"
#include "../Beardifier.h"
#include <cstdint>
#include <functional>
#include <string>

namespace mc::levelgen { class RandomSource; }

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
    std::function<bool(const std::string&, mc::levelgen::RandomSource&, ::mc::BlockPos)> placeFeature;
};

// Generates vanilla data-driven jigsaw structures whose structure_set grid resolves
// to `active`. Unsupported structure families deliberately no-op rather than
// placing hand-built approximations.
void generateStructures(ChunkPos active, uint64_t worldSeed,
                        const StructureWorld& world,
                        const std::function<std::string(int, int, int)>& biomeGetter,
                        const std::string& dataMinecraftDir = {});

// Build the per-chunk Beardifier (Beardifier.forStructuresInChunk): assembles the
// terrain-adapting structures whose RIGID pieces/junctions reach `active` and returns
// a Beardifier whose compute() must be ADDED to the final density during fillFromNoise
// (NoiseChunk: add(finalDensity, BeardifierMarker)). `columnHeight(x,z)` is the
// noise-column surface (WORLD_SURFACE_WG topmost solid), sampled before the chunk's
// terrain exists. Returns an EMPTY beardifier (compute()==0) when no structure beards
// near the chunk, so terrain elsewhere is byte-unchanged.
mc::levelgen::Beardifier generateBeardifier(
    ChunkPos active, uint64_t worldSeed,
    const std::function<int(int, int)>& columnHeight,
    const std::function<std::string(int, int, int)>& biomeGetter,
    const std::string& dataMinecraftDir);

// ── Structure-starts dump API ───────────────────────────────────────────────
// For the structure-starts parity gate: returns the same piece list the engine
// would place at `active`, without actually writing any blocks. This is the
// C++ counterpart of tools/StructureStartsDump.java — same TSV format, so the
// two outputs can be byte-diffed to certify 1:1 structure assembly.
//
// One Piece per child of the StructureStart, in the engine's stored order. The
// `id` is the NBT piece id ("minecraft:msroom", "minecraft:jigsaw", etc.) as
// the server would write it. `orientation` is Direction.get2DDataValue() (the
// "O" NBT field encoding: SOUTH=0, WEST=1, NORTH=2, EAST=3, -1 = none).
struct DumpPiece {
    std::string id;          // NBT piece id, e.g. "minecraft:msroom"
    int minX, minY, minZ;    // bounding box min
    int maxX, maxY, maxZ;    // bounding box max
    int orientation = -1;    // 2D Direction data value (-1 if none)
    int genDepth = 0;        // GD NBT field
};

struct DumpStart {
    std::string structureId; // e.g. "minecraft:mineshaft"
    int chunkX = 0, chunkZ = 0;
    std::vector<DumpPiece> pieces;
};

// Dumps ALL structure starts whose origin is `active` (i.e. structures that
// BEGIN at this chunk — same definition as StructureStartsDump.java's
// `structures.starts` compound). The caller passes a biomeGetter that returns
// the real biome at (x,y,z) so the biome gate matches the server.
//
// For jigsaw structures, this calls the same assembleJigsaw the engine uses
// at runtime (no block placement). For non-jigsaw, it calls the per-structure
// assembly function (assembleMineshaftNormal etc.) where available; structures
// whose assembly is not exposed yet return an empty piece list with the
// structure id only (coarse: just the start, no children).
std::vector<DumpStart> dumpStructureStarts(
    ChunkPos active, uint64_t worldSeed,
    const std::function<std::string(int, int, int)>& biomeGetter,
    const std::string& dataMinecraftDir);

// Extended dump with terrain height support. The `heightAt` callback returns
// the OCEAN_FLOOR_WG heightmap value at (x, z) — needed for structures that
// project to the ocean floor (buried_treasure, shipwreck, ocean_ruin). When
// `heightAt` is null, defaults to sea level - 1 (the original behavior).
std::vector<DumpStart> dumpStructureStarts(
    ChunkPos active, uint64_t worldSeed,
    const std::function<std::string(int, int, int)>& biomeGetter,
    const std::function<int(int, int)>& heightAt,
    const std::string& dataMinecraftDir);

// Full dump with both heightmaps. `oceanFloorHeightAt` returns OCEAN_FLOOR_WG
// and `worldSurfaceHeightAt` returns WORLD_SURFACE_WG. Jigsaw structures use
// WORLD_SURFACE_WG (via project_start_to_heightmap); ocean structures use
// OCEAN_FLOOR_WG. When a callback is null, falls back to sea level - 1.
std::vector<DumpStart> dumpStructureStarts(
    ChunkPos active, uint64_t worldSeed,
    const std::function<std::string(int, int, int)>& biomeGetter,
    const std::function<int(int, int)>& oceanFloorHeightAt,
    const std::function<int(int, int)>& worldSurfaceHeightAt,
    const std::string& dataMinecraftDir);

// Full dump with both heightmaps + the noise-only block column
// (NoiseBasedChunkGenerator.getBaseColumn: state ids indexed by y - minBuildY).
// Ruined portals need it (findSuitableY scans the 4 corner columns downward).
std::vector<DumpStart> dumpStructureStarts(
    ChunkPos active, uint64_t worldSeed,
    const std::function<std::string(int, int, int)>& biomeGetter,
    const std::function<int(int, int)>& oceanFloorHeightAt,
    const std::function<int(int, int)>& worldSurfaceHeightAt,
    const std::function<std::vector<uint32_t>(int, int)>& baseColumnAt,
    const std::string& dataMinecraftDir);

} // namespace mc::levelgen::structure

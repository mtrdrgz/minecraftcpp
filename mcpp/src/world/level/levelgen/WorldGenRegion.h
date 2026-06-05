#pragma once

#include "../chunk/LevelChunk.h"

#include <cstdint>
#include <functional>

namespace mc::levelgen {

// Minimal foundation for a vanilla-like WorldGenRegion.
//
// Java worldgen does not place features/structures/carvers through a bare
// LevelChunk. It places through a WorldGenRegion that can read neighbouring
// chunks and only writes through the region's writable bounds. This type is the
// project-level replacement for ad-hoc single-chunk views such as ChunkWGL and
// StructureWorld. It intentionally contains no block-specific placement rules:
// kelp, glow lichen, ores, trees, structures, etc. must validate themselves in
// their Java-ported Feature / PlacementModifier / BlockPredicate logic.
class WorldGenRegion {
public:
    using ChunkResolver = std::function<LevelChunk*(int chunkX, int chunkZ)>;

    WorldGenRegion(ChunkPos center, ChunkResolver resolver,
                   int minWritableChunkX, int minWritableChunkZ,
                   int maxWritableChunkX, int maxWritableChunkZ)
        : m_center(center),
          m_resolver(std::move(resolver)),
          m_minWritableChunkX(minWritableChunkX),
          m_minWritableChunkZ(minWritableChunkZ),
          m_maxWritableChunkX(maxWritableChunkX),
          m_maxWritableChunkZ(maxWritableChunkZ) {}

    // Convenience region for the common decoration case: read via resolver, but
    // only write the active chunk. Broader writable regions should be requested
    // explicitly by the generation stage that owns them.
    static WorldGenRegion forCenterChunk(ChunkPos center, ChunkResolver resolver) {
        return WorldGenRegion(center, std::move(resolver), center.x, center.z, center.x, center.z);
    }

    ChunkPos center() const noexcept { return m_center; }

    static int floorDiv(int value, int divisor) noexcept {
        const int q = value / divisor;
        const int r = value % divisor;
        return (r != 0 && ((r < 0) != (divisor < 0))) ? q - 1 : q;
    }

    static int chunkCoord(int worldBlock) noexcept { return floorDiv(worldBlock, 16); }
    static int localCoord(int worldBlock) noexcept { return worldBlock & 15; }

    static bool isInsideBuildHeight(int y) noexcept {
        return y >= CHUNK_MIN_Y && y < CHUNK_MAX_Y;
    }

    LevelChunk* chunkAtChunkPos(int chunkX, int chunkZ) const {
        if (m_resolver) return m_resolver(chunkX, chunkZ);
        return nullptr;
    }

    LevelChunk* chunkAtBlockPos(int worldX, int worldZ) const {
        return chunkAtChunkPos(chunkCoord(worldX), chunkCoord(worldZ));
    }

    bool canRead(int worldX, int worldY, int worldZ) const {
        return isInsideBuildHeight(worldY) && chunkAtBlockPos(worldX, worldZ) != nullptr;
    }

    bool canWrite(int worldX, int worldY, int worldZ) const {
        if (!isInsideBuildHeight(worldY)) return false;
        const int cx = chunkCoord(worldX);
        const int cz = chunkCoord(worldZ);
        return cx >= m_minWritableChunkX && cx <= m_maxWritableChunkX &&
               cz >= m_minWritableChunkZ && cz <= m_maxWritableChunkZ &&
               chunkAtChunkPos(cx, cz) != nullptr;
    }

    uint32_t getBlock(int worldX, int worldY, int worldZ) const {
        if (!isInsideBuildHeight(worldY)) return 0;
        LevelChunk* chunk = chunkAtBlockPos(worldX, worldZ);
        return chunk ? chunk->getBlock(worldX, worldY, worldZ) : 0;
    }

    bool setBlock(int worldX, int worldY, int worldZ, uint32_t stateId) const {
        if (!canWrite(worldX, worldY, worldZ)) return false;
        LevelChunk* chunk = chunkAtBlockPos(worldX, worldZ);
        if (!chunk) return false;
        chunk->setBlock(worldX, worldY, worldZ, stateId);
        return true;
    }

    int heightAt(int worldX, int worldZ) const {
        LevelChunk* chunk = chunkAtBlockPos(worldX, worldZ);
        return chunk ? chunk->heightmap(localCoord(worldX), localCoord(worldZ)) : CHUNK_MIN_Y - 1;
    }

private:
    ChunkPos m_center;
    ChunkResolver m_resolver;
    int m_minWritableChunkX;
    int m_minWritableChunkZ;
    int m_maxWritableChunkX;
    int m_maxWritableChunkZ;
};

} // namespace mc::levelgen

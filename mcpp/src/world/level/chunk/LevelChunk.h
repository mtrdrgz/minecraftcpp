#pragma once
#include "ChunkSection.h"
#include "../../../core/Math.h"
#include "../../../nbt/Tag.h"
#include <array>
#include <vector>
#include <memory>
#include <atomic>

namespace mc {

// Y range for 26.1.2: min = -64, max = 320, sections = 24
static constexpr int CHUNK_MIN_Y        = -64;
static constexpr int CHUNK_MAX_Y        = 320;
static constexpr int CHUNK_SECTION_COUNT = (CHUNK_MAX_Y - CHUNK_MIN_Y) / 16; // 24

// Port of net.minecraft.world.level.chunk.LevelChunk
class LevelChunk {
public:
    explicit LevelChunk(ChunkPos pos);
    ~LevelChunk() = default;

    ChunkPos pos() const { return m_pos; }

    // World-space block access
    uint32_t getBlock(int worldX, int worldY, int worldZ) const;
    void     setBlock(int worldX, int worldY, int worldZ, uint32_t stateId);

    const ChunkSection* getSection(int sectionIndex) const;
          ChunkSection* getSection(int sectionIndex);

    // Section index from world Y (sectionIndex = (y - CHUNK_MIN_Y) / 16)
    static int sectionIndex(int worldY) {
        return (worldY - CHUNK_MIN_Y) >> 4;
    }
    static int localY(int worldY) { return (worldY - CHUNK_MIN_Y) & 15; }

    // Heightmap: highest non-air block Y per XZ (world Y, not local)
    int16_t heightmap(int localX, int localZ) const {
        return m_heightmap[localZ * 16 + localX];
    }
    void computeHeightmap(); // scan sections top-down, write m_heightmap

    bool isLoaded() const { return m_loaded; }
    void setLoaded(bool v) { m_loaded = v; }

    // Dirty flag — set when blocks change, cleared after mesh rebuild
    std::atomic<bool> meshDirty{true};

    // NBT data from block entities (tile entities)
    std::vector<nbt::NbtCompound> blockEntities;

private:
    ChunkPos m_pos;
    std::array<std::unique_ptr<ChunkSection>, CHUNK_SECTION_COUNT> m_sections;
    std::array<int16_t, 256> m_heightmap{}; // 16x16
    bool m_loaded = false;
};

} // namespace mc

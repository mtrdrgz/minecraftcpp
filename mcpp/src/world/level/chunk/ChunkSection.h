#pragma once
#include "PalettedContainer.h"
#include <array>

namespace mc {

// Port of net.minecraft.world.level.chunk.LevelChunkSection
// One 16x16x16 sub-cube of a chunk column
class ChunkSection {
public:
    ChunkSection() = default;

    uint32_t getBlock(int x, int y, int z) const { return m_blocks.get(x, y, z); }
    uint32_t getBiome(int x, int y, int z) const { return m_biomes.get(x >> 2, y >> 2, z >> 2); }

    void setBlock(int x, int y, int z, uint32_t stateId) {
        m_blocks.set(x, y, z, stateId);
        if (stateId != 0) ++m_nonAirCount;
        else if (m_nonAirCount > 0) --m_nonAirCount;
    }

    bool isEmpty() const { return m_nonAirCount == 0; }
    int  nonAirCount() const { return m_nonAirCount; }

    PalettedContainer& blocks() { return m_blocks; }
    PalettedContainer& biomes() { return m_biomes; }
    const PalettedContainer& blocks() const { return m_blocks; }

    // Sky + block light (4-bit per block, nibble-packed)
    std::array<uint8_t, 2048> skyLight{};
    std::array<uint8_t, 2048> blockLight{};

    uint8_t getSkyLight(int x, int y, int z) const {
        int i = (y << 8) | (z << 4) | x;
        uint8_t b = skyLight[i >> 1];
        return (i & 1) ? (b >> 4) : (b & 0xF);
    }
    uint8_t getBlockLight(int x, int y, int z) const {
        int i = (y << 8) | (z << 4) | x;
        uint8_t b = blockLight[i >> 1];
        return (i & 1) ? (b >> 4) : (b & 0xF);
    }

private:
    PalettedContainer m_blocks;
    PalettedContainer m_biomes;
    int               m_nonAirCount = 0;
};

} // namespace mc

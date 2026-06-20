#include "LevelChunk.h"
#include <algorithm>

namespace mc {

LevelChunk::LevelChunk(ChunkPos pos) : m_pos(pos) {}

ChunkSection* LevelChunk::getSection(int idx) {
    if (idx < 0 || idx >= CHUNK_SECTION_COUNT) return nullptr;
    if (!m_sections[idx]) m_sections[idx] = std::make_unique<ChunkSection>();
    return m_sections[idx].get();
}

const ChunkSection* LevelChunk::getSection(int idx) const {
    if (idx < 0 || idx >= CHUNK_SECTION_COUNT) return nullptr;
    return m_sections[idx].get();
}

uint32_t LevelChunk::getBlock(int wx, int wy, int wz) const {
    int idx = sectionIndex(wy);
    if (idx < 0 || idx >= CHUNK_SECTION_COUNT) return 0;
    const ChunkSection* sec = m_sections[idx].get();
    if (!sec) return 0;
    return sec->getBlock(wx & 15, localY(wy), wz & 15);
}

void LevelChunk::setBlock(int wx, int wy, int wz, uint32_t stateId) {
    int idx = sectionIndex(wy);
    if (idx < 0 || idx >= CHUNK_SECTION_COUNT) return;
    if (!m_sections[idx]) m_sections[idx] = std::make_unique<ChunkSection>();
    m_sections[idx]->setBlock(wx & 15, localY(wy), wz & 15, stateId);
    meshDirty = true;
    int lx = wx & 15, lz = wz & 15;
    if (stateId != 0)
        m_heightmap[lz * 16 + lx] = std::max(m_heightmap[lz * 16 + lx], (int16_t)wy);
}

void LevelChunk::setBlockDuringNoise(int wx, int wy, int wz, uint32_t stateId) {
    int idx = sectionIndex(wy);
    if (idx < 0 || idx >= CHUNK_SECTION_COUNT) return;
    if (!m_sections[idx]) m_sections[idx] = std::make_unique<ChunkSection>();
    m_sections[idx]->setBlock(wx & 15, localY(wy), wz & 15, stateId);
}

void LevelChunk::computeHeightmap() {
    // Scan each column top-down, record highest non-air block Y (world coords)
    for (int lz = 0; lz < 16; ++lz) {
        for (int lx = 0; lx < 16; ++lx) {
            int16_t h = (int16_t)(CHUNK_MIN_Y - 1); // sentinel: no solid block
            // Iterate sections from top to bottom
            for (int s = CHUNK_SECTION_COUNT - 1; s >= 0; --s) {
                const ChunkSection* sec = m_sections[s].get();
                if (!sec || sec->isEmpty()) continue;
                int baseY = CHUNK_MIN_Y + s * 16;
                for (int ly = 15; ly >= 0; --ly) {
                    if (sec->getBlock(lx, ly, lz) != 0) {
                        h = (int16_t)(baseY + ly);
                        goto next_column;
                    }
                }
            }
        next_column:
            m_heightmap[lz * 16 + lx] = h;
        }
    }
}

} // namespace mc

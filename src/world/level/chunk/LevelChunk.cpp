#include "LevelChunk.h"
#include <algorithm>

namespace mc {

LevelChunk::LevelChunk(ChunkPos pos) : m_pos(pos) {
    m_heightmap.fill(static_cast<int16_t>(CHUNK_MIN_Y - 1));
}

LevelChunk::LevelChunk(const LevelChunk& other)
    : m_pos(other.m_pos),
      m_heightmap(other.m_heightmap),
      m_loaded(other.m_loaded) {
    meshDirty.store(other.meshDirty.load());
    decorated = other.decorated;
    blockEntities = other.blockEntities;
    for (int i = 0; i < CHUNK_SECTION_COUNT; ++i) {
        if (other.m_sections[i]) {
            m_sections[i] = std::make_unique<ChunkSection>(*other.m_sections[i]);
        }
    }
}

LevelChunk& LevelChunk::operator=(const LevelChunk& other) {
    if (this == &other) return *this;
    m_pos = other.m_pos;
    m_heightmap = other.m_heightmap;
    m_loaded = other.m_loaded;
    meshDirty.store(other.meshDirty.load());
    decorated = other.decorated;
    blockEntities = other.blockEntities;
    for (int i = 0; i < CHUNK_SECTION_COUNT; ++i) {
        m_sections[i].reset();
        if (other.m_sections[i]) {
            m_sections[i] = std::make_unique<ChunkSection>(*other.m_sections[i]);
        }
    }
    return *this;
}

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
    int lx = wx & 15, lz = wz & 15;
    uint32_t oldStateId = 0;
    if (m_sections[idx]) {
        oldStateId = m_sections[idx]->getBlock(lx, localY(wy), lz);
    }
    if (oldStateId == stateId) return;
    if (stateId == 0 && !m_sections[idx]) return;
    if (!m_sections[idx]) m_sections[idx] = std::make_unique<ChunkSection>();
    m_sections[idx]->setBlock(lx, localY(wy), lz, stateId);
    meshDirty = true;
    updateHeightmapAfterSet(lx, wy, lz, oldStateId, stateId);
}

void LevelChunk::setBlockDuringNoise(int wx, int wy, int wz, uint32_t stateId) {
    int idx = sectionIndex(wy);
    if (idx < 0 || idx >= CHUNK_SECTION_COUNT) return;
    int lx = wx & 15, lz = wz & 15;
    uint32_t oldStateId = 0;
    if (m_sections[idx]) {
        oldStateId = m_sections[idx]->getBlock(lx, localY(wy), lz);
    }
    if (oldStateId == stateId) return;
    if (stateId == 0 && !m_sections[idx]) return;
    if (!m_sections[idx]) m_sections[idx] = std::make_unique<ChunkSection>();
    m_sections[idx]->setBlock(lx, localY(wy), lz, stateId);
    updateHeightmapAfterSet(lx, wy, lz, oldStateId, stateId);
}

void LevelChunk::updateHeightmapAfterSet(int lx, int wy, int lz, uint32_t oldStateId, uint32_t newStateId) {
    int16_t& h = m_heightmap[lz * 16 + lx];
    if (newStateId != 0) {
        h = std::max(h, static_cast<int16_t>(wy));
        return;
    }

    if (oldStateId == 0 || h != wy) return;
    h = static_cast<int16_t>(CHUNK_MIN_Y - 1);
    for (int y = wy - 1; y >= CHUNK_MIN_Y; --y) {
        if (getBlock(m_pos.x * 16 + lx, y, m_pos.z * 16 + lz) != 0) {
            h = static_cast<int16_t>(y);
            return;
        }
    }
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

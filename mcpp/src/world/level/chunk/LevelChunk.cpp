#include "LevelChunk.h"
#include "../block/Blocks.h"
#include <algorithm>
#include <string_view>

namespace mc {

namespace {

std::string_view blockName(uint32_t stateId) {
    const BlockState* state = getBlockState(stateId);
    return (state && state->block) ? std::string_view(state->block->name) : std::string_view("air");
}

bool isWater(uint32_t stateId) {
    return blockName(stateId) == "water";
}

bool isSolidOpaque(uint32_t stateId) {
    const BlockState* state = getBlockState(stateId);
    return state && state->isSolid() && state->isOpaque();
}

bool isUnderwaterPlant(std::string_view name) {
    return name == "kelp" || name == "kelp_plant" || name == "seagrass" ||
           name == "tall_seagrass" || name == "sea_pickle";
}

bool isCavePlant(std::string_view name) {
    return name == "glow_lichen" || name == "sculk_vein" ||
           name == "cave_vines" || name == "cave_vines_plant" ||
           name == "hanging_roots" || name == "spore_blossom";
}

bool isCaveVine(std::string_view name) {
    return name == "cave_vines" || name == "cave_vines_plant";
}

} // namespace

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

    const std::string_view placedName = blockName(stateId);
    const int lx = wx & 15;
    const int lz = wz & 15;

    // Low-level safety net for currently simplified underwater feature placers:
    // kelp/seagrass/sea pickles must replace water, never air above the ocean.
    if (isUnderwaterPlant(placedName) && !isWater(getBlock(wx, wy, wz))) {
        return;
    }

    // Low-level safety net for cave-only plants while the full placement chain is
    // being restored. Prevent glow lichen / cave vines from being written on the
    // surface or on top of fluids after missing filters became pass-through.
    if (isCavePlant(placedName)) {
        const int surfaceY = m_heightmap[lz * 16 + lx];
        if (surfaceY >= CHUNK_MIN_Y && wy >= surfaceY - 13) {
            return;
        }

        if (isCaveVine(placedName)) {
            const std::string_view above = blockName(getBlock(wx, wy + 1, wz));
            if (!isSolidOpaque(getBlock(wx, wy + 1, wz)) && above != "cave_vines" && above != "cave_vines_plant") {
                return;
            }
        } else {
            const bool hasSolidFace = isSolidOpaque(getBlock(wx + 1, wy, wz)) ||
                                      isSolidOpaque(getBlock(wx - 1, wy, wz)) ||
                                      isSolidOpaque(getBlock(wx, wy + 1, wz)) ||
                                      isSolidOpaque(getBlock(wx, wy - 1, wz)) ||
                                      isSolidOpaque(getBlock(wx, wy, wz + 1)) ||
                                      isSolidOpaque(getBlock(wx, wy, wz - 1));
            if (!hasSolidFace) {
                return;
            }
        }
    }

    if (!m_sections[idx]) m_sections[idx] = std::make_unique<ChunkSection>();
    m_sections[idx]->setBlock(lx, localY(wy), lz, stateId);
    meshDirty = true;
    if (stateId != 0)
        m_heightmap[lz * 16 + lx] = std::max(m_heightmap[lz * 16 + lx], (int16_t)wy);
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

#include "PalettedContainer.h"
#include <algorithm>
#include <cstring>

namespace mc {

PalettedContainer::PalettedContainer() {
    m_palette.push_back(0); // index 0 = air (state 0)
    m_bitsPerEntry = 4;
    m_data.resize(calcLongsNeeded(m_bitsPerEntry), 0ULL);
}

uint32_t PalettedContainer::getRaw(int index) const {
    if (isDirect()) {
        // Direct: full state ID packed in 15 bits
        int longIndex = (index * 15) / 64;
        int bitOffset = (index * 15) % 64;
        if (longIndex >= (int)m_data.size()) return 0;
        uint64_t v = m_data[longIndex] >> bitOffset;
        if (bitOffset + 15 > 64 && longIndex + 1 < (int)m_data.size())
            v |= m_data[longIndex + 1] << (64 - bitOffset);
        return (uint32_t)(v & 0x7FFF);
    }
    // Palette-based: values do NOT span longs in vanilla MC (aligned per long)
    int valuesPerLong = 64 / m_bitsPerEntry;
    int longIndex  = index / valuesPerLong;
    int bitOffset  = (index % valuesPerLong) * m_bitsPerEntry;
    if (longIndex >= (int)m_data.size()) return 0;
    uint64_t mask = (1ULL << m_bitsPerEntry) - 1;
    uint32_t palIdx = (uint32_t)((m_data[longIndex] >> bitOffset) & mask);
    if (palIdx < m_palette.size()) return m_palette[palIdx];
    return 0;
}

void PalettedContainer::setRaw(int index, uint32_t paletteIdx) {
    int valuesPerLong = 64 / m_bitsPerEntry;
    int longIndex  = index / valuesPerLong;
    int bitOffset  = (index % valuesPerLong) * m_bitsPerEntry;
    uint64_t mask  = (1ULL << m_bitsPerEntry) - 1;
    m_data[longIndex] = (m_data[longIndex] & ~(mask << bitOffset))
                        | ((uint64_t)(paletteIdx & mask) << bitOffset);
}

void PalettedContainer::resize(int newBits) {
    // Rebuild packed array with new bits-per-entry
    std::vector<uint32_t> old(SECTION_SIZE);
    for (int i = 0; i < SECTION_SIZE; ++i) old[i] = getRaw(i);
    m_bitsPerEntry = newBits;
    m_data.assign(calcLongsNeeded(newBits), 0ULL);
    for (int i = 0; i < SECTION_SIZE; ++i) setRaw(i, old[i]);
}

uint32_t PalettedContainer::get(int x, int y, int z) const {
    int index = (y << 8) | (z << 4) | x; // vanilla MC ordering
    return getRaw(index);
}

void PalettedContainer::set(int x, int y, int z, uint32_t stateId) {
    int index = (y << 8) | (z << 4) | x;
    if (isDirect()) {
        setRaw(index, stateId);
        return;
    }
    // Find in palette
    auto it = std::find(m_palette.begin(), m_palette.end(), stateId);
    uint32_t palIdx;
    if (it != m_palette.end()) {
        palIdx = (uint32_t)(it - m_palette.begin());
    } else {
        palIdx = (uint32_t)m_palette.size();
        m_palette.push_back(stateId);
        // Grow bit width if needed
        int needed = 1;
        while ((1 << needed) < (int)m_palette.size()) ++needed;
        needed = std::max(needed, 4);
        if (needed > 8) { // switch to direct
            m_bitsPerEntry = 15;
            m_data.assign(calcLongsNeeded(15), 0ULL);
            // Rebuild in direct mode
            std::vector<uint32_t> old(SECTION_SIZE);
            for (int i = 0; i < SECTION_SIZE; ++i) old[i] = m_palette[getRaw(i)];
            m_palette.clear();
            for (int i = 0; i < SECTION_SIZE; ++i) setRaw(i, old[i]);
            setRaw(index, stateId);
            return;
        }
        if (needed > m_bitsPerEntry) resize(needed);
    }
    setRaw(index, palIdx);
}

size_t PalettedContainer::readNetwork(std::span<const uint64_t> data, int bitsPerEntry, bool hasPalette) {
    m_bitsPerEntry = bitsPerEntry;
    size_t consumed = 0;

    if (hasPalette) {
        // Read palette size as VarInt — but here we just take the first element as count
        // Caller passes already-decoded data; palette is packed before the longs
        // This is handled in the network layer — here we just store what's given
    }

    int longsNeeded = calcLongsNeeded(bitsPerEntry);
    if (bitsPerEntry == 0) {
        // Single-value section: all blocks are the same
        // Palette has 1 entry; no longs follow
        m_data.clear();
        return 0;
    }

    longsNeeded = std::min(longsNeeded, (int)data.size());
    m_data.assign(data.begin(), data.begin() + longsNeeded);
    return (size_t)longsNeeded;
}

} // namespace mc

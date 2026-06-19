#include "PalettedContainer.h"
#include <algorithm>
#include <cstring>

namespace mc {

PalettedContainer::PalettedContainer() {
    m_palette.push_back(0); // index 0 = air (state 0)
    m_bitsPerEntry = 4;
    m_data.resize(calcLongsNeeded(m_bitsPerEntry), 0ULL);
}

// Both palette and direct modes use the vanilla SimpleBitStorage layout: values
// are packed valuesPerLong = 64/bits per long, NON-SPANNING (the high 64%bits bits
// of each long are padding). This is the disk + network wire layout, certified by
// bit_storage_parity. Direct mode is just this layout with bits=15 and the stored
// value being the state id itself (no palette indirection). The previous direct
// path used a pre-1.16 cross-long-spanning packing ((index*15)/64), which is
// self-consistent for in-memory set/get but MIS-DECODES chunk data read from a real
// server or disk (readNetwork stores the server's non-spanning longs verbatim).
uint32_t PalettedContainer::getRaw(int index) const {
    const int bits         = m_bitsPerEntry;
    const int valuesPerLong = 64 / bits;
    const int longIndex    = index / valuesPerLong;
    const int bitOffset    = (index % valuesPerLong) * bits;
    if (longIndex >= (int)m_data.size()) return 0;
    const uint64_t mask    = (1ULL << bits) - 1;
    const uint32_t stored  = (uint32_t)((m_data[longIndex] >> bitOffset) & mask);
    if (isDirect()) return stored;            // direct: the stored value IS the state id
    return stored < m_palette.size() ? m_palette[stored] : 0;
}

void PalettedContainer::setRaw(int index, uint32_t value) {
    const int bits         = m_bitsPerEntry;
    const int valuesPerLong = 64 / bits;
    const int longIndex    = index / valuesPerLong;
    const int bitOffset    = (index % valuesPerLong) * bits;
    if (longIndex >= (int)m_data.size()) return;
    const uint64_t mask    = (1ULL << bits) - 1;
    m_data[longIndex] = (m_data[longIndex] & ~(mask << bitOffset))
                        | (((uint64_t)value & mask) << bitOffset);
}

void PalettedContainer::resize(int newBits) {
    // Repack the RAW palette indices at the new bit width. The stored values are
    // indices into the (unchanged) palette — resolving them via getRaw (which
    // returns m_palette[idx]) and re-storing the resolved STATE IDS as indices
    // scrambled every cell of the section once a palette grew past 16 entries
    // (the bug behind the chunk-wide block shuffle the decoration parity caught).
    // Only meaningful in palette mode (direct mode never resizes).
    const int oldBits = m_bitsPerEntry;
    const int oldValuesPerLong = 64 / oldBits;
    const uint64_t oldMask = (1ULL << oldBits) - 1;
    std::vector<uint64_t> oldData = std::move(m_data);
    m_bitsPerEntry = newBits;
    m_data.assign(calcLongsNeeded(newBits), 0ULL);
    for (int i = 0; i < SECTION_SIZE; ++i) {
        const int li = i / oldValuesPerLong;
        const uint32_t palIdx = li < (int)oldData.size()
            ? (uint32_t)((oldData[li] >> ((i % oldValuesPerLong) * oldBits)) & oldMask)
            : 0;
        setRaw(i, palIdx);
    }
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
        if (needed > 8) { // switch to direct (15-bit raw state ids)
            // Snapshot the RESOLVED state ids while still in palette mode (the old
            // code wiped m_data first and then read it back through the palette,
            // zeroing the entire section).
            std::vector<uint32_t> old(SECTION_SIZE);
            for (int i = 0; i < SECTION_SIZE; ++i) old[i] = getRaw(i);
            m_bitsPerEntry = 15;
            m_data.assign(calcLongsNeeded(15), 0ULL);
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

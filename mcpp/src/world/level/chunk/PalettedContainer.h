#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <span>
#include <stdexcept>

namespace mc {

// Port of net.minecraft.world.level.chunk.storage.PalettedContainer
// Stores 4096 values (16^3) using palette + packed bit array.
// Supports 4/8/15-bit storage (direct = 15 bits, no palette).

class PalettedContainer {
public:
    static constexpr int SECTION_SIZE = 4096; // 16*16*16

    PalettedContainer();

    // Get/set value at (x,y,z) local to section (0-15 each)
    uint32_t get(int x, int y, int z) const;
    void     set(int x, int y, int z, uint32_t value);

    // Read from network packet (Minecraft's ChunkData format)
    // Returns number of longs consumed
    size_t readNetwork(std::span<const uint64_t> data, int bitsPerEntry, bool hasPalette);

    // Direct state IDs (bitsPerEntry=15, no palette) — used when palette is full
    bool isDirect() const { return m_bitsPerEntry >= 15; }

    uint32_t paletteSize() const { return (uint32_t)m_palette.size(); }

    // Iterate all values (calls f(index, stateId) for each of the 4096 entries)
    template<class F>
    void forEach(F&& f) const {
        for (int i = 0; i < SECTION_SIZE; ++i)
            f(i, getRaw(i));
    }

private:
    int                  m_bitsPerEntry = 4;
    std::vector<uint32_t> m_palette;    // index -> stateId
    std::vector<uint64_t> m_data;       // packed bit array

    uint32_t getRaw(int index) const;
    void     setRaw(int index, uint32_t value);
    void     resize(int newBits);
    // Longs needed for SECTION_SIZE entries. Palette mode (bits < 15) packs an
    // integral number of values per long with no spanning (vanilla layout, matching
    // getRaw/setRaw); direct mode (15 bits) spans values across longs.
    static int calcLongsNeeded(int bits) {
        if (bits <= 0) return 0;
        if (bits >= 15) return (SECTION_SIZE * 15 + 63) / 64;
        const int valuesPerLong = 64 / bits;
        return (SECTION_SIZE + valuesPerLong - 1) / valuesPerLong;
    }
};

} // namespace mc

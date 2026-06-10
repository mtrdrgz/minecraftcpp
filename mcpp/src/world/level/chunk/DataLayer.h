#pragma once

// 1:1 port of net.minecraft.world.level.chunk.DataLayer (26.1.2) — the 4-bit-per-cell
// nibble array (2048 bytes for 4096 cells) used for block-light / sky-light storage on
// a chunk section. Two cells per byte: nibble 0 = low 4 bits, nibble 1 = high 4 bits.
// A DataLayer is "homogenous" (data == null) when every cell equals defaultValue; it
// lazily allocates a packed 2048-byte buffer on the first set(). This is the exact disk
// + network wire layout, so it must match vanilla bit-for-bit.
// Certified by data_layer_parity.
//
// Java bytes are signed; `data` is byte[]. get() reads `data[pos] >> 4*nibble & 15`,
// where the byte promotes to int with sign-extension before the shift — the trailing
// `& 15` masks back down so the result is always 0..15 regardless of sign. We mirror
// that promotion exactly by storing int8_t and computing in int.

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace mc::world::level::chunk {

class DataLayer {
public:
    static constexpr int LAYER_COUNT = 16;
    static constexpr int LAYER_SIZE = 128;
    static constexpr int SIZE = 2048;

    // DataLayer() -> this(0)
    DataLayer() : DataLayer(0) {}

    // DataLayer(int defaultValue)
    explicit DataLayer(int defaultValue)
        : hasData_(false), defaultValue_(defaultValue) {}

    // DataLayer(byte[] data) — adopts a 2048-byte buffer; defaultValue reset to 0.
    explicit DataLayer(const std::vector<int8_t>& data)
        : data_(data), hasData_(true), defaultValue_(0) {
        if (static_cast<int>(data.size()) != SIZE) {
            throw std::invalid_argument(
                "DataLayer should be 2048 bytes not: " + std::to_string(data.size()));
        }
    }

    // public int get(int x, int y, int z) -> get(getIndex(x, y, z))
    int get(int x, int y, int z) const { return getByIndex(getIndex(x, y, z)); }

    // public void set(int x, int y, int z, int val) -> set(getIndex(x, y, z), val)
    void set(int x, int y, int z, int val) { setByIndex(getIndex(x, y, z), val); }

    // private static int getIndex(int x, int y, int z) { return y << 8 | z << 4 | x; }
    static int getIndex(int x, int y, int z) { return y << 8 | z << 4 | x; }

    // private int get(int index)
    int getByIndex(int index) const {
        if (!hasData_) {
            return defaultValue_;
        }
        int position = getByteIndex(index);
        int nibble = getNibbleIndex(index);
        // Java: this.data[position] >> 4 * nibble & 15  (byte sign-extends to int)
        return static_cast<int>(data_[position]) >> (4 * nibble) & 15;
    }

    // private void set(int index, int val)
    void setByIndex(int index, int val) {
        std::vector<int8_t>& d = getData();
        int position = getByteIndex(index);
        int nibble = getNibbleIndex(index);
        int mask = ~(15 << (4 * nibble));
        int valueToSet = (val & 15) << (4 * nibble);
        d[position] = static_cast<int8_t>(static_cast<int>(d[position]) & mask | valueToSet);
    }

    // private static int getNibbleIndex(int index) { return index & 1; }
    static int getNibbleIndex(int index) { return index & 1; }

    // private static int getByteIndex(int position) { return position >> 1; }
    static int getByteIndex(int position) { return position >> 1; }

    // public void fill(int value)
    void fill(int value) {
        defaultValue_ = value;
        hasData_ = false;
        data_.clear();
    }

    // public byte[] getData() — lazily allocates, fills with packFilled(defaultValue).
    std::vector<int8_t>& getData() {
        if (!hasData_) {
            data_.assign(SIZE, static_cast<int8_t>(0));
            hasData_ = true;
            if (defaultValue_ != 0) {
                std::fill(data_.begin(), data_.end(), packFilled(defaultValue_));
            }
        }
        return data_;
    }

    // public DataLayer copy()
    DataLayer copy() const {
        return hasData_ ? DataLayer(data_) : DataLayer(defaultValue_);
    }

    // public boolean isDefinitelyHomogenous() { return this.data == null; }
    bool isDefinitelyHomogenous() const { return !hasData_; }

    // public boolean isDefinitelyFilledWith(int value)
    bool isDefinitelyFilledWith(int value) const {
        return !hasData_ && defaultValue_ == value;
    }

    // public boolean isEmpty() { return this.data == null && this.defaultValue == 0; }
    bool isEmpty() const { return !hasData_ && defaultValue_ == 0; }

private:
    // private static byte packFilled(int value)
    static int8_t packFilled(int value) {
        int8_t packed = static_cast<int8_t>(value);
        for (int i = 4; i < 8; i += 4) {
            packed = static_cast<int8_t>(static_cast<int>(packed) | (value << i));
        }
        return packed;
    }

    std::vector<int8_t> data_;  // byte[] data (only valid when hasData_)
    bool hasData_;              // data != null
    int defaultValue_;
};

}  // namespace mc::world::level::chunk

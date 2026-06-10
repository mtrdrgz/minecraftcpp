#pragma once

// 1:1 port of net.minecraft.util.SimpleBitStorage (26.1.2) — the packed long-array
// storage under PalettedContainer and every chunk section / heightmap. Values are
// packed valuesPerLong = 64/bits per long, NON-SPANNING (the high 64%bits bits of
// each long are padding); cell selection uses the "magic number" division. This is
// the exact disk + network wire layout, so it must match vanilla bit-for-bit.
// Certified by bit_storage_parity.

#include <cstdint>
#include <vector>

namespace mc::util {

// SimpleBitStorage.MAGIC — reflection-dumped from the real class (raw ints).
#include "SimpleBitStorageMagic.inc"

class SimpleBitStorage {
public:
    SimpleBitStorage(int bits, int size) { init(bits, size); }

    // SimpleBitStorage(bits, size, int[] values) — packs values, NON-spanning.
    SimpleBitStorage(int bits, int size, const std::vector<int>& values) {
        init(bits, size);
        int outputIndex = 0;
        int inputOffset = 0;
        for (; inputOffset <= size_ - valuesPerLong_; inputOffset += valuesPerLong_) {
            uint64_t packed = 0;
            for (int k = valuesPerLong_ - 1; k >= 0; --k) {
                packed <<= bits_;
                packed |= static_cast<uint64_t>(values[inputOffset + k]) & mask_;
            }
            data_[outputIndex++] = static_cast<int64_t>(packed);
        }
        int remainder = size_ - inputOffset;
        if (remainder > 0) {
            uint64_t packed = 0;
            for (int k = remainder - 1; k >= 0; --k) {
                packed <<= bits_;
                packed |= static_cast<uint64_t>(values[inputOffset + k]) & mask_;
            }
            data_[outputIndex] = static_cast<int64_t>(packed);
        }
    }

    // SimpleBitStorage(bits, size, long[] data) — adopt raw data (e.g. from wire/disk).
    SimpleBitStorage(int bits, int size, std::vector<int64_t> rawData) {
        init(bits, size);
        data_ = std::move(rawData);
    }

    // cellIndex — the magic-number division equivalent to bitIndex / valuesPerLong.
    int cellIndex(int bitIndex) const {
        uint64_t mul = static_cast<uint64_t>(static_cast<uint32_t>(divideMul_));
        uint64_t add = static_cast<uint64_t>(static_cast<uint32_t>(divideAdd_));
        int64_t prod = static_cast<int64_t>(static_cast<uint64_t>(static_cast<int64_t>(bitIndex)) * mul + add);
        return static_cast<int>((prod >> 32) >> divideShift_);
    }

    int getAndSet(int index, int value) {
        int ci = cellIndex(index);
        uint64_t cell = static_cast<uint64_t>(data_[ci]);
        int bitIndex = (index - ci * valuesPerLong_) * bits_;
        int oldValue = static_cast<int>((cell >> bitIndex) & mask_);
        data_[ci] = static_cast<int64_t>((cell & ~(mask_ << bitIndex)) | ((static_cast<uint64_t>(value) & mask_) << bitIndex));
        return oldValue;
    }

    void set(int index, int value) {
        int ci = cellIndex(index);
        uint64_t cell = static_cast<uint64_t>(data_[ci]);
        int bitIndex = (index - ci * valuesPerLong_) * bits_;
        data_[ci] = static_cast<int64_t>((cell & ~(mask_ << bitIndex)) | ((static_cast<uint64_t>(value) & mask_) << bitIndex));
    }

    int get(int index) const {
        int ci = cellIndex(index);
        uint64_t cell = static_cast<uint64_t>(data_[ci]);
        int bitIndex = (index - ci * valuesPerLong_) * bits_;
        return static_cast<int>((cell >> bitIndex) & mask_);
    }

    // unpack(int[] output) — SimpleBitStorage.java:334-358.
    void unpack(std::vector<int>& output) const {
        int dataLength = static_cast<int>(data_.size());
        int outputOffset = 0;
        for (int i = 0; i < dataLength - 1; ++i) {
            uint64_t cell = static_cast<uint64_t>(data_[i]);
            for (int k = 0; k < valuesPerLong_; ++k) {
                output[outputOffset + k] = static_cast<int>(cell & mask_);
                cell >>= bits_;
            }
            outputOffset += valuesPerLong_;
        }
        int remainder = size_ - outputOffset;
        if (remainder > 0) {
            uint64_t cell = static_cast<uint64_t>(data_[dataLength - 1]);
            for (int k = 0; k < remainder; ++k) {
                output[outputOffset + k] = static_cast<int>(cell & mask_);
                cell >>= bits_;
            }
        }
    }

    const std::vector<int64_t>& getRaw() const { return data_; }
    int getSize() const { return size_; }
    int getBits() const { return bits_; }
    int valuesPerLong() const { return valuesPerLong_; }
    static int requiredLength(int bits, int size) { int vpl = 64 / bits; return (size + vpl - 1) / vpl; }

private:
    void init(int bits, int size) {
        size_ = size;
        bits_ = bits;
        mask_ = (1ULL << bits) - 1ULL;
        valuesPerLong_ = static_cast<int>(static_cast<char>(64 / bits)); // (char) cast like Java
        int row = 3 * (valuesPerLong_ - 1);
        divideMul_ = SIMPLE_BIT_STORAGE_MAGIC[row + 0];
        divideAdd_ = SIMPLE_BIT_STORAGE_MAGIC[row + 1];
        divideShift_ = SIMPLE_BIT_STORAGE_MAGIC[row + 2];
        data_.assign((size + valuesPerLong_ - 1) / valuesPerLong_, 0);
    }

    std::vector<int64_t> data_;
    int      bits_ = 0;
    uint64_t mask_ = 0;
    int      size_ = 0;
    int      valuesPerLong_ = 0;
    int      divideMul_ = 0;
    int      divideAdd_ = 0;
    int      divideShift_ = 0;
};

} // namespace mc::util

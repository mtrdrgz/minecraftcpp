#pragma once

// 1:1 port of net.minecraft.util.datafix.PackedBitStorage (Minecraft 26.1.2).
//
// PackedBitStorage is the legacy (DataFixerUpper) bit-packed integer array: it
// stores `size` values of `bits` bits each, tightly packed across a long[] with
// NO long boundaries respected — a value may straddle two adjacent longs. This is
// the old pre-1.16 chunk palette layout the data fixers still read/write, and it
// differs from the modern SimpleBitStorage (which never lets a value cross a long
// boundary). Translating it 1:1 is a thicket of signed/unsigned shift traps:
//
//   * `position >> 6` / `(index+1)*bits-1 >> 6`  — ARITHMETIC right shift on a
//     Java `int` (signed 32-bit). All inputs here are non-negative, but we keep
//     int32_t arithmetic so any overflow wraps exactly as Java does.
//   * `position ^ startData << 6`  — Java precedence: `<<` binds tighter than `^`,
//     so this is `position ^ (startData << 6)`, computing the in-long bit offset.
//   * `>>>` UNSIGNED right shift on a Java `long` (64-bit). C++ `>>` on a signed
//     int64 is impl-defined/arithmetic, so we MUST do these on uint64_t. The data
//     array elements are Java `long` (int64), but every `>>>` must be unsigned.
//   * `(value & mask) >> shiftBits`  — `value` is a Java `int`; after `& mask`
//     (mask <= 0xFFFFFFFF for bits<=32, but bits<=32 means mask up to 0xFFFFFFFFL)
//     it is non-negative when promoted, but the shift is on the int `value`. We
//     mirror Java: the masked value is taken as a 64-bit non-negative quantity.
//   * the long write `data[start] & ~(mask<<startBit) | (value&mask)<<startBit`
//     — `mask<<startBit` can shift bits off the top of a 64-bit long; on int64
//     this is well-defined modulo 2^64 only via uint64 in C++.
//   * the constructor length `Mth.roundToward(size*bits, 64) / 64` —
//     `size*bits` is computed as a Java `int` (can overflow for huge inputs;
//     we keep int32_t), then roundToward = positiveCeilDiv(input,64)*64 =
//     -floorDiv(-input,64)*64.
//
// Pure, self-contained: depends only on int32/int64 arithmetic. Certified by
// packed_bit_storage_parity (ground truth: tools/PackedBitStorageParity.java
// driving the REAL net.minecraft.util.datafix.PackedBitStorage via reflection).

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace mc::util::datafix {

class PackedBitStorage {
  public:
    static constexpr int BIT_TO_LONG_SHIFT = 6;

    // 1:1 of Mth.floorDiv(a,b) == Math.floorDiv(a,b): floored (toward -inf)
    // integer division of two Java ints.
    static int32_t mthFloorDiv(int32_t a, int32_t b) {
        // Java Math.floorDiv: rounds toward negative infinity.
        int32_t q = a / b;
        // if signs differ and division was not exact, round down.
        if ((a % b != 0) && ((a ^ b) < 0)) {
            --q;
        }
        return q;
    }

    // 1:1 of Mth.positiveCeilDiv(input,divisor) == -Math.floorDiv(-input,divisor).
    static int32_t mthPositiveCeilDiv(int32_t input, int32_t divisor) {
        return -mthFloorDiv(-input, divisor);
    }

    // 1:1 of Mth.roundToward(input,multiple) == positiveCeilDiv(input,multiple)*multiple.
    static int32_t mthRoundToward(int32_t input, int32_t multiple) {
        return mthPositiveCeilDiv(input, multiple) * multiple;
    }

    // Constructor matching `new PackedBitStorage(bits, size)` (allocates a fresh,
    // zeroed long[] of the required length).
    PackedBitStorage(int32_t bits, int32_t size)
        : PackedBitStorage(bits, size,
                           std::vector<int64_t>(
                               static_cast<size_t>(mthRoundToward(size * bits, 64) / 64), 0)) {}

    // Constructor matching `new PackedBitStorage(bits, size, long[] data)`.
    PackedBitStorage(int32_t bits, int32_t size, std::vector<int64_t> data)
        : data_(std::move(data)), bits_(bits), size_(size) {
        // Validate.inclusiveBetween(1L, 32L, bits)
        if (bits < 1 || bits > 32) {
            throw std::invalid_argument("bits out of [1,32]");
        }
        // mask = (1L << bits) - 1L   — done on a Java long (64-bit).
        mask_ = (static_cast<int64_t>(1) << bits) - 1;
        int32_t requiredLength = mthRoundToward(size * bits, 64) / 64;
        if (static_cast<int32_t>(data_.size()) != requiredLength) {
            throw std::invalid_argument("Invalid length given for storage");
        }
    }

    // 1:1 of set(index, value). The Validate.inclusiveBetween range checks throw
    // exactly like Java; we keep them so out-of-range probes match GT behavior.
    void set(int32_t index, int32_t value) {
        // Validate.inclusiveBetween(0L, size-1, index)
        if (index < 0 || index > size_ - 1) {
            throw std::invalid_argument("index out of range");
        }
        // Validate.inclusiveBetween(0L, mask, value)  — compared as Java longs.
        if (static_cast<int64_t>(value) < 0 || static_cast<int64_t>(value) > mask_) {
            throw std::invalid_argument("value out of range");
        }
        int32_t position = index * bits_;
        int32_t startData = position >> 6;                  // arithmetic >> on int
        int32_t endData = ((index + 1) * bits_ - 1) >> 6;   // arithmetic >> on int
        int32_t startBit = position ^ (startData << 6);     // <<  binds tighter than ^

        // data[startData] = data[startData] & ~(mask << startBit) | (value & mask) << startBit
        // All long ops; the masked value is a 64-bit non-negative quantity.
        uint64_t cur = static_cast<uint64_t>(data_[static_cast<size_t>(startData)]);
        uint64_t umask = static_cast<uint64_t>(mask_);
        uint64_t vMaskedLo = static_cast<uint64_t>(value) & umask;
        uint64_t lowWrite = (cur & ~(umask << startBit)) | (vMaskedLo << startBit);
        data_[static_cast<size_t>(startData)] = static_cast<int64_t>(lowWrite);

        if (startData != endData) {
            int32_t shiftBits = 64 - startBit;
            int32_t wantedBits = bits_ - shiftBits;
            // data[endData] = data[endData] >>> wantedBits << wantedBits | (value & mask) >> shiftBits
            // >>> is unsigned on the long; << wantedBits clears the low bits; the
            // value's high portion is (value & mask) >>> shiftBits in effect — Java
            // writes `(value & mask) >> shiftBits` where the masked value is a
            // non-negative int, so >> behaves as logical here. We mirror with the
            // 64-bit non-negative masked value and a logical shift.
            uint64_t end = static_cast<uint64_t>(data_[static_cast<size_t>(endData)]);
            uint64_t cleared = (end >> wantedBits) << wantedBits;
            uint64_t highPart = vMaskedLo >> shiftBits;
            data_[static_cast<size_t>(endData)] = static_cast<int64_t>(cleared | highPart);
        }
    }

    // 1:1 of get(index).
    int32_t get(int32_t index) const {
        if (index < 0 || index > size_ - 1) {
            throw std::invalid_argument("index out of range");
        }
        int32_t position = index * bits_;
        int32_t startData = position >> 6;
        int32_t endData = ((index + 1) * bits_ - 1) >> 6;
        int32_t startBit = position ^ (startData << 6);
        uint64_t umask = static_cast<uint64_t>(mask_);
        if (startData == endData) {
            // (int)(data[startData] >>> startBit & mask)
            uint64_t v = (static_cast<uint64_t>(data_[static_cast<size_t>(startData)]) >> startBit) & umask;
            return static_cast<int32_t>(v);
        }
        int32_t shiftBits = 64 - startBit;
        // (int)((data[startData] >>> startBit | data[endData] << shiftBits) & mask)
        uint64_t lo = static_cast<uint64_t>(data_[static_cast<size_t>(startData)]) >> startBit;
        uint64_t hi = static_cast<uint64_t>(data_[static_cast<size_t>(endData)]) << shiftBits;
        uint64_t v = (lo | hi) & umask;
        return static_cast<int32_t>(v);
    }

    const std::vector<int64_t>& getRaw() const { return data_; }
    int32_t getBits() const { return bits_; }
    int64_t mask() const { return mask_; }

  private:
    std::vector<int64_t> data_;
    int32_t bits_ = 0;
    int64_t mask_ = 0;
    int32_t size_ = 0;
};

}  // namespace mc::util::datafix

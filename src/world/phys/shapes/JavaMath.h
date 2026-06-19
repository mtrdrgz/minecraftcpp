#pragma once
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Bit-exact ports of the small Java math/util pieces the voxel-shape code
// depends on (Minecraft Java Edition 26.1.2):
//
//   net/minecraft/util/Mth.java     — floor(double) (:65-67), clamp(int) (:93-95),
//                                     clamp(double) (:105-107), binarySearch (:506-521)
//   java.lang.Math.round(double)    — JDK exact bit-twiddling implementation
//   java (int)/(long) double casts  — saturating, NaN -> 0 (JLS 5.1.3)
//   com.google.common.math.DoubleMath.fuzzyEquals(a, b, tolerance)
//   com.google.common.math.IntMath.gcd(a, b)   (callers pass positives)
//   java.util.BitSet                — the subset BitSetDiscreteVoxelShape uses:
//                                     set/get/clear(range)/isEmpty/nextClearBit
// ---------------------------------------------------------------------------

namespace mc {

inline constexpr bool rawSignBit(double v) noexcept {
    return (std::bit_cast<uint64_t>(v) >> 63) != 0;
}

inline constexpr bool rawSignBit(float v) noexcept {
    return (std::bit_cast<uint32_t>(v) >> 31) != 0;
}

// Java narrowing cast double -> long (JLS 5.1.3): NaN -> 0, saturates at the
// long range, otherwise truncates toward zero. (C++ raw cast would be UB.)
inline int64_t jlongCast(double v) noexcept {
    if (std::isnan(v)) return 0;
    if (v >= 9223372036854775807.0) return INT64_MAX;
    if (v <= -9223372036854775808.0) return INT64_MIN;
    return static_cast<int64_t>(v);
}

// Java narrowing cast double -> int (JLS 5.1.3).
inline int32_t jintCast(double v) noexcept {
    if (std::isnan(v)) return 0;
    if (v >= 2147483647.0) return INT32_MAX;
    if (v <= -2147483648.0) return INT32_MIN;
    return static_cast<int32_t>(v);
}

// Java: Mth.floor(double) = (int)Math.floor(v) — Mth.java:65-67.
inline int32_t mthFloor(double v) noexcept { return jintCast(std::floor(v)); }

// Java: Mth.clamp(int, int, int) = Math.min(Math.max(value, min), max) — Mth.java:93-95.
constexpr int32_t mthClamp(int32_t value, int32_t mn, int32_t mx) noexcept {
    const int32_t lo = value > mn ? value : mn; // Math.max(value, min)
    return lo < mx ? lo : mx;                   // Math.min(lo, max)
}

// Java: java.lang.Math.min(double, double) — exact JDK semantics: NaN-poisoning
// on the first operand and min(+0.0, -0.0) == -0.0. (std::fmin differs on NaN.)
inline constexpr double javaMathMin(double a, double b) noexcept {
    if (a != a) return a; // a is NaN
    if (a == 0.0 && b == 0.0 && rawSignBit(b)) return b; // min(+0.0,-0.0) == -0.0
    return (a <= b) ? a : b;
}

// Java: java.lang.Math.max(double, double) — exact JDK semantics: NaN-poisoning
// on the first operand and max(-0.0, +0.0) == +0.0. (std::fmax differs on NaN.)
inline constexpr double javaMathMax(double a, double b) noexcept {
    if (a != a) return a; // a is NaN
    if (a == 0.0 && b == 0.0 && rawSignBit(a)) return b; // max(-0.0,+0.0) == +0.0
    return (a >= b) ? a : b;
}

// Java: java.lang.Math.{min,max}(float, float) — same NaN-poison + signed-zero rules as
// the double versions (used by AABB.Builder, which accumulates float corners).
inline constexpr float javaMathMinF(float a, float b) noexcept {
    if (a != a) return a;
    if (a == 0.0f && b == 0.0f && rawSignBit(b)) return b;
    return (a <= b) ? a : b;
}
inline constexpr float javaMathMaxF(float a, float b) noexcept {
    if (a != a) return a;
    if (a == 0.0f && b == 0.0f && rawSignBit(a)) return b;
    return (a >= b) ? a : b;
}

// Java: Mth.clamp(double, double, double) — Mth.java:105-107:
//   value < min ? min : Math.min(value, max)
inline double mthClamp(double value, double mn, double mx) noexcept {
    return value < mn ? mn : javaMathMin(value, mx);
}

// Java: Mth.binarySearch(int from, int to, IntPredicate) — Mth.java:506-521.
template <typename IntPredicate>
inline int32_t mthBinarySearch(int32_t from, int32_t to, IntPredicate condition) {
    int32_t len = to - from;
    while (len > 0) {
        int32_t half = len / 2;
        int32_t middle = from + half;
        if (condition(middle)) {
            len = half;
        } else {
            from = middle + 1;
            len -= half + 1;
        }
    }
    return from;
}

// Java: java.lang.Math.round(double) — the exact JDK implementation (bit
// manipulation; correctly returns 0 for 0.49999999999999994 where the naive
// floor(x + 0.5) would double-round to 1).
inline int64_t javaMathRound(double a) noexcept {
    constexpr int64_t SIGNIFICAND_WIDTH = 53;
    constexpr int64_t EXP_BIT_MASK = 0x7FF0000000000000LL;
    constexpr int64_t SIGNIF_BIT_MASK = 0x000FFFFFFFFFFFFFLL;
    constexpr int64_t EXP_BIAS = 1023;
    static_assert(sizeof(double) == sizeof(int64_t));
    int64_t longBits = std::bit_cast<int64_t>(a); // Double.doubleToRawLongBits
    int64_t biasedExp = (longBits & EXP_BIT_MASK) >> (SIGNIFICAND_WIDTH - 1);
    int64_t shift = (SIGNIFICAND_WIDTH - 2 + EXP_BIAS) - biasedExp;
    if ((shift & -64) == 0) { // shift >= 0 && shift < 64
        int64_t r = (longBits & SIGNIF_BIT_MASK) | (SIGNIF_BIT_MASK + 1);
        if (longBits < 0) r = -r;
        return ((r >> shift) + 1) >> 1; // Java >> on long == C++ arithmetic shift
    }
    return jlongCast(a); // Java (long)a
}

// Guava: com.google.common.math.DoubleMath.fuzzyEquals(a, b, tolerance):
//   Math.copySign(a - b, 1.0) <= tolerance || (a == b) || (isNaN(a) && isNaN(b))
inline bool doubleMathFuzzyEquals(double a, double b, double tolerance) noexcept {
    return std::copysign(a - b, 1.0) <= tolerance || a == b
        || (std::isnan(a) && std::isnan(b));
}

// Guava: com.google.common.math.IntMath.gcd(a, b) for the non-negative inputs the
// shape code passes (list sizes >= 1); same result as the Stein binary gcd Guava uses.
constexpr int32_t intMathGcd(int32_t a, int32_t b) noexcept {
    while (b != 0) {
        int32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

// ---------------------------------------------------------------------------
// java.util.BitSet — the subset used by BitSetDiscreteVoxelShape.java:
//   set(i), get(i), clear(from, to), isEmpty(), nextClearBit(from).
// Java BitSet is conceptually infinite with all bits clear beyond the words in
// use; this implementation preserves those semantics (indices are non-negative
// by construction in the shape code).
// ---------------------------------------------------------------------------
class JavaBitSet {
public:
    explicit JavaBitSet(int32_t nbits) : words_(static_cast<size_t>((nbits + 63) / 64), 0) {}

    void set(int32_t index) {
        const size_t w = static_cast<size_t>(index) >> 6;
        if (w >= words_.size()) words_.resize(w + 1, 0); // Java BitSet grows on set
        words_[w] |= (1ULL << (index & 63));
    }

    bool get(int32_t index) const noexcept {
        const size_t w = static_cast<size_t>(index) >> 6;
        return w < words_.size() && (words_[w] >> (index & 63)) & 1ULL;
    }

    // Java: BitSet.clear(fromIndex, toIndex) — clears [from, to); no-op if from >= to.
    void clear(int32_t from, int32_t to) noexcept {
        if (from >= to) return;
        const int32_t allocBits = static_cast<int32_t>(words_.size()) * 64;
        if (from >= allocBits) return;
        if (to > allocBits) to = allocBits;
        size_t firstWord = static_cast<size_t>(from) >> 6;
        size_t lastWord = static_cast<size_t>(to - 1) >> 6;
        const uint64_t firstMask = ~0ULL << (from & 63);
        const uint64_t lastMask = ~0ULL >> (63 - ((to - 1) & 63));
        if (firstWord == lastWord) {
            words_[firstWord] &= ~(firstMask & lastMask);
        } else {
            words_[firstWord] &= ~firstMask;
            for (size_t w = firstWord + 1; w < lastWord; ++w) words_[w] = 0;
            words_[lastWord] &= ~lastMask;
        }
    }

    // Java: BitSet.isEmpty() — true if no bit is set.
    bool isEmpty() const noexcept {
        for (uint64_t w : words_)
            if (w != 0) return false;
        return true;
    }

    // Java: BitSet.nextClearBit(fromIndex) — index of the first clear bit >= from
    // (bits beyond the allocated words are clear).
    int32_t nextClearBit(int32_t from) const noexcept {
        size_t w = static_cast<size_t>(from) >> 6;
        if (w >= words_.size()) return from;
        uint64_t word = ~words_[w] & (~0ULL << (from & 63));
        while (true) {
            if (word != 0) {
                return static_cast<int32_t>(w * 64) + static_cast<int32_t>(std::countr_zero(word));
            }
            if (++w == words_.size()) return static_cast<int32_t>(w * 64);
            word = ~words_[w];
        }
    }

private:
    std::vector<uint64_t> words_;
};

} // namespace mc

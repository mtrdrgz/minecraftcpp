// 1:1 C++ port of net.minecraft.util.InclusiveRange (26.1.2).
//
// Java source (26.1.2/src/net/minecraft/util/InclusiveRange.java):
//   public record InclusiveRange<T extends Comparable<T>>(T minInclusive, T maxInclusive) {
//      public InclusiveRange {                                   // canonical ctor invariant
//         if (minInclusive.compareTo(maxInclusive) > 0)
//            throw new IllegalArgumentException("min_inclusive must be less than or equal to max_inclusive");
//      }
//      public InclusiveRange(final T value) { this(value, value); }
//      public boolean isValueInRange(final T value) {
//         return value.compareTo(this.minInclusive) >= 0 && value.compareTo(this.maxInclusive) <= 0;
//      }
//      public boolean contains(final InclusiveRange<T> subRange) {
//         return subRange.minInclusive().compareTo(this.minInclusive) >= 0
//             && subRange.maxInclusive.compareTo(this.maxInclusive) <= 0;
//      }
//      public static <T> DataResult<InclusiveRange<T>> create(final T min, final T max) {
//         return min.compareTo(max) <= 0 ? success(new InclusiveRange(min, max))
//                                        : error("min_inclusive must be less than or equal to max_inclusive");
//      }
//      public String toString() { return "[" + this.minInclusive + ", " + this.maxInclusive + "]"; }
//   }
//
// Bit-exact: every comparison maps directly onto Java's Comparable.compareTo sign
// semantics (>0, >=0, <=0) on the element type T. For T=int (Integer), that is plain
// signed comparison. No floats are involved, so no IEEE rounding subtleties.
//
// NOT PORTED (Codec / DataResult / serialization coupled — out of scope for a pure
// value gate): the two codec(...) factory overloads and map(Function). create()'s
// DataResult is modeled here as a bool success flag plus a constructed range.
#ifndef MCPP_UTIL_INCLUSIVE_RANGE_H
#define MCPP_UTIL_INCLUSIVE_RANGE_H

#include <stdexcept>
#include <string>
#include <type_traits>

namespace mc::util {

// T must be a comparable (Comparable<T> in Java). For the certified gate T = int.
// Java's `a.compareTo(b)` returns sign of (a - b) for Integer; we replicate the three
// sign-tests (>0, >=0, <=0) used by the class via the relational operators on T.
template <typename T>
class InclusiveRange {
public:
    // Canonical record constructor. Java throws IllegalArgumentException when
    // minInclusive.compareTo(maxInclusive) > 0.
    InclusiveRange(const T& minInclusive, const T& maxInclusive)
        : min_(minInclusive), max_(maxInclusive) {
        if (minInclusive > maxInclusive) {  // compareTo(...) > 0
            throw std::invalid_argument(
                "min_inclusive must be less than or equal to max_inclusive");
        }
    }

    // public InclusiveRange(final T value) { this(value, value); }
    explicit InclusiveRange(const T& value) : InclusiveRange(value, value) {}

    // Accessors (record components).
    const T& minInclusive() const { return min_; }
    const T& maxInclusive() const { return max_; }

    // public boolean isValueInRange(final T value)
    //   value.compareTo(min) >= 0 && value.compareTo(max) <= 0
    bool isValueInRange(const T& value) const {
        return value >= min_ && value <= max_;
    }

    // public boolean contains(final InclusiveRange<T> subRange)
    //   subRange.min().compareTo(this.min) >= 0 && subRange.max().compareTo(this.max) <= 0
    bool contains(const InclusiveRange<T>& subRange) const {
        return subRange.min_ >= min_ && subRange.max_ <= max_;
    }

    // public String toString() { return "[" + minInclusive + ", " + maxInclusive + "]"; }
    std::string toString() const {
        return "[" + std::to_string(min_) + ", " + std::to_string(max_) + "]";
    }

private:
    T min_;
    T max_;
};

// static create(min, max): models Java's DataResult — `ok` is true when the range is
// constructible (min.compareTo(max) <= 0). When ok, `range` holds the constructed value.
template <typename T>
struct InclusiveRangeResult {
    bool ok;
    T min;
    T max;
};

// public static <T> DataResult<InclusiveRange<T>> create(final T min, final T max)
//   min.compareTo(max) <= 0 ? success(new InclusiveRange(min, max)) : error(...)
template <typename T>
InclusiveRangeResult<T> inclusiveRangeCreate(const T& minInclusive, const T& maxInclusive) {
    if (minInclusive <= maxInclusive) {  // compareTo(...) <= 0
        return InclusiveRangeResult<T>{true, minInclusive, maxInclusive};
    }
    return InclusiveRangeResult<T>{false, minInclusive, maxInclusive};
}

}  // namespace mc::util

#endif  // MCPP_UTIL_INCLUSIVE_RANGE_H

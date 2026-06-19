// 1:1 C++ port of net.minecraft.util.ByIdMap (Minecraft 26.1.2).
//
// Java source: 26.1.2/src/net/minecraft/util/ByIdMap.java
//
// ByIdMap builds an int -> T lookup function in two flavours:
//
//   * sparse(idGetter, values, _default)
//       Backed by an Int2ObjectOpenHashMap. Each value is placed at key
//       idGetter(value). A lookup `apply(id)` returns the stored value for `id`,
//       or `_default` if `id` is absent (Objects.requireNonNullElse).
//       Duplicate ids throw at build time.
//
//   * continuous(idGetter, values, strategy)
//       Builds a *dense, contiguous* array: every value must have a unique id in
//       [0, length), with no gaps (createSortedArray throws otherwise). The
//       returned function handles out-of-range ids per OutOfBoundsStrategy:
//         ZERO  -> in-range: values[id]; else values[0]
//         WRAP  -> values[Mth.positiveModulo(id, length)]   (Math.floorMod)
//         CLAMP -> values[Mth.clamp(id, 0, length - 1)]      (min(max(id,0),len-1))
//
// The genericity over T is irrelevant to the *math*: every branch reduces to an
// integer index computation on `id` and `length`. This port therefore exposes the
// pure index arithmetic (which index, if any, a given id resolves to) plus thin
// T-templated wrappers that build over a std::vector<T>. The parity gate drives the
// index arithmetic directly (T = the index itself), which is exactly what the Java
// lookup functions compute internally.
//
// All constants/formulas are verbatim from the Java; nothing is invented.

#ifndef MCPP_UTIL_BYIDMAP_H
#define MCPP_UTIL_BYIDMAP_H

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc::util {

// ---------------------------------------------------------------------------
// Integer helpers replicated VERBATIM from net.minecraft.util.Mth (26.1.2):
//
//   Mth.clamp(int value, int min, int max)  = Math.min(Math.max(value, min), max)
//   Mth.positiveModulo(int input, int mod)  = Math.floorMod(input, mod)
//
// Math.floorMod(x, y) = x - Math.floorDiv(x, y) * y; the result has the sign of
// the divisor (here `mod`, always > 0 for a non-empty value list), so it lands in
// [0, mod). We reproduce the JDK Math.floorMod algorithm exactly.
// ---------------------------------------------------------------------------

// java.lang.Math.floorDiv(int, int): floor of the exact quotient.
inline int32_t javaFloorDiv(int32_t x, int32_t y) {
    int32_t q = x / y;  // C++ truncates toward zero, as does Java's `/`.
    // If the signs differ and the division was not exact, round toward -infinity.
    if ((x ^ y) < 0 && q * y != x) {
        --q;
    }
    return q;
}

// java.lang.Math.floorMod(int, int).
inline int32_t javaFloorMod(int32_t x, int32_t y) {
    return x - javaFloorDiv(x, y) * y;
}

// net.minecraft.util.Mth.positiveModulo(int, int).
inline int32_t mthPositiveModulo(int32_t input, int32_t mod) {
    return javaFloorMod(input, mod);
}

// net.minecraft.util.Mth.clamp(int, int, int).
inline int32_t mthClamp(int32_t value, int32_t mn, int32_t mx) {
    // Math.min(Math.max(value, min), max)
    int32_t hi = value > mn ? value : mn;
    return hi < mx ? hi : mx;
}

// net.minecraft.util.ByIdMap.OutOfBoundsStrategy
enum class OutOfBoundsStrategy { ZERO, WRAP, CLAMP };

// ---------------------------------------------------------------------------
// Pure index arithmetic for `continuous(...)`.
//
// Given the dense length and an out-of-bounds strategy, resolve an arbitrary `id`
// to the array index the Java lookup function would read. This is the entire
// bit-exact surface of continuous(): the value returned is sortedValues[<index>].
// ---------------------------------------------------------------------------
inline int32_t continuousIndex(int32_t id, int32_t length, OutOfBoundsStrategy strategy) {
    switch (strategy) {
        case OutOfBoundsStrategy::ZERO:
            // id -> (id >= 0 && id < length) ? sortedValues[id] : zeroValue
            // zeroValue == sortedValues[0].
            return (id >= 0 && id < length) ? id : 0;
        case OutOfBoundsStrategy::WRAP:
            // id -> sortedValues[Mth.positiveModulo(id, length)]
            return mthPositiveModulo(id, length);
        case OutOfBoundsStrategy::CLAMP:
            // id -> sortedValues[Mth.clamp(id, 0, length - 1)]
            return mthClamp(id, 0, length - 1);
    }
    return 0;  // unreachable
}

// ---------------------------------------------------------------------------
// Templated builders mirroring the Java API surface. These reproduce the build-
// time validation (duplicate ids, non-contiguous indices, missing values, empty
// list) so callers get the same throwing behaviour as Java. The returned closures
// reproduce the lookup semantics exactly.
// ---------------------------------------------------------------------------

// net.minecraft.util.ByIdMap.continuous(idGetter, values, strategy).
template <typename T>
std::function<T(int32_t)> continuous(const std::function<int32_t(const T&)>& idGetter,
                                     const std::vector<T>& values,
                                     OutOfBoundsStrategy strategy) {
    const int32_t length = static_cast<int32_t>(values.size());
    if (length == 0) {
        throw std::invalid_argument("Empty value list");
    }

    // createSortedArray: place each value at its id, validating contiguity.
    std::vector<const T*> sorted(static_cast<size_t>(length), nullptr);
    for (const T& value : values) {
        int32_t id = idGetter(value);
        if (id < 0 || id >= length) {
            throw std::invalid_argument("Values are not continous, found index " +
                                        std::to_string(id));
        }
        if (sorted[static_cast<size_t>(id)] != nullptr) {
            throw std::invalid_argument("Duplicate entry on id " + std::to_string(id));
        }
        sorted[static_cast<size_t>(id)] = &value;
    }
    for (int32_t i = 0; i < length; ++i) {
        if (sorted[static_cast<size_t>(i)] == nullptr) {
            throw std::invalid_argument("Missing value at index: " + std::to_string(i));
        }
    }

    // Snapshot the sorted values (by copy) so the closure is self-contained.
    std::vector<T> snap;
    snap.reserve(static_cast<size_t>(length));
    for (int32_t i = 0; i < length; ++i) snap.push_back(*sorted[static_cast<size_t>(i)]);

    return [snap = std::move(snap), length, strategy](int32_t id) -> T {
        return snap[static_cast<size_t>(continuousIndex(id, length, strategy))];
    };
}

// net.minecraft.util.ByIdMap.sparse(idGetter, values, _default).
template <typename T>
std::function<T(int32_t)> sparse(const std::function<int32_t(const T&)>& idGetter,
                                 const std::vector<T>& values,
                                 const T& _default) {
    if (values.empty()) {
        throw std::invalid_argument("Empty value list");
    }
    std::unordered_map<int32_t, T> idToObject;
    for (const T& value : values) {
        int32_t id = idGetter(value);
        if (idToObject.find(id) != idToObject.end()) {
            throw std::invalid_argument("Duplicate entry on id " + std::to_string(id));
        }
        idToObject.emplace(id, value);
    }
    return [idToObject = std::move(idToObject), _default](int32_t id) -> T {
        auto it = idToObject.find(id);
        return it != idToObject.end() ? it->second : _default;
    };
}

}  // namespace mc::util

#endif  // MCPP_UTIL_BYIDMAP_H

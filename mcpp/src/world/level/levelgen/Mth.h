#pragma once

// 1:1 port of the parts of net.minecraft.util.Mth that worldgen depends on. These
// MUST match Java bit-for-bit: the sine table is the table-based approximation
// (NOT std::sin/cos), and the index/scale arithmetic reproduces Java's exact
// double-precision multiply + (long) truncation + 16-bit mask. Using a different
// scale (e.g. a truncated float 10430.378f) or table expression diverges by ULPs
// and corrupts feature/carver shapes at boundaries.

#include <array>
#include <cmath>
#include <cstdint>

namespace mc::levelgen::mth {

// SIN[i] = (float)Math.sin(i / 10430.378350470453), i in [0, 65536). Built once.
inline const std::array<float, 65536>& sinTable() {
    static const std::array<float, 65536> table = [] {
        std::array<float, 65536> t{};
        for (int i = 0; i < 65536; ++i) {
            t[static_cast<std::size_t>(i)] = static_cast<float>(std::sin(i / 10430.378350470453));
        }
        return t;
    }();
    return table;
}

// Mth.sin(double): SIN[(int)((long)(i * 10430.378350470453) & 65535L)].
inline float sin(double i) {
    const std::int64_t idx = static_cast<std::int64_t>(i * 10430.378350470453) & 65535LL;
    return sinTable()[static_cast<std::size_t>(idx)];
}

// Mth.cos(double): SIN[(int)((long)(i * 10430.378350470453 + 16384.0) & 65535L)].
inline float cos(double i) {
    const std::int64_t idx = static_cast<std::int64_t>(i * 10430.378350470453 + 16384.0) & 65535LL;
    return sinTable()[static_cast<std::size_t>(idx)];
}

// Mth.floor / Mth.ceil = (int)Math.floor/ceil (the float overloads widen to double).
inline int floor(double v) { return static_cast<int>(std::floor(v)); }
inline int floor(float v)  { return static_cast<int>(std::floor(static_cast<double>(v))); }
inline int ceil(double v)  { return static_cast<int>(std::ceil(v)); }
inline int ceil(float v)   { return static_cast<int>(std::ceil(static_cast<double>(v))); }

// Mth.lerp(double delta, double start, double end) = start + delta * (end - start).
inline double lerp(double delta, double start, double end) { return start + delta * (end - start); }

} // namespace mc::levelgen::mth

#pragma once

// 1:1 port of the PURE math primitives of net.minecraft.world.level.biome.Climate
// (Minecraft Java 26.1.2). This header intentionally ports ONLY the self-contained,
// RandomSource-free, registry-free, datapack-free helpers:
//
//   * quantizeCoord(float)  / unquantizeCoord(long)   -- the QUANTIZATION_FACTOR pack
//   * Climate.target(...)                              -- 6x quantizeCoord -> TargetPoint
//   * Climate.Parameter.point / span / distance        -- quantized interval math
//   * Climate.ParameterPoint.fitness                    -- squared-distance fitness
//   * Climate.parameters(...)                           -- ParameterPoint constructors
//
// Everything else in Climate (the R-tree index, Sampler/DensityFunction sampling, the
// spawn finder) is NOT here -- it is either covered elsewhere or pulls in worldgen
// state. These primitives are the ones the biome-search code is built on, and they
// are riddled with 1:1 traps: float*10000 truncation toward zero (NOT rounding), the
// signed-long Parameter.distance branch, and Mth.square(long) computed in int64 so the
// fitness sum WRAPS on two's-complement overflow exactly as Java's long does.
//
// Reference: 26.1.2/src/net/minecraft/world/level/biome/Climate.java

#include <cstdint>
#include <stdexcept>
#include <string>

namespace mc::biome::ClimateMath {

// private static final float QUANTIZATION_FACTOR = 10000.0F;
inline constexpr float QUANTIZATION_FACTOR = 10000.0f;

// public static long quantizeCoord(float coord) { return (long)(coord * 10000.0F); }
//
// TRAP: the multiply is float*float -> float, THEN a Java (long) cast which truncates
// toward zero (and clamps NaN->0, +-inf -> Long.MAX/MIN). C++ static_cast<int64_t> on a
// finite float truncates toward zero identically; we replicate Java's NaN/inf edge rules
// explicitly so out-of-range inputs match Double-to-long semantics.
inline int64_t quantizeCoord(float coord) {
    const float scaled = coord * QUANTIZATION_FACTOR;
    if (scaled != scaled) {                       // NaN -> 0L  (JLS narrowing of NaN)
        return 0;
    }
    // A finite float is always within long range after scaling for any realistic coord,
    // but honor the JLS saturating cast for completeness.
    if (scaled >= 9223372036854775807.0f) {       // >= 2^63 -> Long.MAX_VALUE
        return INT64_MAX;
    }
    if (scaled <= -9223372036854775808.0f) {      // <= -2^63 -> Long.MIN_VALUE
        return INT64_MIN;
    }
    return static_cast<int64_t>(scaled);          // truncate toward zero
}

// public static float unquantizeCoord(long coord) { return (float)coord / 10000.0F; }
//
// TRAP: (float)coord first rounds the long to nearest float (round-half-to-even, may
// lose precision for |coord| >= 2^24), THEN divides by the float 10000.0F.
inline float unquantizeCoord(int64_t coord) {
    return static_cast<float>(coord) / QUANTIZATION_FACTOR;
}

// Mth.square(long x) { return x * x; }  -- int64 multiply, WRAPS on overflow like Java long.
inline int64_t squareL(int64_t x) {
    // Unsigned multiply to make the two's-complement wrap well-defined (matches Java).
    return static_cast<int64_t>(static_cast<uint64_t>(x) * static_cast<uint64_t>(x));
}

inline int64_t maxL(int64_t a, int64_t b) { return a > b ? a : b; }
inline int64_t minL(int64_t a, int64_t b) { return a < b ? a : b; }

// public record Parameter(long min, long max)
struct Parameter {
    int64_t min = 0;
    int64_t max = 0;

    // public static Parameter point(float min) { return span(min, min); }
    static Parameter point(float minValue) { return span(minValue, minValue); }

    // public static Parameter span(float min, float max) {
    //    if (min > max) throw; return new Parameter(quantizeCoord(min), quantizeCoord(max)); }
    static Parameter span(float minValue, float maxValue) {
        if (minValue > maxValue) {
            throw std::invalid_argument("min > max: " + std::to_string(minValue) + " " +
                                        std::to_string(maxValue));
        }
        return Parameter{ quantizeCoord(minValue), quantizeCoord(maxValue) };
    }

    // public static Parameter span(Parameter min, Parameter max) {
    //    if (min.min() > max.max()) throw; return new Parameter(min.min(), max.max()); }
    static Parameter span(const Parameter& minP, const Parameter& maxP) {
        if (minP.min > maxP.max) {
            throw std::invalid_argument("min > max (Parameter span)");
        }
        return Parameter{ minP.min, maxP.max };
    }

    // public long distance(long target) {
    //    long above = target - this.max; long below = this.min - target;
    //    return above > 0L ? above : Math.max(below, 0L); }
    //
    // TRAP: signed long subtraction (may wrap for extreme inputs, matching Java); the
    // ternary returns `above` when strictly positive, else max(below, 0).
    int64_t distance(int64_t target) const {
        const int64_t above = static_cast<int64_t>(static_cast<uint64_t>(target) - static_cast<uint64_t>(max));
        const int64_t below = static_cast<int64_t>(static_cast<uint64_t>(min) - static_cast<uint64_t>(target));
        return above > 0 ? above : maxL(below, 0);
    }

    // public long distance(Parameter target) {
    //    long above = target.min() - this.max; long below = this.min - target.max();
    //    return above > 0L ? above : Math.max(below, 0L); }
    int64_t distance(const Parameter& target) const {
        const int64_t above = static_cast<int64_t>(static_cast<uint64_t>(target.min) - static_cast<uint64_t>(max));
        const int64_t below = static_cast<int64_t>(static_cast<uint64_t>(min) - static_cast<uint64_t>(target.max));
        return above > 0 ? above : maxL(below, 0);
    }

    // public Parameter span(@Nullable Parameter other) {
    //    return other == null ? this : new Parameter(min(min,other.min), max(max,other.max)); }
    Parameter spanWith(const Parameter& other) const {
        return Parameter{ minL(min, other.min), maxL(max, other.max) };
    }

    bool operator==(const Parameter& o) const { return min == o.min && max == o.max; }
};

// public record TargetPoint(long temperature, long humidity, long continentalness,
//                           long erosion, long depth, long weirdness)
struct TargetPoint {
    int64_t temperature = 0;
    int64_t humidity = 0;
    int64_t continentalness = 0;
    int64_t erosion = 0;
    int64_t depth = 0;
    int64_t weirdness = 0;
};

// public static TargetPoint target(float temperature, float humidity, float continentalness,
//                                  float erosion, float depth, float weirdness)
inline TargetPoint target(float temperature, float humidity, float continentalness,
                          float erosion, float depth, float weirdness) {
    return TargetPoint{
        quantizeCoord(temperature),
        quantizeCoord(humidity),
        quantizeCoord(continentalness),
        quantizeCoord(erosion),
        quantizeCoord(depth),
        quantizeCoord(weirdness)
    };
}

// public record ParameterPoint(Parameter temperature, humidity, continentalness, erosion,
//                              depth, weirdness, long offset)
struct ParameterPoint {
    Parameter temperature;
    Parameter humidity;
    Parameter continentalness;
    Parameter erosion;
    Parameter depth;
    Parameter weirdness;
    int64_t offset = 0;

    // private long fitness(TargetPoint target) {
    //    return Mth.square(temperature.distance(target.temperature))
    //         + Mth.square(humidity.distance(target.humidity))
    //         + ... + Mth.square(offset);
    //
    // TRAP: each Mth.square is a long*long (int64 wrap) and the 7-term sum is int64 too,
    // so the whole thing can overflow/wrap exactly like Java's long arithmetic.
    int64_t fitness(const TargetPoint& t) const {
        int64_t sum = 0;
        sum = static_cast<int64_t>(static_cast<uint64_t>(sum) + static_cast<uint64_t>(squareL(temperature.distance(t.temperature))));
        sum = static_cast<int64_t>(static_cast<uint64_t>(sum) + static_cast<uint64_t>(squareL(humidity.distance(t.humidity))));
        sum = static_cast<int64_t>(static_cast<uint64_t>(sum) + static_cast<uint64_t>(squareL(continentalness.distance(t.continentalness))));
        sum = static_cast<int64_t>(static_cast<uint64_t>(sum) + static_cast<uint64_t>(squareL(erosion.distance(t.erosion))));
        sum = static_cast<int64_t>(static_cast<uint64_t>(sum) + static_cast<uint64_t>(squareL(depth.distance(t.depth))));
        sum = static_cast<int64_t>(static_cast<uint64_t>(sum) + static_cast<uint64_t>(squareL(weirdness.distance(t.weirdness))));
        sum = static_cast<int64_t>(static_cast<uint64_t>(sum) + static_cast<uint64_t>(squareL(offset)));
        return sum;
    }
};

// public static ParameterPoint parameters(Parameter t,h,c,e,d,w, float offset) {
//    return new ParameterPoint(t,h,c,e,d,w, quantizeCoord(offset)); }
inline ParameterPoint parameters(const Parameter& temperature, const Parameter& humidity,
                                 const Parameter& continentalness, const Parameter& erosion,
                                 const Parameter& depth, const Parameter& weirdness, float offset) {
    return ParameterPoint{ temperature, humidity, continentalness, erosion, depth, weirdness,
                           quantizeCoord(offset) };
}

// public static ParameterPoint parameters(float t,h,c,e,d,w, float offset) {
//    return new ParameterPoint(Parameter.point(t), ..., quantizeCoord(offset)); }
inline ParameterPoint parameters(float temperature, float humidity, float continentalness,
                                 float erosion, float depth, float weirdness, float offset) {
    return parameters(Parameter::point(temperature), Parameter::point(humidity),
                      Parameter::point(continentalness), Parameter::point(erosion),
                      Parameter::point(depth), Parameter::point(weirdness), offset);
}

} // namespace mc::biome::ClimateMath

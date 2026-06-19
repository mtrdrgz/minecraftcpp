#pragma once

// 1:1 port of the float-evaluation half of net.minecraft.util.CubicSpline (26.1.2).
//
// NOTE ON FILE NAMING: the assignment asked for this at CubicSpline.h, but that
// path is ALREADY occupied by the engine's real CubicSpline (header+cpp) which is
// bound to DensityFunction/DensityFunctionContext and is part of the live build.
// Clobbering it would break the engine, so this standalone, raw-float-driven
// parity port lives in CubicSplineFloat.h instead. (The engine apply() in
// CubicSpline.cpp is itself a faithful 1:1 of the same Java; this file isolates
// the float math so cubic_spline_parity can drive it directly from a float sweep
// and certify it bit-for-bit against the real net.minecraft.util.CubicSpline.)
//
// CubicSpline is a worldgen spline: a piecewise function defined by sample
// "points" (location, value, derivative). In Java it is a sealed interface with
// two implementations:
//   - Constant<C,I>   : apply(c) -> value (a flat spline).
//   - Multipoint<C,I> : apply(c) -> piecewise Hermite-style interpolation over a
//                       sorted array of locations, each with a child spline value
//                       and a derivative; linear extension past the ends.
//
// In the full engine the input is produced by a ToFloatFunction over a worldgen
// "point": float input = this.coordinate.apply(c). For this gate the coordinate
// is supplied DIRECTLY as a float (we evaluate apply over a raw float sweep), so
// we port the float math of apply faithfully and skip the coordinate plumbing.
// The child `value` splines may themselves be Constant or Multipoint; that is
// modelled recursively via a polymorphic node.
//
// What is ported here (verbatim from CubicSpline.java):
//   - Constant.apply               (CubicSpline.java:138-140)
//   - Multipoint.apply(float)      (CubicSpline.java:251-276) — the linear/derivative
//                                   Hermite interpolation between sample points.
//   - Multipoint.findIntervalStart (CubicSpline.java:278-280) via Mth.binarySearch.
//   - Multipoint.linearExtend      (CubicSpline.java:234-237).
//
// All float math routes through the certified mc::levelgen::mth (Mth.lerp / the
// repo's float arithmetic), so this is bit-exact with the real class.
//
// NOT ported (out of scope / needs un-ported deps for THIS gate): the Codec, the
// Builder ascending-order check, minValue()/maxValue() precompute in
// Multipoint.create (range analysis, not part of apply), parityString(), mapAll(),
// and the ToFloatFunction<C> coordinate indirection. See unportedMethods.

#include <memory>
#include <utility>
#include <vector>

#include "Mth.h"

namespace mc::levelgen::cubicspline {

// A CubicSpline node evaluated against a raw float coordinate. Mirrors the
// Constant / Multipoint duality of the Java sealed interface. The coordinate is
// the float passed straight to apply() (the engine's coordinate.apply(c) result).
class Spline {
public:
    virtual ~Spline() = default;

    // CubicSpline.apply(c) — here `coordinate` is the already-evaluated float.
    virtual float apply(float coordinate) const = 0;
};

using SplinePtr = std::shared_ptr<const Spline>;

// CubicSpline.Constant (CubicSpline.java:136-161). apply returns value unchanged.
class Constant final : public Spline {
public:
    explicit Constant(float value) : value_(value) {}

    // Constant.apply (CubicSpline.java:138-140): return this.value.
    float apply(float /*coordinate*/) const override { return value_; }

    float value() const { return value_; }

private:
    float value_;
};

// CubicSpline.Multipoint (CubicSpline.java:167-306). Stores parallel arrays of
// locations / child value splines / derivatives, all of equal length (>= 1).
class Multipoint final : public Spline {
public:
    Multipoint(std::vector<float> locations, std::vector<SplinePtr> values,
               std::vector<float> derivatives)
        : locations_(std::move(locations)),
          values_(std::move(values)),
          derivatives_(std::move(derivatives)) {}

    // Multipoint.linearExtend (CubicSpline.java:234-237):
    //   derivative == 0 ? value : value + derivative * (input - locations[index]).
    static float linearExtend(float input, const std::vector<float>& locations,
                              float value, const std::vector<float>& derivatives,
                              int index) {
        float derivative = derivatives[static_cast<std::size_t>(index)];
        return derivative == 0.0F
                   ? value
                   : value + derivative * (input - locations[static_cast<std::size_t>(index)]);
    }

    // Multipoint.findIntervalStart (CubicSpline.java:278-280):
    //   Mth.binarySearch(0, locations.length, i -> input < locations[i]) - 1.
    static int findIntervalStart(const std::vector<float>& locations, float input) {
        const int len = static_cast<int>(locations.size());
        return mc::levelgen::mth::binarySearch(
                   0, len,
                   [&](int i) { return input < locations[static_cast<std::size_t>(i)]; }) -
               1;
    }

    // Multipoint.apply (CubicSpline.java:251-276). `c` is the raw float coordinate
    // (the engine first does float input = this.coordinate.apply(c); here input == c).
    float apply(float c) const override {
        const float input = c;
        const int start = findIntervalStart(locations_, input);
        const int lastIndex = static_cast<int>(locations_.size()) - 1;
        if (start < 0) {
            return linearExtend(input, locations_, values_[0]->apply(c), derivatives_, 0);
        }
        if (start == lastIndex) {
            return linearExtend(input, locations_,
                                values_[static_cast<std::size_t>(lastIndex)]->apply(c),
                                derivatives_, lastIndex);
        }

        const float x1 = locations_[static_cast<std::size_t>(start)];
        const float x2 = locations_[static_cast<std::size_t>(start + 1)];
        const float t = (input - x1) / (x2 - x1);
        const Spline& f1 = *values_[static_cast<std::size_t>(start)];
        const Spline& f2 = *values_[static_cast<std::size_t>(start + 1)];
        const float d1 = derivatives_[static_cast<std::size_t>(start)];
        const float d2 = derivatives_[static_cast<std::size_t>(start + 1)];
        const float y1 = f1.apply(c);
        const float y2 = f2.apply(c);
        const float a = d1 * (x2 - x1) - (y2 - y1);
        const float b = -d2 * (x2 - x1) + (y2 - y1);
        // Mth.lerp(t, y1, y2) + t * (1.0F - t) * Mth.lerp(t, a, b).
        return mc::levelgen::mth::lerpF(t, y1, y2) +
               t * (1.0F - t) * mc::levelgen::mth::lerpF(t, a, b);
    }

    const std::vector<float>& locations() const { return locations_; }
    const std::vector<SplinePtr>& values() const { return values_; }
    const std::vector<float>& derivatives() const { return derivatives_; }

private:
    std::vector<float> locations_;
    std::vector<SplinePtr> values_;
    std::vector<float> derivatives_;
};

// Convenience factories mirroring CubicSpline.constant / Builder.build.
inline SplinePtr makeConstant(float value) {
    return std::make_shared<Constant>(value);
}
inline SplinePtr makeMultipoint(std::vector<float> locations, std::vector<SplinePtr> values,
                                std::vector<float> derivatives) {
    return std::make_shared<Multipoint>(std::move(locations), std::move(values),
                                        std::move(derivatives));
}

}  // namespace mc::levelgen::cubicspline

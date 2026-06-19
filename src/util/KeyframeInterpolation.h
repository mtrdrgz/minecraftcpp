#pragma once

// 1:1 port of the PURE interpolation math behind net.minecraft.util.Keyframe /
// KeyframeTrack / KeyframeTrackSampler (26.1.2). The Keyframe record itself is
// just (int ticks, T value) + a Codec and computes nothing; the actual
// interpolation lives in:
//
//   * EasingType.apply(float)                         — EasingType.java
//       - CONSTANT  : x -> 0.0F
//       - LINEAR    : x -> x
//       - <named>   : Ease::* (already certified in mc::util::ease)
//       - CubicBezier: 4-iteration Newton-Raphson on the x-curve, then sample
//                      the y-curve (EasingType.CubicBezier).
//   * KeyframeTrackSampler.sample(long ticks)         — KeyframeTrackSampler.java
//       alpha = (float)(sampleTicks - fromTicks) / (toTicks - fromTicks)
//       easedAlpha = easing.apply(alpha)
//       result = lerp.apply(easedAlpha, fromValue, toValue)
//   * LerpFunction.ofFloat/ofInteger/ofDegrees/...     — LerpFunction.java
//
// What is NOT ported here (coupled to registries/codecs/world/components and
// listed as unported): the EasingType SIMPLE_REGISTRY string<->codec mapping,
// KeyframeTrack codec/validateKeyframes/validatePeriod, the segment baking with
// Optional<Integer> period + floorMod looping (that part IS pure and small, so a
// helper for the alpha + getSegmentAt edge clamps is provided), ARGB.srgbLerp's
// channel unpacking (ARGB is its own certified gate), and LerpFunction.ofColor.
//
// These MUST match Java bit-for-bit. Conventions reproduced exactly:
//   * (float) division: the integer (sampleTicks - fromTicks) and the int
//     (toTicks - fromTicks) are computed in long/int, the numerator is cast to
//     float, then the float divide runs — mirrored with explicit casts.
//   * Mth.lerp(float)/lerpInt are reused VERBATIM from the certified
//     mc::levelgen::mth header (start + alpha*(end-start) / p0 + floor(...)).
//   * Math.abs / Mth.wrapDegrees reused from the certified headers.
//   * CubicBezier uses plain float a*t+b chained multiplies; the 1.0E-5F gradient
//     cutoff literal is copied verbatim from the source.
//
// Source: 26.1.2/src/net/minecraft/util/{Keyframe,KeyframeTrack,
//         KeyframeTrackSampler,EasingType}.java
//         26.1.2/src/net/minecraft/world/attribute/LerpFunction.java

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "util/Ease.h"
#include "world/level/levelgen/Mth.h"

namespace mc::util::keyframe {

namespace mth  = mc::levelgen::mth;
namespace ease = mc::util::ease;

// ── EasingType simple easings (EasingType.java:13-44) ────────────────────────
// EasingType.CONSTANT = registerSimple("constant", x -> 0.0F)
inline float easeConstant(float /*x*/) { return 0.0F; }
// EasingType.LINEAR = registerSimple("linear", x -> x)
inline float easeLinear(float x) { return x; }

// ── EasingType.CubicBezier (EasingType.java:61-121) ──────────────────────────
// CubicCurve: sample(t) = ((a*t + b)*t + c)*t ; sampleGradient(t) = (3a*t + 2b)*t + c.
struct CubicCurve {
    float a, b, c;
    // EasingType.CubicBezier.CubicCurve.sample — EasingType.java:113-115
    float sample(float t) const { return ((a * t + b) * t + c) * t; }
    // EasingType.CubicBezier.CubicCurve.sampleGradient — EasingType.java:117-119
    float sampleGradient(float t) const { return (3.0F * a * t + 2.0F * b) * t + c; }
};

// EasingType.CubicBezier.curveFromControls — EasingType.java:76-78
inline CubicCurve curveFromControls(float v1, float v2) {
    return CubicCurve{3.0F * v1 - 3.0F * v2 + 1.0F, -6.0F * v1 + 3.0F * v2, 3.0F * v1};
}

// EasingType.CubicBezier — built from controls (x1,y1,x2,y2). apply() runs the
// 4-iteration Newton-Raphson identical to the source.
struct CubicBezier {
    CubicCurve xCurve;
    CubicCurve yCurve;

    // EasingType.CubicBezier ctor — EasingType.java:70-74
    CubicBezier(float x1, float y1, float x2, float y2)
        : xCurve(curveFromControls(x1, x2)), yCurve(curveFromControls(y1, y2)) {}

    // EasingType.CubicBezier.apply — EasingType.java:80-95
    float apply(float x) const {
        float t = x;
        // NEWTON_RAPHSON_ITERATIONS = 4 (EasingType.java:65,84)
        for (int i = 0; i < 4; i++) {
            float gradient = xCurve.sampleGradient(t);
            if (gradient < 1.0E-5F) {
                break;
            }
            float error = xCurve.sample(t) - x;
            t -= error / gradient;
        }
        return yCurve.sample(t);
    }
};

// EasingType.cubicBezier / symmetricCubicBezier — EasingType.java:51-57.
inline CubicBezier cubicBezier(float x1, float y1, float x2, float y2) {
    return CubicBezier(x1, y1, x2, y2);
}
inline CubicBezier symmetricCubicBezier(float x1, float y1) {
    return cubicBezier(x1, y1, 1.0F - x1, 1.0F - y1);
}

// ── LerpFunction (LerpFunction.java) ─────────────────────────────────────────
// LerpFunction.ofFloat() = Mth::lerp(float) — LerpFunction.java:7-9 + Mth:532-534.
inline float lerpFloat(float alpha, float from, float to) { return mth::lerpF(alpha, from, to); }

// LerpFunction.ofInteger() = Mth::lerpInt — LerpFunction.java:11-13 + Mth:523-525.
inline int lerpInt(float alpha, int from, int to) { return mth::lerpInt(alpha, from, to); }

// LerpFunction.ofDegrees(maxDelta) — LerpFunction.java:15-20.
//   delta = Mth.wrapDegrees(to - from);
//   return Math.abs(delta) >= maxDelta ? to : from + alpha * delta;
inline float lerpDegrees(float maxDelta, float alpha, float from, float to) {
    float delta = mth::wrapDegrees(to - from);
    return std::fabs(delta) >= maxDelta ? to : from + alpha * delta;
}

// LerpFunction.ofConstant() — LerpFunction.java:22-24.  (alpha,from,to) -> from
template <typename T>
inline T lerpConstant(float /*alpha*/, T from, T /*to*/) { return from; }

// LerpFunction.ofStep(threshold) — LerpFunction.java:26-28.
//   (alpha,from,to) -> alpha >= threshold ? to : from
template <typename T>
inline T lerpStep(float threshold, float alpha, T from, T to) {
    return alpha >= threshold ? to : from;
}

// ── KeyframeTrackSampler.sample inner math (KeyframeTrackSampler.java:50-64) ──
// Given a chosen segment [fromTicks, toTicks] (with fromValue/toValue) and the
// looped sampleTicks, reproduce the alpha computation + edge clamps EXACTLY.
//
//   if (sampleTicks <= fromTicks) return fromValue;
//   if (sampleTicks >= toTicks)   return toValue;
//   alpha = (float)(sampleTicks - fromTicks) / (toTicks - fromTicks);
//
// The subtraction (sampleTicks - fromTicks) is long (sampleTicks is long, fromTicks
// int widens), cast to float; the denominator (toTicks - fromTicks) is int.
inline float segmentAlpha(int64_t sampleTicks, int fromTicks, int toTicks) {
    return static_cast<float>(sampleTicks - fromTicks)
           / static_cast<float>(toTicks - fromTicks);
}

// Math.floorMod(long, int) used by KeyframeTrackSampler.loopTicks — :76-78.
// Reuses the same algorithm as java.lang.Math.floorMod (matches Mth.floorMod's int
// form; here the dividend is long and the divisor int -> long result).
inline int64_t loopTicks(int64_t ticks, int periodTicks) {
    int64_t r = ticks % static_cast<int64_t>(periodTicks);
    if (r != 0 && ((r ^ static_cast<int64_t>(periodTicks)) < 0)) r += periodTicks;
    return r;
}

// ── EasingType dispatch by registry id (EasingType.java:13-44) ───────────────
// Maps an EasingType field name (CONSTANT/LINEAR/IN_BACK/...) to its apply(float).
// Returns nullptr for an unknown name (caller treats as a hard skip — never a
// silent pass-through). Mirrors the registerSimple bindings VERBATIM.
inline float (*easingByName(const char* name))(float) {
    auto eq = [](const char* a, const char* b) {
        while (*a && *b) { if (*a != *b) return false; ++a; ++b; }
        return *a == *b;
    };
    if (eq(name, "CONSTANT"))       return easeConstant;
    if (eq(name, "LINEAR"))         return easeLinear;
    if (eq(name, "IN_BACK"))        return ease::inBack;
    if (eq(name, "IN_BOUNCE"))      return ease::inBounce;
    if (eq(name, "IN_CIRC"))        return ease::inCirc;
    if (eq(name, "IN_CUBIC"))       return ease::inCubic;
    if (eq(name, "IN_ELASTIC"))     return ease::inElastic;
    if (eq(name, "IN_EXPO"))        return ease::inExpo;
    if (eq(name, "IN_QUAD"))        return ease::inQuad;
    if (eq(name, "IN_QUART"))       return ease::inQuart;
    if (eq(name, "IN_QUINT"))       return ease::inQuint;
    if (eq(name, "IN_SINE"))        return ease::inSine;
    if (eq(name, "IN_OUT_BACK"))    return ease::inOutBack;
    if (eq(name, "IN_OUT_BOUNCE"))  return ease::inOutBounce;
    if (eq(name, "IN_OUT_CIRC"))    return ease::inOutCirc;
    if (eq(name, "IN_OUT_CUBIC"))   return ease::inOutCubic;
    if (eq(name, "IN_OUT_ELASTIC")) return ease::inOutElastic;
    if (eq(name, "IN_OUT_EXPO"))    return ease::inOutExpo;
    if (eq(name, "IN_OUT_QUAD"))    return ease::inOutQuad;
    if (eq(name, "IN_OUT_QUART"))   return ease::inOutQuart;
    if (eq(name, "IN_OUT_QUINT"))   return ease::inOutQuint;
    if (eq(name, "IN_OUT_SINE"))    return ease::inOutSine;
    if (eq(name, "OUT_BACK"))       return ease::outBack;
    if (eq(name, "OUT_BOUNCE"))     return ease::outBounce;
    if (eq(name, "OUT_CIRC"))       return ease::outCirc;
    if (eq(name, "OUT_CUBIC"))      return ease::outCubic;
    if (eq(name, "OUT_ELASTIC"))    return ease::outElastic;
    if (eq(name, "OUT_EXPO"))       return ease::outExpo;
    if (eq(name, "OUT_QUAD"))       return ease::outQuad;
    if (eq(name, "OUT_QUART"))      return ease::outQuart;
    if (eq(name, "OUT_QUINT"))      return ease::outQuint;
    if (eq(name, "OUT_SINE"))       return ease::outSine;
    return nullptr;
}

// ── KeyframeTrackSampler<Float> end-to-end (KeyframeTrackSampler.java) ────────
// Concrete Float instantiation of the generic sampler. Reproduces bakeSegments,
// getSegmentAt, loopTicks and sample VERBATIM. EasingType is the per-track easing
// (constant id for all segments, matching EasingType.CONSTANT special-casing for
// the single-keyframe degenerate track).

struct KeyframeF { int ticks; float value; };

// KeyframeTrackSampler.Segment — :80-84.
struct SegmentF {
    float (*easing)(float);  // EasingType.apply for this segment
    float fromValue;
    int   fromTicks;
    float toValue;
    int   toTicks;
};

struct KeyframeTrackSamplerF {
    std::optional<int> periodTicks;
    std::vector<SegmentF> segments;

    // KeyframeTrackSampler.bakeSegments — :19-38.
    static KeyframeTrackSamplerF bake(const std::vector<KeyframeF>& keyframes,
                                      float (*easing)(float),
                                      std::optional<int> period) {
        KeyframeTrackSamplerF s;
        s.periodTicks = period;
        if (keyframes.size() == 1) {
            // List.of(new Segment(EasingType.CONSTANT, value, 0, value, 0)) — :21-24.
            float value = keyframes.front().value;
            s.segments.push_back(SegmentF{easeConstant, value, 0, value, 0});
            return s;
        }
        if (period.has_value()) {
            const KeyframeF& first = keyframes.front();
            const KeyframeF& last  = keyframes.back();
            // segments.add(new Segment(track, last, last.ticks - period, first, first.ticks)) — :30
            s.segments.push_back(SegmentF{easing, last.value, last.ticks - period.value(),
                                          first.value, first.ticks});
            addSegmentsFromKeyframes(keyframes, easing, s.segments);
            // segments.add(new Segment(track, last, last.ticks, first, first.ticks + period)) — :32
            s.segments.push_back(SegmentF{easing, last.value, last.ticks,
                                          first.value, first.ticks + period.value()});
        } else {
            addSegmentsFromKeyframes(keyframes, easing, s.segments);
        }
        return s;
    }

    // KeyframeTrackSampler.addSegmentsFromKeyframes — :40-48.
    static void addSegmentsFromKeyframes(const std::vector<KeyframeF>& keyframes,
                                         float (*easing)(float),
                                         std::vector<SegmentF>& output) {
        for (std::size_t i = 0; i + 1 < keyframes.size(); ++i) {
            const KeyframeF& k = keyframes[i];
            const KeyframeF& nk = keyframes[i + 1];
            output.push_back(SegmentF{easing, k.value, k.ticks, nk.value, nk.ticks});
        }
    }

    // KeyframeTrackSampler.loopTicks — :76-78.
    int64_t loop(int64_t ticks) const {
        return periodTicks.has_value() ? loopTicks(ticks, periodTicks.value()) : ticks;
    }

    // KeyframeTrackSampler.getSegmentAt — :66-74.
    const SegmentF& getSegmentAt(int64_t currentTicks) const {
        for (const SegmentF& seg : segments) {
            if (currentTicks < seg.toTicks) return seg;
        }
        return segments.back();
    }

    // KeyframeTrackSampler.sample(long) — :50-64.  (lerp = LerpFunction.ofFloat)
    float sample(int64_t ticks) const {
        int64_t sampleTicks = loop(ticks);
        const SegmentF& segment = getSegmentAt(sampleTicks);
        if (sampleTicks <= segment.fromTicks) return segment.fromValue;
        if (sampleTicks >= segment.toTicks)   return segment.toValue;
        float alpha = segmentAlpha(sampleTicks, segment.fromTicks, segment.toTicks);
        float easedAlpha = segment.easing(alpha);
        return lerpFloat(easedAlpha, segment.fromValue, segment.toValue);
    }
};

}  // namespace mc::util::keyframe

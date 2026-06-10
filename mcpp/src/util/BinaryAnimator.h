#pragma once

// 1:1 port of net.minecraft.util.BinaryAnimator (+ the easing surface it drives:
// net.minecraft.util.EasingType and net.minecraft.util.Ease) for 26.1.2.
//
// BinaryAnimator is a tiny stateful sweep: tick(active) ramps an integer `ticks`
// up/down between 0 and animationLength; getFactor(partialTicks) lerps ticksOld→ticks,
// divides by animationLength, and runs the result through an EasingType.
//
// Bit-exactness notes (these MUST match Java exactly):
//   * BinaryAnimator.getFactor: Mth.lerp(float,float,float) with the two int fields
//     widened to float, then / animationLength (int widened to float). All float math.
//   * Ease.* mix THREE numeric kinds, faithfully reproduced here:
//       - Mth.sin / Mth.cos    -> the table-based approximation (mc::levelgen::mth).
//       - Mth.square / Mth.cube-> plain float x*x / x*x*x.
//       - Math.pow / Math.sin / Math.sqrt -> host libm in DOUBLE, then narrowed.
//     Math.sqrt is correctly-rounded (== std::sqrt). Math.pow / Math.sin are libm and
//     can in principle differ by ULPs from Java's StrictMath-free libm; the
//     binary_animator_parity gate against the real jar is the arbiter.
//   * The literal float constants below are the EXACT (float)-cast values Java uses,
//     e.g. C4 = (float)(Math.PI * 2.0 / 3.0). They are written as the double the Java
//     expression evaluates to, then cast to float — identical rounding.
//
// Source: 26.1.2/src/net/minecraft/util/{BinaryAnimator,EasingType,Ease}.java

#include <cmath>

#include "../world/level/levelgen/Mth.h"  // mc::levelgen::mth::{sin,cos,square,cube}

namespace mc::util {

namespace bamth = mc::levelgen::mth;

// ── net.minecraft.util.Ease (Ease.java) ──────────────────────────────────────
// Every method is a static `float f(float x)`. Constant casts reproduce Java's
// `(float)(Math.PI * 2.0 / 3.0)` etc. exactly.
namespace ease {

// Mth.square / Mth.cube — plain float ops (NOT the table sin/cos).
inline float sq(float x)  { return bamth::square(x); }   // x * x
inline float cb(float x)  { return bamth::cube(x); }     // x * x * x

// (float)(Math.PI * 2.0 / 3.0)
inline constexpr float C4 = static_cast<float>(3.141592653589793 * 2.0 / 3.0);
// (float)(Math.PI / 2)
inline constexpr float HALF_PI = static_cast<float>(3.141592653589793 / 2.0);
// (float)Math.PI
inline constexpr float PI_F = static_cast<float>(3.141592653589793);
// (float)Math.PI * 4.0F / 9.0F  — note: float cast of PI first, then float * / float.
inline constexpr float C5 = static_cast<float>(3.141592653589793) * 4.0F / 9.0F;

inline float outBounce(float x) {  // forward-declared use by inBounce/inOutBounce
    // n1 = 7.5625F; d1 = 2.75F;
    if (x < 0.36363637F) {
        return 7.5625F * sq(x);
    } else if (x < 0.72727275F) {
        return 7.5625F * sq(x - 0.54545456F) + 0.75F;
    } else {
        return x < 0.9090909090909091
                   ? 7.5625F * sq(x - 0.8181818F) + 0.9375F
                   : 7.5625F * sq(x - 0.95454544F) + 0.984375F;
    }
}

inline float inBack(float x) {
    // c1 = 1.70158F; c3 = 2.70158F
    return sq(x) * (2.70158F * x - 1.70158F);
}

inline float inBounce(float x) {
    return 1.0F - outBounce(1.0F - x);
}

inline float inCubic(float x) {
    return cb(x);
}

inline float inElastic(float x) {
    if (x == 0.0F) return 0.0F;
    if (x == 1.0F) return 1.0F;
    // c4 = (float)(Math.PI * 2.0 / 3.0)
    return static_cast<float>(-std::pow(2.0, 10.0 * x - 10.0)
                              * std::sin((x * 10.0 - 10.75) * static_cast<double>(C4)));
}

inline float inExpo(float x) {
    return x == 0.0F ? 0.0F : static_cast<float>(std::pow(2.0, 10.0 * x - 10.0));
}

inline float inQuart(float x) {
    return sq(sq(x));
}

inline float inQuint(float x) {
    return sq(sq(x)) * x;
}

inline float inSine(float x) {
    return 1.0F - bamth::cos(x * HALF_PI);
}

inline float inOutBounce(float x) {
    return x < 0.5F ? (1.0F - outBounce(1.0F - 2.0F * x)) / 2.0F
                    : (1.0F + outBounce(2.0F * x - 1.0F)) / 2.0F;
}

inline float inOutCirc(float x) {
    return x < 0.5F
               ? static_cast<float>((1.0 - std::sqrt(1.0 - std::pow(2.0 * x, 2.0))) / 2.0)
               : static_cast<float>((std::sqrt(1.0 - std::pow(-2.0 * x + 2.0, 2.0)) + 1.0) / 2.0);
}

inline float inOutCubic(float x) {
    return x < 0.5F ? 4.0F * cb(x)
                    : static_cast<float>(1.0 - std::pow(-2.0 * x + 2.0, 3.0) / 2.0);
}

inline float inOutQuad(float x) {
    return x < 0.5F ? 2.0F * sq(x)
                    : static_cast<float>(1.0 - std::pow(-2.0 * x + 2.0, 2.0) / 2.0);
}

inline float inOutQuart(float x) {
    return x < 0.5F ? 8.0F * sq(sq(x))
                    : static_cast<float>(1.0 - std::pow(-2.0 * x + 2.0, 4.0) / 2.0);
}

inline float inOutQuint(float x) {
    // Java condition: x < 0.5 (double literal) — x widens to double for the compare.
    return static_cast<double>(x) < 0.5
               ? 16.0F * x * x * x * x * x
               : static_cast<float>(1.0 - std::pow(-2.0 * x + 2.0, 5.0) / 2.0);
}

inline float outElastic(float x) {
    // c4 = (float)(Math.PI * 2.0 / 3.0)
    if (x == 0.0F) return 0.0F;
    return x == 1.0F
               ? 1.0F
               : static_cast<float>(std::pow(2.0, -10.0 * x)
                                        * std::sin((x * 10.0 - 0.75) * static_cast<double>(C4))
                                    + 1.0);
}

inline float outExpo(float x) {
    return x == 1.0F ? 1.0F : 1.0F - static_cast<float>(std::pow(2.0, -10.0 * x));
}

inline float outQuad(float x) {
    return 1.0F - sq(1.0F - x);
}

inline float outQuint(float x) {
    return 1.0F - static_cast<float>(std::pow(1.0 - x, 5.0));
}

inline float outSine(float x) {
    return bamth::sin(x * HALF_PI);
}

inline float inOutSine(float x) {
    return -(bamth::cos(PI_F * x) - 1.0F) / 2.0F;
}

inline float outBack(float x) {
    // c1 = 1.70158F; c3 = 2.70158F
    return 1.0F + 2.70158F * cb(x - 1.0F) + 1.70158F * sq(x - 1.0F);
}

inline float outQuart(float x) {
    return 1.0F - sq(sq(1.0F - x));
}

inline float outCubic(float x) {
    return 1.0F - cb(1.0F - x);
}

inline float inOutExpo(float x) {
    if (x < 0.5F) {
        return x == 0.0F ? 0.0F : static_cast<float>(std::pow(2.0, 20.0 * x - 10.0) / 2.0);
    } else {
        return x == 1.0F ? 1.0F : static_cast<float>((2.0 - std::pow(2.0, -20.0 * x + 10.0)) / 2.0);
    }
}

inline float inQuad(float x) {
    return x * x;
}

inline float outCirc(float x) {
    return static_cast<float>(std::sqrt(static_cast<double>(1.0F - sq(x - 1.0F))));
}

inline float inOutElastic(float x) {
    // c5 = (float)Math.PI * 4.0F / 9.0F
    if (x == 0.0F) return 0.0F;
    if (x == 1.0F) return 1.0F;
    double sin = std::sin((20.0 * x - 11.125) * static_cast<double>(C5));
    return x < 0.5F
               ? static_cast<float>(-(std::pow(2.0, 20.0 * x - 10.0) * sin) / 2.0)
               : static_cast<float>(std::pow(2.0, -20.0 * x + 10.0) * sin / 2.0 + 1.0);
}

inline float inCirc(float x) {
    return static_cast<float>(-std::sqrt(static_cast<double>(1.0F - x * x))) + 1.0F;
}

inline float inOutBack(float x) {
    // c1 = 1.70158F; c2 = 2.5949094F
    if (x < 0.5F) {
        return 4.0F * x * x * (7.189819F * x - 2.5949094F) / 2.0F;
    }
    float dt = 2.0F * x - 2.0F;
    return (dt * dt * (3.5949094F * dt + 2.5949094F) + 2.0F) / 2.0F;
}

}  // namespace ease

// ── net.minecraft.util.EasingType (EasingType.java) ──────────────────────────
// The "simple" easings are function pointers (float->float). CONSTANT returns 0,
// LINEAR returns x. We expose them as named function pointers matching the Java
// registry ids; the BinaryAnimator stores one EasingFn.
using EasingFn = float (*)(float);

namespace easing_type {

inline float constant(float) { return 0.0F; }   // CONSTANT: x -> 0.0F
inline float linear(float x) { return x; }       // LINEAR:   x -> x

inline constexpr EasingFn CONSTANT       = &constant;
inline constexpr EasingFn LINEAR         = &linear;
inline constexpr EasingFn IN_BACK        = &ease::inBack;
inline constexpr EasingFn IN_BOUNCE      = &ease::inBounce;
inline constexpr EasingFn IN_CIRC        = &ease::inCirc;
inline constexpr EasingFn IN_CUBIC       = &ease::inCubic;
inline constexpr EasingFn IN_ELASTIC     = &ease::inElastic;
inline constexpr EasingFn IN_EXPO        = &ease::inExpo;
inline constexpr EasingFn IN_QUAD        = &ease::inQuad;
inline constexpr EasingFn IN_QUART       = &ease::inQuart;
inline constexpr EasingFn IN_QUINT       = &ease::inQuint;
inline constexpr EasingFn IN_SINE        = &ease::inSine;
inline constexpr EasingFn IN_OUT_BACK    = &ease::inOutBack;
inline constexpr EasingFn IN_OUT_BOUNCE  = &ease::inOutBounce;
inline constexpr EasingFn IN_OUT_CIRC    = &ease::inOutCirc;
inline constexpr EasingFn IN_OUT_CUBIC   = &ease::inOutCubic;
inline constexpr EasingFn IN_OUT_ELASTIC = &ease::inOutElastic;
inline constexpr EasingFn IN_OUT_EXPO    = &ease::inOutExpo;
inline constexpr EasingFn IN_OUT_QUAD    = &ease::inOutQuad;
inline constexpr EasingFn IN_OUT_QUART   = &ease::inOutQuart;
inline constexpr EasingFn IN_OUT_QUINT   = &ease::inOutQuint;
inline constexpr EasingFn IN_OUT_SINE    = &ease::inOutSine;
inline constexpr EasingFn OUT_BACK       = &ease::outBack;
inline constexpr EasingFn OUT_BOUNCE     = &ease::outBounce;
inline constexpr EasingFn OUT_CIRC       = &ease::outCirc;
inline constexpr EasingFn OUT_CUBIC      = &ease::outCubic;
inline constexpr EasingFn OUT_ELASTIC    = &ease::outElastic;
inline constexpr EasingFn OUT_EXPO       = &ease::outExpo;
inline constexpr EasingFn OUT_QUAD       = &ease::outQuad;
inline constexpr EasingFn OUT_QUART      = &ease::outQuart;
inline constexpr EasingFn OUT_QUINT      = &ease::outQuint;
inline constexpr EasingFn OUT_SINE       = &ease::outSine;

}  // namespace easing_type

// NOTE: EasingType.CubicBezier (the codec-decoded cubic-bezier easing) is NOT ported
// here — BinaryAnimator only ever takes a simple EasingType in practice and the bezier
// path is codec/registry-coupled. See unportedMethods.

// ── net.minecraft.util.BinaryAnimator (BinaryAnimator.java) ──────────────────
class BinaryAnimator {
public:
    // BinaryAnimator(int animationLength, EasingType easing)
    BinaryAnimator(int animationLength, EasingFn easing)
        : animationLength_(animationLength), easing_(easing) {}

    // BinaryAnimator(int animationLength) -> this(animationLength, EasingType.LINEAR)
    explicit BinaryAnimator(int animationLength)
        : BinaryAnimator(animationLength, easing_type::LINEAR) {}

    // void tick(boolean active)
    void tick(bool active) {
        ticksOld_ = ticks_;
        if (active) {
            if (ticks_ < animationLength_) {
                ticks_++;
            }
        } else if (ticks_ > 0) {
            ticks_--;
        }
    }

    // float getFactor(float partialTicks)
    float getFactor(float partialTicks) const {
        // Mth.lerp(partialTicks, ticksOld, ticks): the two int fields widen to float.
        float factor = bamth::lerpF(partialTicks,
                                    static_cast<float>(ticksOld_),
                                    static_cast<float>(ticks_))
                       / static_cast<float>(animationLength_);
        return easing_(factor);
    }

    // Test/inspection accessors (no Java counterpart; harmless).
    int ticks() const { return ticks_; }
    int ticksOld() const { return ticksOld_; }

private:
    const int animationLength_;
    const EasingFn easing_;
    int ticks_ = 0;
    int ticksOld_ = 0;
};

}  // namespace mc::util

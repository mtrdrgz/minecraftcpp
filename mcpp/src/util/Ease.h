#pragma once

// 1:1 port of net.minecraft.util.Ease (26.1.2). Pure float easing functions.
//
// These MUST match Java bit-for-bit. Conventions reproduced exactly:
//   * Mth.square(float)=x*x, Mth.cube(float)=x*x*x, Mth.sin/Mth.cos = the
//     table-based approximation (NOT std::sin/cos). Reused VERBATIM from the
//     certified mc::levelgen::mth:: header (square/cube/sin/cos), so the trig
//     and the integer/scale arithmetic stay bit-identical.
//   * Math.pow / Math.sin / Math.sqrt are java.lang.Math (libm doubles). The
//     argument promotion order is preserved precisely: a (float) constant such
//     as (float)(Math.PI*2.0/3.0) is rounded to float FIRST, then widened back
//     to double inside the multiply — we mirror that with explicit float casts.
//   * Every literal (1.70158F, 7.5625F, 0.36363637F, 2.5949094F, ...) is copied
//     VERBATIM from the decompiled source — never recomputed.
//   * Math.sqrt narrows/matches std::sqrt exactly. Math.pow / Math.sin route to
//     the host libm; after the final narrowing to float they match the JDK in
//     every sampled case, but a 1-ULP double divergence on an exotic input is
//     theoretically possible (see EasingParityTest's wide sweep / notes).
//
// Source: 26.1.2/src/net/minecraft/util/Ease.java

#include <cmath>

#include "world/level/levelgen/Mth.h"

namespace mc::util::ease {

namespace mth = mc::levelgen::mth;

// Java: java.lang.Math.PI as a double; (float)Math.PI etc. cast where the source does.
inline constexpr double JAVA_PI = 3.141592653589793;

// Ease.inBack — Ease.java:4-8
inline float inBack(float x) {
    // float c1 = 1.70158F; float c3 = 2.70158F; (unused locals in source)
    return mth::square(x) * (2.70158F * x - 1.70158F);
}

// Ease.outBounce — Ease.java:71-81  (forward-declared: inBounce/inOutBounce use it)
inline float outBounce(float x) {
    // float n1 = 7.5625F; float d1 = 2.75F; (unused locals in source)
    if (x < 0.36363637F) {
        return 7.5625F * mth::square(x);
    } else if (x < 0.72727275F) {
        return 7.5625F * mth::square(x - 0.54545456F) + 0.75F;
    } else {
        return x < 0.9090909090909091
                   ? 7.5625F * mth::square(x - 0.8181818F) + 0.9375F
                   : 7.5625F * mth::square(x - 0.95454544F) + 0.984375F;
    }
}

// Ease.inBounce — Ease.java:10-12
inline float inBounce(float x) { return 1.0F - outBounce(1.0F - x); }

// Ease.inCubic — Ease.java:14-16
inline float inCubic(float x) { return mth::cube(x); }

// Ease.inElastic — Ease.java:18-29
inline float inElastic(float x) {
    if (x == 0.0F) return 0.0F;
    if (x == 1.0F) return 1.0F;
    // float c4 = (float)(Math.PI * 2.0 / 3.0); (local unused in the return)
    return static_cast<float>(
        -std::pow(2.0, 10.0 * x - 10.0)
        * std::sin((x * 10.0 - 10.75) * static_cast<float>(JAVA_PI * 2.0 / 3.0)));
}

// Ease.inExpo — Ease.java:31-33
inline float inExpo(float x) {
    return x == 0.0F ? 0.0F : static_cast<float>(std::pow(2.0, 10.0 * x - 10.0));
}

// Ease.inQuart — Ease.java:35-37
inline float inQuart(float x) { return mth::square(mth::square(x)); }

// Ease.inQuint — Ease.java:39-41
inline float inQuint(float x) { return mth::square(mth::square(x)) * x; }

// Ease.inSine — Ease.java:43-45
inline float inSine(float x) {
    return 1.0F - mth::cos(x * static_cast<float>(JAVA_PI / 2));
}

// Ease.inOutBounce — Ease.java:47-49
inline float inOutBounce(float x) {
    return x < 0.5F ? (1.0F - outBounce(1.0F - 2.0F * x)) / 2.0F
                    : (1.0F + outBounce(2.0F * x - 1.0F)) / 2.0F;
}

// Ease.inOutCirc — Ease.java:51-53
inline float inOutCirc(float x) {
    return x < 0.5F
               ? static_cast<float>((1.0 - std::sqrt(1.0 - std::pow(2.0 * x, 2.0))) / 2.0)
               : static_cast<float>((std::sqrt(1.0 - std::pow(-2.0 * x + 2.0, 2.0)) + 1.0) / 2.0);
}

// Ease.inOutCubic — Ease.java:55-57
inline float inOutCubic(float x) {
    return x < 0.5F ? 4.0F * mth::cube(x)
                    : static_cast<float>(1.0 - std::pow(-2.0 * x + 2.0, 3.0) / 2.0);
}

// Ease.inOutQuad — Ease.java:59-61
inline float inOutQuad(float x) {
    return x < 0.5F ? 2.0F * mth::square(x)
                    : static_cast<float>(1.0 - std::pow(-2.0 * x + 2.0, 2.0) / 2.0);
}

// Ease.inOutQuart — Ease.java:63-65
inline float inOutQuart(float x) {
    return x < 0.5F ? 8.0F * mth::square(mth::square(x))
                    : static_cast<float>(1.0 - std::pow(-2.0 * x + 2.0, 4.0) / 2.0);
}

// Ease.inOutQuint — Ease.java:67-69 (note: the guard compares against 0.5 DOUBLE)
inline float inOutQuint(float x) {
    return x < 0.5 ? 16.0F * x * x * x * x * x
                   : static_cast<float>(1.0 - std::pow(-2.0 * x + 2.0, 5.0) / 2.0);
}

// Ease.outElastic — Ease.java:83-90
inline float outElastic(float x) {
    // float c4 = (float)(Math.PI * 2.0 / 3.0); (local unused in the return)
    if (x == 0.0F) {
        return 0.0F;
    } else {
        return x == 1.0F
                   ? 1.0F
                   : static_cast<float>(
                         std::pow(2.0, -10.0 * x)
                             * std::sin((x * 10.0 - 0.75) * static_cast<float>(JAVA_PI * 2.0 / 3.0))
                         + 1.0);
    }
}

// Ease.outExpo — Ease.java:92-94
inline float outExpo(float x) {
    return x == 1.0F ? 1.0F : 1.0F - static_cast<float>(std::pow(2.0, -10.0 * x));
}

// Ease.outQuad — Ease.java:96-98
inline float outQuad(float x) { return 1.0F - mth::square(1.0F - x); }

// Ease.outQuint — Ease.java:100-102
inline float outQuint(float x) {
    return 1.0F - static_cast<float>(std::pow(1.0 - x, 5.0));
}

// Ease.outSine — Ease.java:104-106
inline float outSine(float x) { return mth::sin(x * static_cast<float>(JAVA_PI / 2)); }

// Ease.inOutSine — Ease.java:108-110
inline float inOutSine(float x) {
    return -(mth::cos(static_cast<float>(JAVA_PI) * x) - 1.0F) / 2.0F;
}

// Ease.outBack — Ease.java:112-116
inline float outBack(float x) {
    // float c1 = 1.70158F; float c3 = 2.70158F; (locals unused in the return)
    return 1.0F + 2.70158F * mth::cube(x - 1.0F) + 1.70158F * mth::square(x - 1.0F);
}

// Ease.outQuart — Ease.java:118-120
inline float outQuart(float x) { return 1.0F - mth::square(mth::square(1.0F - x)); }

// Ease.outCubic — Ease.java:122-124
inline float outCubic(float x) { return 1.0F - mth::cube(1.0F - x); }

// Ease.inOutExpo — Ease.java:126-132
inline float inOutExpo(float x) {
    if (x < 0.5F) {
        return x == 0.0F ? 0.0F : static_cast<float>(std::pow(2.0, 20.0 * x - 10.0) / 2.0);
    } else {
        return x == 1.0F ? 1.0F : static_cast<float>((2.0 - std::pow(2.0, -20.0 * x + 10.0)) / 2.0);
    }
}

// Ease.inQuad — Ease.java:134-136
inline float inQuad(float x) { return x * x; }

// Ease.outCirc — Ease.java:138-140
inline float outCirc(float x) {
    return static_cast<float>(std::sqrt(1.0F - mth::square(x - 1.0F)));
}

// Ease.inOutElastic — Ease.java:142-154
inline float inOutElastic(float x) {
    // float c5 = (float)Math.PI * 4.0F / 9.0F; (local; the return re-spells it)
    if (x == 0.0F) return 0.0F;
    if (x == 1.0F) return 1.0F;
    double sin = std::sin((20.0 * x - 11.125) * (static_cast<float>(JAVA_PI) * 4.0F / 9.0F));
    return x < 0.5F
               ? static_cast<float>(-(std::pow(2.0, 20.0 * x - 10.0) * sin) / 2.0)
               : static_cast<float>(std::pow(2.0, -20.0 * x + 10.0) * sin / 2.0 + 1.0);
}

// Ease.inCirc — Ease.java:156-158
inline float inCirc(float x) {
    return static_cast<float>(-std::sqrt(1.0F - x * x)) + 1.0F;
}

// Ease.inOutBack — Ease.java:160-169
inline float inOutBack(float x) {
    // float c1 = 1.70158F; float c2 = 2.5949094F; (c1 unused in the return)
    if (x < 0.5F) {
        return 4.0F * x * x * (7.189819F * x - 2.5949094F) / 2.0F;
    }
    float dt = 2.0F * x - 2.0F;
    return (dt * dt * (3.5949094F * dt + 2.5949094F) + 2.0F) / 2.0F;
}

} // namespace mc::util::ease

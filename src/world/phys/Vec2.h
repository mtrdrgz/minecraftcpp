#pragma once

// 1:1 port of net.minecraft.world.phys.Vec2 (26.1.2) — the float-precision 2D
// vector (x, y) used for entity rotation (pitch/yaw) and various 2D math. Pure
// float arithmetic; length()/normalized() use Mth.sqrt = (float)Math.sqrt(x)
// (correctly-rounded, so bit-identical to the certified mc::levelgen::mth::sqrt).
// Certified by vec2_parity.
//
// Faithful to Vec2.java: every operation is computed in float exactly as Java
// evaluates `this.x * v.x + this.y * v.y` etc. The Codec field and equals(Object)
// are not ported (serialization / Java Object identity, not pure math) — see
// unportedMethods.

#include "../level/levelgen/Mth.h"

#include <limits>

namespace mc {

namespace mth = mc::levelgen::mth;

struct Vec2 {
    float x = 0.0f, y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    // Vec2.scale(float) — Vec2.java:28-30.
    Vec2 scale(float s) const { return Vec2(x * s, y * s); }

    // Vec2.dot(Vec2) — Vec2.java:32-34.
    float dot(const Vec2& v) const { return x * v.x + y * v.y; }

    // Vec2.add(Vec2) — Vec2.java:36-38.
    Vec2 add(const Vec2& rhs) const { return Vec2(x + rhs.x, y + rhs.y); }
    // Vec2.add(float) — Vec2.java:40-42.
    Vec2 add(float v) const { return Vec2(x + v, y + v); }

    // Vec2.equals(Vec2) — Vec2.java:44-46 (the typed overload, exact float ==).
    bool equals(const Vec2& rhs) const { return x == rhs.x && y == rhs.y; }

    // Vec2.normalized() — Vec2.java:48-51.
    Vec2 normalized() const {
        float dist = mth::sqrt(x * x + y * y);
        return dist < 1.0E-4F ? Vec2(0.0f, 0.0f) : Vec2(x / dist, y / dist);
    }

    // Vec2.length() — Vec2.java:53-55.
    float length() const { return mth::sqrt(x * x + y * y); }

    // Vec2.lengthSquared() — Vec2.java:57-59.
    float lengthSquared() const { return x * x + y * y; }

    // Vec2.distanceToSqr(Vec2) — Vec2.java:61-65.
    float distanceToSqr(const Vec2& p) const {
        float xd = p.x - x;
        float yd = p.y - y;
        return xd * xd + yd * yd;
    }

    // Vec2.negated() — Vec2.java:67-69.
    Vec2 negated() const { return Vec2(-x, -y); }

    // ── public constants — Vec2.java:9-16 ────────────────────────────────────
    static const Vec2 ZERO;
    static const Vec2 ONE;
    static const Vec2 UNIT_X;
    static const Vec2 NEG_UNIT_X;
    static const Vec2 UNIT_Y;
    static const Vec2 NEG_UNIT_Y;
    static const Vec2 MAX;   // Float.MAX_VALUE in both components
    static const Vec2 MIN;   // Float.MIN_VALUE (smallest positive subnormal) in both
};

// Float.MAX_VALUE = 3.4028235e38f; Float.MIN_VALUE = 1.4e-45f (smallest positive
// nonzero, i.e. std::numeric_limits<float>::denorm_min()).
inline const Vec2 Vec2::ZERO       = Vec2(0.0f, 0.0f);
inline const Vec2 Vec2::ONE        = Vec2(1.0f, 1.0f);
inline const Vec2 Vec2::UNIT_X     = Vec2(1.0f, 0.0f);
inline const Vec2 Vec2::NEG_UNIT_X = Vec2(-1.0f, 0.0f);
inline const Vec2 Vec2::UNIT_Y     = Vec2(0.0f, 1.0f);
inline const Vec2 Vec2::NEG_UNIT_Y = Vec2(0.0f, -1.0f);
inline const Vec2 Vec2::MAX        = Vec2(std::numeric_limits<float>::max(),
                                          std::numeric_limits<float>::max());
inline const Vec2 Vec2::MIN        = Vec2(std::numeric_limits<float>::denorm_min(),
                                          std::numeric_limits<float>::denorm_min());

} // namespace mc

#pragma once
#include <cstdint>

#include "../world/phys/Direction.h"          // mc::Direction, mc::Axis, DIRECTION_NORMAL, axisChoose
#include "../world/level/levelgen/Mth.h"       // mc::levelgen::mth::square (double)

// ---------------------------------------------------------------------------
// Port of net/minecraft/core/Vec3i.java (Minecraft Java Edition 26.1.2).
// Immutable integer 3D vector. Lives in the natural mc:: namespace.
//
// Field layout & every formula are VERBATIM from Vec3i.java:
//   getX/Y/Z                              Vec3i.java:65-75
//   offset(int,int,int) / offset(Vec3i)   Vec3i.java:92-98
//   subtract(Vec3i)                       Vec3i.java:100-102
//   multiply(int) / multiply(int,int,int) Vec3i.java:104-114
//   above/below/north/south/west/east(±)  Vec3i.java:116-162
//   relative(Direction,int)               Vec3i.java:168-172
//   relative(Direction.Axis,int)          Vec3i.java:174-183
//   cross(Vec3i)                          Vec3i.java:185-191
//   closerThan(Vec3i,double)              Vec3i.java:193-195
//   distSqr(Vec3i)                        Vec3i.java:201-203
//   distToCenterSqr(double,double,double) Vec3i.java:209-214
//   distToLowCornerSqr(double,double,…)   Vec3i.java:216-221
//   distManhattan(Vec3i)                  Vec3i.java:223-228
//   distChessboard(Vec3i)                 Vec3i.java:230-235
//   get(Direction.Axis)                   Vec3i.java:237-239
//
// NOT ported (need un-ported deps / are out of scope; hard-absent, not stubbed):
//   closerToCenterThan(Position,double)   — needs net.minecraft.core.Position
//   distToCenterSqr(Position)             — needs net.minecraft.core.Position
//   CODEC / STREAM_CODEC / offsetCodec    — serialization (skipped per task)
//   compareTo / equals / hashCode         — Comparator (skipped per task)
//   setX/Y/Z / toMutable / toString       — mutators / interop / strings
//
// Java int == int32_t. Integer arithmetic wraps two's-complement; we route the
// few overflow-prone ops (multiply, cross) through uint32_t to match Java's
// defined wraparound bit-exactly.
// ---------------------------------------------------------------------------

namespace mc {

// Mirrors Java's `int * int` (32-bit two's-complement wraparound).
constexpr int32_t vec3iIMul(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}
// Mirrors Java's `int + int` (32-bit two's-complement wraparound).
constexpr int32_t vec3iIAdd(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
// Mirrors Java's `int - int` (32-bit two's-complement wraparound).
constexpr int32_t vec3iISub(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

struct Vec3i {
    int32_t x = 0, y = 0, z = 0;

    constexpr Vec3i() = default;
    // Java: Vec3i(int,int,int) — Vec3i.java:37-41.
    constexpr Vec3i(int32_t x_, int32_t y_, int32_t z_) noexcept : x(x_), y(y_), z(z_) {}

    constexpr bool operator==(const Vec3i&) const = default;

    // Java: getX/getY/getZ — Vec3i.java:65-75.
    constexpr int32_t getX() const noexcept { return x; }
    constexpr int32_t getY() const noexcept { return y; }
    constexpr int32_t getZ() const noexcept { return z; }

    // Java: offset(int,int,int) — Vec3i.java:92-94. (The (0,0,0)->this short-circuit
    // is identity-only; the produced value is identical, so we compute directly.)
    constexpr Vec3i offset(int32_t dx, int32_t dy, int32_t dz) const noexcept {
        return Vec3i(vec3iIAdd(x, dx), vec3iIAdd(y, dy), vec3iIAdd(z, dz));
    }
    // Java: offset(Vec3i) — Vec3i.java:96-98.
    constexpr Vec3i offset(const Vec3i& v) const noexcept {
        return offset(v.x, v.y, v.z);
    }
    // Java: subtract(Vec3i) — Vec3i.java:100-102 -> offset(-x,-y,-z). Negation of
    // INT_MIN wraps in Java; route through uint to match.
    constexpr Vec3i subtract(const Vec3i& v) const noexcept {
        return offset(static_cast<int32_t>(0u - static_cast<uint32_t>(v.x)),
                      static_cast<int32_t>(0u - static_cast<uint32_t>(v.y)),
                      static_cast<int32_t>(0u - static_cast<uint32_t>(v.z)));
    }

    // Java: multiply(int) — Vec3i.java:104-110. scale==1 -> this, scale==0 -> ZERO;
    // both are identity-only optimizations producing the same value as the multiply.
    constexpr Vec3i multiply(int32_t scale) const noexcept {
        return Vec3i(vec3iIMul(x, scale), vec3iIMul(y, scale), vec3iIMul(z, scale));
    }
    // Java: multiply(int,int,int) — Vec3i.java:112-114.
    constexpr Vec3i multiply(int32_t xScale, int32_t yScale, int32_t zScale) const noexcept {
        return Vec3i(vec3iIMul(x, xScale), vec3iIMul(y, yScale), vec3iIMul(z, zScale));
    }

    // Java: relative(Direction,int) — Vec3i.java:168-172. steps==0 -> this (value-equal).
    // Uses Direction.getStepX/Y/Z == normal components (Direction.java:255-265 / 33-38).
    Vec3i relative(Direction dir, int32_t steps) const noexcept {
        const int32_t sx = DIRECTION_NORMAL[static_cast<int>(dir)][0];
        const int32_t sy = DIRECTION_NORMAL[static_cast<int>(dir)][1];
        const int32_t sz = DIRECTION_NORMAL[static_cast<int>(dir)][2];
        return Vec3i(vec3iIAdd(x, vec3iIMul(sx, steps)),
                     vec3iIAdd(y, vec3iIMul(sy, steps)),
                     vec3iIAdd(z, vec3iIMul(sz, steps)));
    }
    // Java: relative(Direction) — Vec3i.java:164-166.
    Vec3i relative(Direction dir) const noexcept { return relative(dir, 1); }

    // Java: relative(Direction.Axis,int) — Vec3i.java:174-183.
    constexpr Vec3i relative(Axis axis, int32_t steps) const noexcept {
        const int32_t xStep = axis == Axis::X ? steps : 0;
        const int32_t yStep = axis == Axis::Y ? steps : 0;
        const int32_t zStep = axis == Axis::Z ? steps : 0;
        return Vec3i(vec3iIAdd(x, xStep), vec3iIAdd(y, yStep), vec3iIAdd(z, zStep));
    }

    // Java: above/below/north/south/west/east — Vec3i.java:116-162.
    Vec3i above()              const noexcept { return relative(Direction::UP, 1); }
    Vec3i above(int32_t steps) const noexcept { return relative(Direction::UP, steps); }
    Vec3i below()              const noexcept { return relative(Direction::DOWN, 1); }
    Vec3i below(int32_t steps) const noexcept { return relative(Direction::DOWN, steps); }
    Vec3i north()              const noexcept { return relative(Direction::NORTH, 1); }
    Vec3i north(int32_t steps) const noexcept { return relative(Direction::NORTH, steps); }
    Vec3i south()              const noexcept { return relative(Direction::SOUTH, 1); }
    Vec3i south(int32_t steps) const noexcept { return relative(Direction::SOUTH, steps); }
    Vec3i west()               const noexcept { return relative(Direction::WEST, 1); }
    Vec3i west(int32_t steps)  const noexcept { return relative(Direction::WEST, steps); }
    Vec3i east()               const noexcept { return relative(Direction::EAST, 1); }
    Vec3i east(int32_t steps)  const noexcept { return relative(Direction::EAST, steps); }

    // Java: cross(Vec3i) — Vec3i.java:185-191. All products/diffs are int (wrap).
    constexpr Vec3i cross(const Vec3i& u) const noexcept {
        return Vec3i(vec3iISub(vec3iIMul(y, u.z), vec3iIMul(z, u.y)),
                     vec3iISub(vec3iIMul(z, u.x), vec3iIMul(x, u.z)),
                     vec3iISub(vec3iIMul(x, u.y), vec3iIMul(y, u.x)));
    }

    // Java: distToLowCornerSqr(double,double,double) — Vec3i.java:216-221.
    constexpr double distToLowCornerSqr(double px, double py, double pz) const noexcept {
        const double dx = static_cast<double>(x) - px;
        const double dy = static_cast<double>(y) - py;
        const double dz = static_cast<double>(z) - pz;
        return dx * dx + dy * dy + dz * dz;
    }
    // Java: distSqr(Vec3i) — Vec3i.java:201-203 -> distToLowCornerSqr(int,int,int).
    constexpr double distSqr(const Vec3i& p) const noexcept {
        return distToLowCornerSqr(static_cast<double>(p.x),
                                  static_cast<double>(p.y),
                                  static_cast<double>(p.z));
    }
    // Java: distToCenterSqr(double,double,double) — Vec3i.java:209-214.
    constexpr double distToCenterSqr(double px, double py, double pz) const noexcept {
        const double dx = static_cast<double>(x) + 0.5 - px;
        const double dy = static_cast<double>(y) + 0.5 - py;
        const double dz = static_cast<double>(z) + 0.5 - pz;
        return dx * dx + dy * dy + dz * dz;
    }

    // Java: closerThan(Vec3i,double) — Vec3i.java:193-195 -> distSqr < Mth.square(dist).
    bool closerThan(const Vec3i& p, double distance) const noexcept {
        return distSqr(p) < ::mc::levelgen::mth::square(distance);
    }

    // Java: distManhattan(Vec3i) — Vec3i.java:223-228. CRITICAL: the per-axis abs
    // differences are computed as INT then stored into FLOAT, summed as FLOAT, and
    // the float sum is truncated with a (int) cast. Replicated exactly.
    int32_t distManhattan(const Vec3i& p) const noexcept {
        const float xd = static_cast<float>(jabs(vec3iISub(p.x, this->x)));
        const float yd = static_cast<float>(jabs(vec3iISub(p.y, this->y)));
        const float zd = static_cast<float>(jabs(vec3iISub(p.z, this->z)));
        return javaF2I(xd + yd + zd);   // Java's (int)(float) SATURATES (JLS 5.1.3)
    }

    // Java: distChessboard(Vec3i) — Vec3i.java:230-235. int subtraction wraps; Math.abs
    // of the wrapped diff; max of three. (Math.abs(INT_MIN) stays INT_MIN.)
    int32_t distChessboard(const Vec3i& p) const noexcept {
        const int32_t xd = jabs(vec3iISub(this->x, p.x));
        const int32_t yd = jabs(vec3iISub(this->y, p.y));
        const int32_t zd = jabs(vec3iISub(this->z, p.z));
        return jmax(jmax(xd, yd), zd);
    }

    // Java: get(Direction.Axis) — Vec3i.java:237-239 -> axis.choose(x,y,z).
    constexpr int32_t get(Axis axis) const noexcept { return axisChoose(axis, x, y, z); }

private:
    // Java: Math.abs(int) — INT_MIN -> INT_MIN (wraps); matches uint negate.
    static constexpr int32_t jabs(int32_t v) noexcept {
        return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v;
    }
    static constexpr int32_t jmax(int32_t a, int32_t b) noexcept { return a >= b ? a : b; }
    // Java (int)(float) narrowing — JLS 5.1.3: NaN->0, out-of-range saturates to
    // Integer.MAX/MIN_VALUE, else round toward zero. (C++ static_cast<int>(float) is
    // UB out of range, so this is required for INT_MAX/MIN-magnitude sums.)
    static int32_t javaF2I(float f) noexcept {
        if (std::isnan(f)) return 0;
        const double r = std::trunc(static_cast<double>(f));
        if (r >= 2147483647.0) return INT32_MAX;
        if (r <= -2147483648.0) return INT32_MIN;
        return static_cast<int32_t>(r);
    }
};

// Java: Vec3i.ZERO — Vec3i.java:24.
inline constexpr Vec3i VEC3I_ZERO{0, 0, 0};

} // namespace mc

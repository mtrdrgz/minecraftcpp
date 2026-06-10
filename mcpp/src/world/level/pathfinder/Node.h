#pragma once

// 1:1 port of the PURE helpers of net.minecraft.world.level.pathfinder.Node
// (26.1.2). Only the bit-packing hash and the float distance helpers are ported
// here — the A* bookkeeping fields (g/h/f, cameFrom, heapIdx, etc.) and the
// stream (FriendlyByteBuf) / PathType members are intentionally omitted because
// they pull in un-ported dependencies and are not pure value functions.
//
// Bit-exactness notes (must match Java VERBATIM):
//   * createHash returns an `int` built from int bit-ops (Node.java:45-47). The
//     `(x < 0 ? Integer.MIN_VALUE : 0)` term sets the sign bit, so the result can
//     be negative; we keep it as int32_t and use unsigned shifts where Java's
//     `<<` would overflow into the sign bit (well-defined two's-complement).
//   * Each distance coordinate is `to.x - this.x` computed in `int` (two's-comp
//     wrap) and THEN implicitly widened to `float` by Java. We reproduce exactly:
//     int32_t subtraction first, then static_cast<float>. The squares/sum are
//     float arithmetic.
//   * Mth.sqrt(float) == (float)Math.sqrt((double)x) — Mth.java:57-59.
//   * Math.abs(int) is two's-complement (Math.abs(INT_MIN)==INT_MIN); for physical
//     block coords this never triggers, but we port it faithfully anyway.

#include <cmath>
#include <cstdint>

#include "core/Math.h"  // mc::BlockPos (read-only, certified)

namespace mc::pathfinder {

class Node {
public:
    const std::int32_t x;
    const std::int32_t y;
    const std::int32_t z;
    const std::int32_t hash;

    // Node(int x, int y, int z) — Node.java:24-29.
    Node(std::int32_t nx, std::int32_t ny, std::int32_t nz)
        : x(nx), y(ny), z(nz), hash(createHash(nx, ny, nz)) {}

    // Node.createHash(int x, int y, int z) — Node.java:45-47.
    //   y & 0xFF | (x & 32767) << 8 | (z & 32767) << 24
    //     | (x < 0 ? Integer.MIN_VALUE : 0) | (z < 0 ? 32768 : 0)
    // The `(z & 32767) << 24` term shifts up to bit 38 in Java int arithmetic,
    // overflowing into / past the sign bit (two's-complement wrap). Compute the
    // OR-terms in uint32_t for well-defined wrap, then reinterpret as int32_t.
    static std::int32_t createHash(std::int32_t x, std::int32_t y, std::int32_t z) {
        std::uint32_t h =
            (static_cast<std::uint32_t>(y) & 0xFFu)
            | ((static_cast<std::uint32_t>(x) & 32767u) << 8)
            | ((static_cast<std::uint32_t>(z) & 32767u) << 24)
            | (x < 0 ? static_cast<std::uint32_t>(0x80000000u) : 0u)
            | (z < 0 ? static_cast<std::uint32_t>(32768u) : 0u);
        return static_cast<std::int32_t>(h);
    }

    // Mth.sqrt(float) — Mth.java:57-59: (float)Math.sqrt(x).
    static float mthSqrt(float v) {
        return static_cast<float>(std::sqrt(static_cast<double>(v)));
    }

    // distanceTo(Node) — Node.java:49-54.
    float distanceTo(const Node& to) const {
        float xd = static_cast<float>(to.x - x);
        float yd = static_cast<float>(to.y - y);
        float zd = static_cast<float>(to.z - z);
        return mthSqrt(xd * xd + yd * yd + zd * zd);
    }

    // distanceToXZ(Node) — Node.java:56-60.
    float distanceToXZ(const Node& to) const {
        float xd = static_cast<float>(to.x - x);
        float zd = static_cast<float>(to.z - z);
        return mthSqrt(xd * xd + zd * zd);
    }

    // distanceTo(BlockPos) — Node.java:62-67.
    float distanceTo(const mc::BlockPos& pos) const {
        float xd = static_cast<float>(pos.x - x);
        float yd = static_cast<float>(pos.y - y);
        float zd = static_cast<float>(pos.z - z);
        return mthSqrt(xd * xd + yd * yd + zd * zd);
    }

    // distanceToSqr(Node) — Node.java:69-74.
    float distanceToSqr(const Node& to) const {
        float xd = static_cast<float>(to.x - x);
        float yd = static_cast<float>(to.y - y);
        float zd = static_cast<float>(to.z - z);
        return xd * xd + yd * yd + zd * zd;
    }

    // distanceToSqr(BlockPos) — Node.java:76-81.
    float distanceToSqr(const mc::BlockPos& pos) const {
        float xd = static_cast<float>(pos.x - x);
        float yd = static_cast<float>(pos.y - y);
        float zd = static_cast<float>(pos.z - z);
        return xd * xd + yd * yd + zd * zd;
    }

    // distanceManhattan(Node) — Node.java:83-88.
    // Math.abs(int) — two's-complement (abs(INT_MIN)==INT_MIN); the int diff is
    // computed first, then Math.abs, then widened to float.
    float distanceManhattan(const Node& to) const {
        float xd = static_cast<float>(javaAbs(to.x - x));
        float yd = static_cast<float>(javaAbs(to.y - y));
        float zd = static_cast<float>(javaAbs(to.z - z));
        return xd + yd + zd;
    }

    // distanceManhattan(BlockPos) — Node.java:90-95.
    float distanceManhattan(const mc::BlockPos& pos) const {
        float xd = static_cast<float>(javaAbs(pos.x - x));
        float yd = static_cast<float>(javaAbs(pos.y - y));
        float zd = static_cast<float>(javaAbs(pos.z - z));
        return xd + yd + zd;
    }

    // equals: this.hash == no.hash && x==no.x && y==no.y && z==no.z — Node.java:106-108.
    bool equals(const Node& o) const {
        return hash == o.hash && x == o.x && y == o.y && z == o.z;
    }

    // hashCode() — Node.java:111-113.
    std::int32_t hashCode() const { return hash; }

private:
    // java.lang.Math.abs(int): two's-complement, abs(INT_MIN)==INT_MIN.
    static std::int32_t javaAbs(std::int32_t v) {
        return v < 0 ? static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(v)) : v;
    }
};

}  // namespace mc::pathfinder

#pragma once

// 1:1 port of net.minecraft.world.phys.Vec3 (26.1.2) — the double-precision vector
// used throughout entity movement, raytracing, particles, and collision. Pure
// arithmetic plus Mth.cos/sin (the certified table) for rotations and Math.sqrt
// (correctly-rounded) for lengths. Certified by vec3_parity.
//
// rotation() uses java.lang.Math.atan2/asin (1-ULP-tolerant, may differ from the
// host libm) — gated separately so any divergence is visible.

#include "../level/levelgen/Mth.h"
#include "Direction.h"

#include <cmath>

namespace mc {

namespace mth = mc::levelgen::mth;

struct Vec3 {
    double x = 0, y = 0, z = 0;

    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    // ── factories from Vec3i (BlockPos) ──────────────────────────────────────
    static Vec3 atLowerCornerOf(int px, int py, int pz) { return {(double)px, (double)py, (double)pz}; }
    static Vec3 atCenterOf(int px, int py, int pz) { return {px + 0.5, py + 0.5, pz + 0.5}; }
    static Vec3 atBottomCenterOf(int px, int py, int pz) { return {px + 0.5, (double)py, pz + 0.5}; }

    Vec3 vectorTo(const Vec3& v) const { return {v.x - x, v.y - y, v.z - z}; }
    Vec3 normalize() const {
        double dist = std::sqrt(x * x + y * y + z * z);
        return dist < 1.0E-5F ? Vec3{0, 0, 0} : Vec3{x / dist, y / dist, z / dist};
    }
    double dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    Vec3 cross(const Vec3& v) const { return {y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x}; }

    Vec3 add(double ax, double ay, double az) const { return {x + ax, y + ay, z + az}; }
    Vec3 add(const Vec3& v) const { return add(v.x, v.y, v.z); }
    Vec3 add(double value) const { return add(value, value, value); }
    Vec3 subtract(double sx, double sy, double sz) const { return add(-sx, -sy, -sz); }
    Vec3 subtract(const Vec3& v) const { return subtract(v.x, v.y, v.z); }
    Vec3 subtract(double value) const { return subtract(value, value, value); }

    double distanceTo(const Vec3& v) const {
        double xd = v.x - x, yd = v.y - y, zd = v.z - z;
        return std::sqrt(xd * xd + yd * yd + zd * zd);
    }
    double distanceToSqr(const Vec3& v) const { return distanceToSqr(v.x, v.y, v.z); }
    double distanceToSqr(double px, double py, double pz) const {
        double xd = px - x, yd = py - y, zd = pz - z;
        return xd * xd + yd * yd + zd * zd;
    }

    Vec3 multiply(double xs, double ys, double zs) const { return {x * xs, y * ys, z * zs}; }
    Vec3 multiply(const Vec3& s) const { return multiply(s.x, s.y, s.z); }
    Vec3 scale(double s) const { return multiply(s, s, s); }
    Vec3 reverse() const { return scale(-1.0); }
    Vec3 horizontal() const { return {x, 0.0, z}; }

    double length() const { return std::sqrt(x * x + y * y + z * z); }
    double lengthSqr() const { return x * x + y * y + z * z; }
    double horizontalDistance() const { return std::sqrt(x * x + z * z); }
    double horizontalDistanceSqr() const { return x * x + z * z; }

    Vec3 lerp(const Vec3& v, double a) const {
        return {mth::lerp(a, x, v.x), mth::lerp(a, y, v.y), mth::lerp(a, z, v.z)};
    }

    // Mth.cos/sin take double; the float radians widens. cos/sin return float.
    Vec3 xRot(float radians) const {
        float cos = mth::cos((double)radians), sin = mth::sin((double)radians);
        return {x, y * cos + z * sin, z * cos - y * sin};
    }
    Vec3 yRot(float radians) const {
        float cos = mth::cos((double)radians), sin = mth::sin((double)radians);
        return {x * cos + z * sin, y, z * cos - x * sin};
    }
    Vec3 zRot(float radians) const {
        float cos = mth::cos((double)radians), sin = mth::sin((double)radians);
        return {x * cos + y * sin, y * cos - x * sin, z};
    }
    Vec3 rotateClockwise90() const { return {-z, y, x}; }

    static constexpr float PI_F = static_cast<float>(3.141592653589793);
    static constexpr float DEG = static_cast<float>(3.141592653589793 / 180.0);
    static Vec3 directionFromRotation(float rotX, float rotY) {
        float yCos = mth::cos((double)(-rotY * DEG - PI_F));
        float ySin = mth::sin((double)(-rotY * DEG - PI_F));
        float xCos = -mth::cos((double)(-rotX * DEG));
        float xSin = mth::sin((double)(-rotX * DEG));
        return {ySin * xCos, xSin, yCos * xCos};
    }
    // Vec2 rotation() — uses java.lang.Math.atan2/asin. Returns (pitch, yaw).
    void rotation(float& pitch, float& yaw) const {
        yaw = (float)std::atan2(-x, z) * (180.0F / PI_F);
        pitch = (float)std::asin(-y / std::sqrt(x * x + y * y + z * z)) * (180.0F / PI_F);
    }

    Vec3 with(int axis, double value) const {
        return {axis == 0 ? value : x, axis == 1 ? value : y, axis == 2 ? value : z};
    }
    double get(int axis) const { return mc::axisChoose((mc::Axis)axis, x, y, z); }
    Vec3 align(bool ax, bool ay, bool az) const {
        return {ax ? (double)mth::floor(x) : x, ay ? (double)mth::floor(y) : y, az ? (double)mth::floor(z) : z};
    }
    Vec3 relative(Direction dir, double distance) const {
        const int* n = mc::DIRECTION_NORMAL[(int)dir];
        return {x + distance * n[0], y + distance * n[1], z + distance * n[2]};
    }
    Vec3 projectedOn(const Vec3& onto) const {
        return onto.lengthSqr() == 0.0 ? onto : onto.scale(this->dot(onto)).scale(1.0 / onto.lengthSqr());
    }
    static Vec3 applyLocalCoordinatesToRotation(float rotX, float rotY, const Vec3& direction) {
        float yCos = mth::cos((double)((rotY + 90.0F) * DEG));
        float ySin = mth::sin((double)((rotY + 90.0F) * DEG));
        float xCos = mth::cos((double)(-rotX * DEG));
        float xSin = mth::sin((double)(-rotX * DEG));
        float xCosUp = mth::cos((double)((-rotX + 90.0F) * DEG));
        float xSinUp = mth::sin((double)((-rotX + 90.0F) * DEG));
        Vec3 forwards{yCos * xCos, xSin, ySin * xCos};
        Vec3 up{yCos * xCosUp, xSinUp, ySin * xCosUp};
        Vec3 left = forwards.cross(up).scale(-1.0);
        double xa = forwards.x * direction.z + up.x * direction.y + left.x * direction.x;
        double ya = forwards.y * direction.z + up.y * direction.y + left.y * direction.x;
        double za = forwards.z * direction.z + up.z * direction.y + left.z * direction.x;
        return {xa, ya, za};
    }
    bool isFinite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
};

} // namespace mc

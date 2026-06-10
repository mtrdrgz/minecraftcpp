#pragma once
#include <glm/glm.hpp>
#include <optional>
#include <vector>

#include "core/Math.h"               // mc::BlockPos
#include "world/phys/BlockHitResult.h"
#include "world/phys/Direction.h"

// ---------------------------------------------------------------------------
// Port of net/minecraft/world/phys/AABB.java (Minecraft Java Edition 26.1.2)
//
// Java methods ported in this header:
//   - AABB(double, double, double, double, double, double)   (sorts corners)
//   - AABB(Vec3, Vec3)                                       (begin/end ctor)
//   - static AABB ofSize(Vec3 center, double, double, double)
//   - double getXsize() / getYsize() / getZsize()
//   - double getSize()                                       (avg of three)
//   - AABB inflate(double xAdd, double yAdd, double zAdd)
//   - AABB inflate(double amountToAddInAllDirections)
//   - AABB deflate(double, double, double)
//   - AABB deflate(double)
//   - AABB expandTowards(double, double, double)
//   - AABB expandTowards(Vec3 delta)
//   - AABB contract(double xa, double ya, double za)
//   - AABB intersect(AABB other)
//   - AABB minmax(AABB other)
//   - AABB move(double, double, double)
//   - AABB move(Vec3 pos)
//   - boolean intersects(AABB)
//   - boolean intersects(double minX, double minY, double minZ,
//                        double maxX, double maxY, double maxZ)
//   - boolean intersects(Vec3 min, Vec3 max)
//   - boolean contains(Vec3 vec)
//   - boolean contains(double x, double y, double z)
//   - Vec3 getCenter()
//   - Vec3 getMinPosition()
//   - Vec3 getMaxPosition()
//   - double distanceToSqr(Vec3 point)
//   - double distanceToSqr(AABB boundingBox)
//   - boolean hasNaN()
//   - Optional<Vec3> clip(Vec3 from, Vec3 to)
//   - static Optional<Vec3> clip(double minX, double minY, double minZ,
//                                double maxX, double maxY, double maxZ,
//                                Vec3 from, Vec3 to)
//
// Skipped (not yet needed at this phase, will be added when callers appear):
//   - double min(Direction.Axis) / max(Direction.Axis)
//   - static BlockHitResult clip(Iterable<AABB>, Vec3, Vec3, BlockPos)
//     (+ the private getDirection/clipPoint helpers, Direction-returning)
//
// Skipped (not yet needed at this phase, will be added when callers appear):
//   - AABB(BlockPos)                              (no BlockPos overload here;
//                                                  use the 6-double ctor)
//   - static AABB of(BoundingBox)                 (BoundingBox not ported yet)
//   - static AABB unitCubeFromLowerCorner(Vec3)   (trivial; not yet needed)
//   - static AABB encapsulatingFullBlocks(BlockPos, BlockPos)
//   - setMinX/Y/Z / setMaxX/Y/Z                   (trivial; not yet needed)
//   - equals / hashCode / toString                (Java boilerplate)
//   - AABB move(BlockPos) / AABB move(Vector3f)   (no BlockPos / Vector3f
//                                                  overloads required)
//   - boolean intersects(BlockPos)                (no BlockPos overload)
//   - Vec3 getBottomCenter()                      (not yet needed)
//   - boolean collidedAlongVector(Vec3, List<AABB>)  (not yet needed)
//   - static class Builder                        (not yet needed)
// ---------------------------------------------------------------------------

namespace mc {

class AABB {
public:
    glm::dvec3 minCorner;
    glm::dvec3 maxCorner;

    // 6-double ctor; sorts the corners so min<=max on every axis.
    constexpr AABB(double x0, double y0, double z0,
                   double x1, double y1, double z1) noexcept
        : minCorner(x0 < x1 ? x0 : x1,
                    y0 < y1 ? y0 : y1,
                    z0 < z1 ? z0 : z1),
          maxCorner(x0 > x1 ? x0 : x1,
                    y0 > y1 ? y0 : y1,
                    z0 > z1 ? z0 : z1) {}

    // begin/end Vec3 ctor; also sorts.
    constexpr AABB(const glm::dvec3& a, const glm::dvec3& b) noexcept
        : minCorner(a.x < b.x ? a.x : b.x,
                    a.y < b.y ? a.y : b.y,
                    a.z < b.z ? a.z : b.z),
          maxCorner(a.x > b.x ? a.x : b.x,
                    a.y > b.y ? a.y : b.y,
                    a.z > b.z ? a.z : b.z) {}

    // Java: ofSize(Vec3 center, double sx, double sy, double sz)
    static constexpr AABB ofSize(const glm::dvec3& center,
                                 double dx, double dy, double dz) noexcept {
        return AABB(center.x - dx * 0.5, center.y - dy * 0.5, center.z - dz * 0.5,
                    center.x + dx * 0.5, center.y + dy * 0.5, center.z + dz * 0.5);
    }

    constexpr double getXsize() const noexcept { return maxCorner.x - minCorner.x; }
    constexpr double getYsize() const noexcept { return maxCorner.y - minCorner.y; }
    constexpr double getZsize() const noexcept { return maxCorner.z - minCorner.z; }

    constexpr double getSize() const noexcept {
        return (getXsize() + getYsize() + getZsize()) / 3.0;
    }

    // Java: inflate(double xAdd, double yAdd, double zAdd)
    constexpr AABB inflate(double dx, double dy, double dz) const noexcept {
        return AABB(minCorner.x - dx, minCorner.y - dy, minCorner.z - dz,
                    maxCorner.x + dx, maxCorner.y + dy, maxCorner.z + dz);
    }
    constexpr AABB inflate(double v) const noexcept { return inflate(v, v, v); }

    // Java: deflate(double, double, double) = inflate(negated)
    constexpr AABB deflate(double dx, double dy, double dz) const noexcept {
        return inflate(-dx, -dy, -dz);
    }
    constexpr AABB deflate(double v) const noexcept { return inflate(-v); }

    // Java: expandTowards(double xa, double ya, double za) — push the corresponding
    // side outward in the direction of the delta.
    AABB expandTowards(double dx, double dy, double dz) const noexcept;

    // Java: contract(double xa, double ya, double za) — opposite of expandTowards.
    AABB contract(double dx, double dy, double dz) const noexcept;

    // Java: intersect(AABB)
    constexpr AABB intersect(const AABB& other) const noexcept {
        return AABB(minCorner.x > other.minCorner.x ? minCorner.x : other.minCorner.x,
                    minCorner.y > other.minCorner.y ? minCorner.y : other.minCorner.y,
                    minCorner.z > other.minCorner.z ? minCorner.z : other.minCorner.z,
                    maxCorner.x < other.maxCorner.x ? maxCorner.x : other.maxCorner.x,
                    maxCorner.y < other.maxCorner.y ? maxCorner.y : other.maxCorner.y,
                    maxCorner.z < other.maxCorner.z ? maxCorner.z : other.maxCorner.z);
    }

    // Java: minmax(AABB) — union (encapsulating box).
    constexpr AABB minmax(const AABB& other) const noexcept {
        return AABB(minCorner.x < other.minCorner.x ? minCorner.x : other.minCorner.x,
                    minCorner.y < other.minCorner.y ? minCorner.y : other.minCorner.y,
                    minCorner.z < other.minCorner.z ? minCorner.z : other.minCorner.z,
                    maxCorner.x > other.maxCorner.x ? maxCorner.x : other.maxCorner.x,
                    maxCorner.y > other.maxCorner.y ? maxCorner.y : other.maxCorner.y,
                    maxCorner.z > other.maxCorner.z ? maxCorner.z : other.maxCorner.z);
    }

    // Java: move(double, double, double)
    constexpr AABB move(double dx, double dy, double dz) const noexcept {
        return AABB(minCorner.x + dx, minCorner.y + dy, minCorner.z + dz,
                    maxCorner.x + dx, maxCorner.y + dy, maxCorner.z + dz);
    }
    // Java: move(Vec3)
    constexpr AABB move(const glm::dvec3& v) const noexcept {
        return move(v.x, v.y, v.z);
    }

    // Java: intersects(AABB)
    constexpr bool intersects(const AABB& other) const noexcept {
        return minCorner.x < other.maxCorner.x && maxCorner.x > other.minCorner.x
            && minCorner.y < other.maxCorner.y && maxCorner.y > other.minCorner.y
            && minCorner.z < other.maxCorner.z && maxCorner.z > other.minCorner.z;
    }

    // Java: intersects(Vec3 min, Vec3 max) — does NOT assume min<=max; sorts.
    constexpr bool intersects(const glm::dvec3& a, const glm::dvec3& b) const noexcept {
        const double aMinX = a.x < b.x ? a.x : b.x;
        const double aMinY = a.y < b.y ? a.y : b.y;
        const double aMinZ = a.z < b.z ? a.z : b.z;
        const double aMaxX = a.x > b.x ? a.x : b.x;
        const double aMaxY = a.y > b.y ? a.y : b.y;
        const double aMaxZ = a.z > b.z ? a.z : b.z;
        return minCorner.x < aMaxX && maxCorner.x > aMinX
            && minCorner.y < aMaxY && maxCorner.y > aMinY
            && minCorner.z < aMaxZ && maxCorner.z > aMinZ;
    }

    // Java: contains(Vec3)
    constexpr bool contains(const glm::dvec3& p) const noexcept {
        return p.x >= minCorner.x && p.x < maxCorner.x
            && p.y >= minCorner.y && p.y < maxCorner.y
            && p.z >= minCorner.z && p.z < maxCorner.z;
    }

    // Java: getCenter()  — Mth.lerp(0.5, a, b) == (a + b) * 0.5
    constexpr glm::dvec3 getCenter() const noexcept {
        return glm::dvec3((minCorner.x + maxCorner.x) * 0.5,
                          (minCorner.y + maxCorner.y) * 0.5,
                          (minCorner.z + maxCorner.z) * 0.5);
    }

    constexpr glm::dvec3 getMinPosition() const noexcept { return minCorner; }
    constexpr glm::dvec3 getMaxPosition() const noexcept { return maxCorner; }

    // Java: double min(Direction.Axis) = axis.choose(minX, minY, minZ);
    //       double max(Direction.Axis) = axis.choose(maxX, maxY, maxZ).
    constexpr double min(Axis axis) const noexcept {
        return axisChoose(axis, minCorner.x, minCorner.y, minCorner.z);
    }
    constexpr double max(Axis axis) const noexcept {
        return axisChoose(axis, maxCorner.x, maxCorner.y, maxCorner.z);
    }

    // Java: distanceToSqr(Vec3) — minimum squared distance from point to AABB.
    double distanceToSqr(const glm::dvec3& v) const noexcept;

    // Java: distanceToSqr(AABB) — minimum squared distance between two AABBs.
    double distanceToSqr(const AABB& other) const noexcept;

    // Java: hasNaN()
    bool hasNaN() const noexcept;

    // Java: clip(Vec3 from, Vec3 to)
    std::optional<glm::dvec3> clip(const glm::dvec3& from, const glm::dvec3& to) const noexcept;

    // Static variant matching Java's static clip(minX..maxZ, from, to).
    static std::optional<glm::dvec3> clip(double minX, double minY, double minZ,
                                          double maxX, double maxY, double maxZ,
                                          const glm::dvec3& from,
                                          const glm::dvec3& to) noexcept;

    // Java: static @Nullable BlockHitResult clip(Iterable<AABB>, Vec3 from,
    // Vec3 to, BlockPos pos) — AABB.java:310-327. Null is std::nullopt.
    static std::optional<BlockHitResult> clip(const std::vector<AABB>& aabbs,
                                              const glm::dvec3& from,
                                              const glm::dvec3& to,
                                              const BlockPos& pos) noexcept;

    // Matches Java's EPSILON constant.
    static constexpr double EPSILON = 1.0e-7;
};

} // namespace mc

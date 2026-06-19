#include "AABB.h"

#include <algorithm>
#include <cmath>

namespace mc {

// ---------------------------------------------------------------------------
// expandTowards / contract: shift one side outward (or inward) along the delta.
// Faithful port of AABB.java#expandTowards(xa, ya, za) and contract(xa, ya, za).
// ---------------------------------------------------------------------------

AABB AABB::expandTowards(double dx, double dy, double dz) const noexcept {
    double mnX = minCorner.x;
    double mnY = minCorner.y;
    double mnZ = minCorner.z;
    double mxX = maxCorner.x;
    double mxY = maxCorner.y;
    double mxZ = maxCorner.z;
    if (dx < 0.0)      mnX += dx;
    else if (dx > 0.0) mxX += dx;
    if (dy < 0.0)      mnY += dy;
    else if (dy > 0.0) mxY += dy;
    if (dz < 0.0)      mnZ += dz;
    else if (dz > 0.0) mxZ += dz;
    return AABB(mnX, mnY, mnZ, mxX, mxY, mxZ);
}

AABB AABB::contract(double dx, double dy, double dz) const noexcept {
    double mnX = minCorner.x;
    double mnY = minCorner.y;
    double mnZ = minCorner.z;
    double mxX = maxCorner.x;
    double mxY = maxCorner.y;
    double mxZ = maxCorner.z;
    if (dx < 0.0)      mnX -= dx;
    else if (dx > 0.0) mxX -= dx;
    if (dy < 0.0)      mnY -= dy;
    else if (dy > 0.0) mxY -= dy;
    if (dz < 0.0)      mnZ -= dz;
    else if (dz > 0.0) mxZ -= dz;
    return AABB(mnX, mnY, mnZ, mxX, mxY, mxZ);
}

// ---------------------------------------------------------------------------
// distanceToSqr — Mth.lengthSquared(dx, dy, dz) is just dx*dx + dy*dy + dz*dz.
// ---------------------------------------------------------------------------

double AABB::distanceToSqr(const glm::dvec3& v) const noexcept {
    const double dx = std::max(std::max(minCorner.x - v.x, v.x - maxCorner.x), 0.0);
    const double dy = std::max(std::max(minCorner.y - v.y, v.y - maxCorner.y), 0.0);
    const double dz = std::max(std::max(minCorner.z - v.z, v.z - maxCorner.z), 0.0);
    return dx * dx + dy * dy + dz * dz;
}

double AABB::distanceToSqr(const AABB& other) const noexcept {
    const double dx = std::max(std::max(minCorner.x - other.maxCorner.x,
                                        other.minCorner.x - maxCorner.x), 0.0);
    const double dy = std::max(std::max(minCorner.y - other.maxCorner.y,
                                        other.minCorner.y - maxCorner.y), 0.0);
    const double dz = std::max(std::max(minCorner.z - other.maxCorner.z,
                                        other.minCorner.z - maxCorner.z), 0.0);
    return dx * dx + dy * dy + dz * dz;
}

bool AABB::hasNaN() const noexcept {
    return std::isnan(minCorner.x) || std::isnan(minCorner.y) || std::isnan(minCorner.z)
        || std::isnan(maxCorner.x) || std::isnan(maxCorner.y) || std::isnan(maxCorner.z);
}

// ---------------------------------------------------------------------------
// Ray clip — slab method with EPSILON, faithful to AABB.java#clip and the
// private getDirection/clipPoint helpers (AABB.java:290-395), including the
// Direction-returning forms used by the Iterable<AABB> overload.
// ---------------------------------------------------------------------------

namespace {
// Sentinel standing in for Java's @Nullable Direction.
constexpr int NO_DIRECTION = -1;

// Java: private static @Nullable Direction clipPoint(...) — AABB.java:370-395.
// Tests a slab face plane perpendicular to axis A; 'point' is the plane's
// coordinate on axis A; (minB, maxB)/(minC, maxC) bound the other two axes;
// (da, db, dc) is the ray delta and (fromA, fromB, fromC) the origin, both
// reordered to (A, B, C).
int clipPoint(double scaleReference[1], int direction,
              double da, double db, double dc,
              double point,
              double minB, double maxB,
              double minC, double maxC,
              int newDirection,
              double fromA, double fromB, double fromC) noexcept {
    const double s  = (point - fromA) / da;
    const double pb = fromB + s * db;
    const double pc = fromC + s * dc;
    if (0.0 < s && s < scaleReference[0]
        && minB - AABB::EPSILON < pb && pb < maxB + AABB::EPSILON
        && minC - AABB::EPSILON < pc && pc < maxC + AABB::EPSILON) {
        scaleReference[0] = s;
        return newDirection;
    }
    return direction;
}

// Java: private static @Nullable Direction getDirection(...) — AABB.java:335-368.
int getDirection(double minX, double minY, double minZ,
                 double maxX, double maxY, double maxZ,
                 const glm::dvec3& from, double scaleReference[1], int direction,
                 double dx, double dy, double dz) noexcept {
    using D = mc::Direction;
    if (dx > AABB::EPSILON) {
        direction = clipPoint(scaleReference, direction, dx, dy, dz,
                              minX, minY, maxY, minZ, maxZ,
                              static_cast<int>(D::WEST), from.x, from.y, from.z);
    } else if (dx < -AABB::EPSILON) {
        direction = clipPoint(scaleReference, direction, dx, dy, dz,
                              maxX, minY, maxY, minZ, maxZ,
                              static_cast<int>(D::EAST), from.x, from.y, from.z);
    }

    if (dy > AABB::EPSILON) {
        direction = clipPoint(scaleReference, direction, dy, dz, dx,
                              minY, minZ, maxZ, minX, maxX,
                              static_cast<int>(D::DOWN), from.y, from.z, from.x);
    } else if (dy < -AABB::EPSILON) {
        direction = clipPoint(scaleReference, direction, dy, dz, dx,
                              maxY, minZ, maxZ, minX, maxX,
                              static_cast<int>(D::UP), from.y, from.z, from.x);
    }

    if (dz > AABB::EPSILON) {
        direction = clipPoint(scaleReference, direction, dz, dx, dy,
                              minZ, minX, maxX, minY, maxY,
                              static_cast<int>(D::NORTH), from.z, from.x, from.y);
    } else if (dz < -AABB::EPSILON) {
        direction = clipPoint(scaleReference, direction, dz, dx, dy,
                              maxZ, minX, maxX, minY, maxY,
                              static_cast<int>(D::SOUTH), from.z, from.x, from.y);
    }

    return direction;
}
} // namespace

// Java: static Optional<Vec3> clip(minX..maxZ, from, to) — AABB.java:294-308.
std::optional<glm::dvec3> AABB::clip(double minX, double minY, double minZ,
                                     double maxX, double maxY, double maxZ,
                                     const glm::dvec3& from,
                                     const glm::dvec3& to) noexcept {
    double scaleReference[1] = {1.0};
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    const double dz = to.z - from.z;
    int direction = getDirection(minX, minY, minZ, maxX, maxY, maxZ,
                                 from, scaleReference, NO_DIRECTION, dx, dy, dz);
    if (direction == NO_DIRECTION) return std::nullopt;
    const double scale = scaleReference[0];
    return glm::dvec3(from.x + scale * dx, from.y + scale * dy, from.z + scale * dz);
}

std::optional<glm::dvec3> AABB::clip(const glm::dvec3& from,
                                     const glm::dvec3& to) const noexcept {
    return clip(minCorner.x, minCorner.y, minCorner.z,
                maxCorner.x, maxCorner.y, maxCorner.z, from, to);
}

// Java: static @Nullable BlockHitResult clip(Iterable<AABB>, Vec3, Vec3, BlockPos)
// — AABB.java:310-327.
std::optional<BlockHitResult> AABB::clip(const std::vector<AABB>& aabbs,
                                         const glm::dvec3& from,
                                         const glm::dvec3& to,
                                         const BlockPos& pos) noexcept {
    double scaleReference[1] = {1.0};
    int direction = NO_DIRECTION;
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    const double dz = to.z - from.z;

    for (const AABB& aabb : aabbs) {
        // Java: getDirection(aabb.move(pos), from, ...) — AABB.java:318,329-333.
        const AABB moved = aabb.move(pos.x, pos.y, pos.z);
        direction = getDirection(moved.minCorner.x, moved.minCorner.y, moved.minCorner.z,
                                 moved.maxCorner.x, moved.maxCorner.y, moved.maxCorner.z,
                                 from, scaleReference, direction, dx, dy, dz);
    }

    if (direction == NO_DIRECTION) return std::nullopt;
    const double scale = scaleReference[0];
    return BlockHitResult(glm::dvec3(from.x + scale * dx, from.y + scale * dy,
                                     from.z + scale * dz),
                          static_cast<Direction>(direction), pos, false);
}

// Java: collidedAlongVector(Vec3 vector, List<AABB> aabbs) — AABB.java:397-413.
bool AABB::collidedAlongVector(const glm::dvec3& vector, const std::vector<AABB>& aabbs) const noexcept {
    const glm::dvec3 from = getCenter();
    const glm::dvec3 to(from.x + vector.x, from.y + vector.y, from.z + vector.z);
    for (const AABB& shapePart : aabbs) {
        const AABB inflated = shapePart.inflate(getXsize() * 0.5 - EPSILON,
                                                getYsize() * 0.5 - EPSILON,
                                                getZsize() * 0.5 - EPSILON);
        if (inflated.contains(to) || inflated.contains(from)) return true;
        if (inflated.clip(from, to).has_value()) return true;
    }
    return false;
}

} // namespace mc

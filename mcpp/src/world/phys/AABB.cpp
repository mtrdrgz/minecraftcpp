#include "AABB.h"

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
// private getDirection/clipPoint helpers. The Direction return value from the
// Java version is dropped here; we only return the hit point as an Optional,
// matching the public Optional<Vec3> clip(from, to) entrypoint.
// ---------------------------------------------------------------------------

// Tracks: did we find a hit? what's the current minimum scale [0, 1]?
struct ClipState {
    double scale = 1.0;
    bool   hit   = false;
};

// Mirrors Java's clipPoint(): test a slab face plane perpendicular to axis A.
// 'point' is the plane's coordinate on axis A; (minB, maxB) and (minC, maxC)
// are the bounds on the other two axes. (da, db, dc) is the ray delta and
// (fromA, fromB, fromC) is the ray origin, both reordered to (A, B, C).
static void clipPoint(ClipState& state,
                      double da, double db, double dc,
                      double point,
                      double minB, double maxB,
                      double minC, double maxC,
                      double fromA, double fromB, double fromC) noexcept {
    const double s  = (point - fromA) / da;
    const double pb = fromB + s * db;
    const double pc = fromC + s * dc;
    if (0.0 < s && s < state.scale
        && minB - AABB::EPSILON < pb && pb < maxB + AABB::EPSILON
        && minC - AABB::EPSILON < pc && pc < maxC + AABB::EPSILON) {
        state.scale = s;
        state.hit   = true;
    }
}

std::optional<glm::dvec3> AABB::clip(double minX, double minY, double minZ,
                                     double maxX, double maxY, double maxZ,
                                     const glm::dvec3& from,
                                     const glm::dvec3& to) noexcept {
    ClipState state;
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    const double dz = to.z - from.z;

    // X slabs
    if (dx > EPSILON) {
        clipPoint(state, dx, dy, dz, minX, minY, maxY, minZ, maxZ, from.x, from.y, from.z);
    } else if (dx < -EPSILON) {
        clipPoint(state, dx, dy, dz, maxX, minY, maxY, minZ, maxZ, from.x, from.y, from.z);
    }

    // Y slabs (axis reorder: A=y, B=z, C=x — matches Java)
    if (dy > EPSILON) {
        clipPoint(state, dy, dz, dx, minY, minZ, maxZ, minX, maxX, from.y, from.z, from.x);
    } else if (dy < -EPSILON) {
        clipPoint(state, dy, dz, dx, maxY, minZ, maxZ, minX, maxX, from.y, from.z, from.x);
    }

    // Z slabs (axis reorder: A=z, B=x, C=y — matches Java)
    if (dz > EPSILON) {
        clipPoint(state, dz, dx, dy, minZ, minX, maxX, minY, maxY, from.z, from.x, from.y);
    } else if (dz < -EPSILON) {
        clipPoint(state, dz, dx, dy, maxZ, minX, maxX, minY, maxY, from.z, from.x, from.y);
    }

    if (!state.hit) return std::nullopt;
    return glm::dvec3(from.x + state.scale * dx,
                      from.y + state.scale * dy,
                      from.z + state.scale * dz);
}

std::optional<glm::dvec3> AABB::clip(const glm::dvec3& from,
                                     const glm::dvec3& to) const noexcept {
    return clip(minCorner.x, minCorner.y, minCorner.z,
                maxCorner.x, maxCorner.y, maxCorner.z, from, to);
}

} // namespace mc

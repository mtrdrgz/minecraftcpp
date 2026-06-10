#pragma once

// Partial 1:1 port of net.minecraft.client.resources.model.cuboid.FaceBakery
// (26.1.2) — the geometry helpers used when baking a block-model element face into
// a quad. These build directly on the certified render/model/Joml.h (transformPosition,
// GeometryUtils.normal, Vector3f.dot are bit-exact), so they are deterministic.
// Certified by face_bakery_parity.
//
// Ported here (the bounded, world-free helpers):
//   cornerToCenter / centerToCorner   (FaceBakery.java:168-174)
//   rotateVertexBy(vertex, origin, M) (FaceBakery.java:176-180)
//   findClosestDirection(direction)   (FaceBakery.java:188-205)
//   calculateFacing(positions[0..2])  (FaceBakery.java:182-186)
// NOT yet ported (need FaceInfo corner tables + BlockElementRotation + UV pipeline):
//   bakeVertex / bakeQuad / recalculateWinding.

#include "Joml.h"

#include <cmath>

namespace mc::render::model {

namespace fb {

using joml::Matrix4f;
using joml::Vector3f;

// net.minecraft.core.Direction.getUnitVec3f() for ordinals DOWN..EAST (exact).
inline const Vector3f DIRECTION_UNIT_VEC[6] = {
    Vector3f{0.f, -1.f, 0.f},  // DOWN
    Vector3f{0.f, 1.f, 0.f},   // UP
    Vector3f{0.f, 0.f, -1.f},  // NORTH
    Vector3f{0.f, 0.f, 1.f},   // SOUTH
    Vector3f{-1.f, 0.f, 0.f},  // WEST
    Vector3f{1.f, 0.f, 0.f},   // EAST
};

inline float cornerToCenter(float value) { return value - 0.5F; }
inline float centerToCorner(float value) { return value + 0.5F; }

// FaceBakery.rotateVertexBy: vertex.sub(origin); M.transformPosition(vertex); vertex.add(origin).
inline void rotateVertexBy(Vector3f& vertex, const Vector3f& origin, const Matrix4f& transformation) {
    vertex.sub(origin);
    transformation.transformPosition(vertex);
    vertex.add(origin);
}

inline bool isFiniteV3(const Vector3f& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// FaceBakery.findClosestDirection → Direction ordinal, or -1 for null.
inline int findClosestDirection(const Vector3f& direction) {
    if (!isFiniteV3(direction)) return -1;
    int best = -1;
    float closestProduct = 0.0F;
    for (int candidate = 0; candidate < 6; ++candidate) {
        float product = direction.dot(DIRECTION_UNIT_VEC[candidate]);
        if (product >= 0.0F && product > closestProduct) {
            closestProduct = product;
            best = candidate;
        }
    }
    return best;
}

// FaceBakery.calculateFacing: normal from positions[0..2], then findClosestDirection.
inline int calculateFacing(const Vector3f& p0, const Vector3f& p1, const Vector3f& p2) {
    Vector3f normal;
    joml::geometryNormal(p0, p1, p2, normal);
    return findClosestDirection(normal);
}

} // namespace fb

} // namespace mc::render::model

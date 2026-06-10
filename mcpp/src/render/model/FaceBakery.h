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
#include <utility>

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

// ── net.minecraft.client.renderer.FaceInfo corner tables (FaceInfo.java:11-105) ──
// Extent ordinals: MIN_X=0,MIN_Y=1,MIN_Z=2,MAX_X=3,MAX_Y=4,MAX_Z=5.
// FACE_INFO[facing][vertex] = {xFace, yFace, zFace} extents; facing = Direction ordinal.
inline constexpr int FACE_INFO[6][4][3] = {
    // DOWN
    {{0,1,5},{0,1,2},{3,1,2},{3,1,5}},
    // UP
    {{0,4,2},{0,4,5},{3,4,5},{3,4,2}},
    // NORTH
    {{3,4,2},{3,1,2},{0,1,2},{0,4,2}},
    // SOUTH
    {{0,4,5},{0,1,5},{3,1,5},{3,4,5}},
    // WEST
    {{0,4,2},{0,1,2},{0,1,5},{0,4,5}},
    // EAST
    {{3,4,5},{3,1,5},{3,1,2},{3,4,2}},
};
// FaceInfo.Extent.select(min,max).
inline float extentSelect(int extent, const Vector3f& mn, const Vector3f& mx) {
    switch (extent) {
        case 0: return mn.x; case 1: return mn.y; case 2: return mn.z;
        case 3: return mx.x; case 4: return mx.y; default: return mx.z; // 5
    }
}
// FaceInfo.VertexInfo.select(min,max) -> corner position.
inline Vector3f faceInfoSelect(int facing, int index, const Vector3f& from, const Vector3f& to) {
    const int* e = FACE_INFO[facing][index];
    return Vector3f{extentSelect(e[0], from, to), extentSelect(e[1], from, to), extentSelect(e[2], from, to)};
}

inline const Vector3f BLOCK_MIDDLE{0.5f, 0.5f, 0.5f};

// FaceBakery.bakeVertex — POSITION path only (FaceBakery.java:141-149,164). The UV
// path (CuboidFace.getU/getV + uvRotation + uvTransform) is not yet ported.
//   hasElement -> apply rotateVertexBy(vertex, elementOrigin, elementTransform)
//   hasModel   -> apply rotateVertexBy(vertex, BLOCK_MIDDLE, modelMatrix)
inline Vector3f bakeVertexPosition(int facing, int index, const Vector3f& from, const Vector3f& to,
                                   bool hasElement, const Vector3f& elementOrigin, const Matrix4f& elementTransform,
                                   bool hasModel, const Matrix4f& modelMatrix) {
    Vector3f vertex = faceInfoSelect(facing, index, from, to);
    vertex.div(16.0F);
    if (hasElement) rotateVertexBy(vertex, elementOrigin, elementTransform);
    if (hasModel)   rotateVertexBy(vertex, BLOCK_MIDDLE, modelMatrix);
    return vertex;
}

// ── CuboidFace UV pipeline (CuboidFace.java:18-24,76-86; Quadrant.rotateVertexIndex) ──
struct UVs {
    float minU, minV, maxU, maxV;
    // CuboidFace.UVs.getVertexU/getVertexV.
    float getVertexU(int index) const { return (index != 0 && index != 1) ? maxU : minU; }
    float getVertexV(int index) const { return (index != 0 && index != 3) ? maxV : minV; }
};
// Quadrant.rotateVertexIndex(index) = (index + shift) % 4, shift = quadrant ordinal.
inline int quadrantRotateVertexIndex(int shift, int index) { return (index + shift) % 4; }
// CuboidFace.getU/getV.
inline float cuboidFaceGetU(const UVs& uvs, int uvShift, int vertex) { return uvs.getVertexU(quadrantRotateVertexIndex(uvShift, vertex)) / 16.0F; }
inline float cuboidFaceGetV(const UVs& uvs, int uvShift, int index)  { return uvs.getVertexV(quadrantRotateVertexIndex(uvShift, index)) / 16.0F; }

// FaceBakery.bakeVertex — UV path (FaceBakery.java:151-162). Returns the (u,v) before
// the atlas-sprite mapping (sprite.getU/getV is texture-dependent, not ported here).
inline void bakeVertexUV(const UVs& uvs, int uvShift, int index, bool hasUvTransform,
                         const Matrix4f& uvTransform, float& outU, float& outV) {
    float rawU = cuboidFaceGetU(uvs, uvShift, index);
    float rawV = cuboidFaceGetV(uvs, uvShift, index);
    if (!hasUvTransform) { outU = rawU; outV = rawV; return; }
    Vector3f t{cornerToCenter(rawU), cornerToCenter(rawV), 0.0F};
    uvTransform.transformPosition(t);
    outU = centerToCorner(t.x);
    outV = centerToCorner(t.y);
}

// FaceInfo.Extent.select(minX,minY,minZ,maxX,maxY,maxZ) — the 6-arg form (FaceInfo.java:89-98).
inline float extentSelect6(int e, float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
    switch (e) {
        case 0: return minX; case 1: return minY; case 2: return minZ;
        case 3: return maxX; case 4: return maxY; default: return maxZ; // 5
    }
}
// FaceBakery.findVertex — first index >= start whose position exactly equals (x,y,z), else -1.
inline int findVertexEq(const Vector3f pos[4], int start, float x, float y, float z) {
    for (int i = start; i < 4; ++i)
        if (x == pos[i].x && y == pos[i].y && z == pos[i].z) return i;
    return -1;
}
// FaceBakery.recalculateWinding (FaceBakery.java:207-262) — reorders the 4 face
// vertices (and a parallel perm/uv array) into FaceInfo's canonical winding for the
// face's axis-aligned bounding corners. Returns false if a corner can't be matched
// (the Java throws IllegalStateException).
inline bool recalculateWinding(Vector3f pos[4], int perm[4], int facing) {
    float minX = 999.0F, minY = 999.0F, minZ = 999.0F, maxX = -999.0F, maxY = -999.0F, maxZ = -999.0F;
    for (int i = 0; i < 4; ++i) {
        float x = pos[i].x, y = pos[i].y, z = pos[i].z;
        if (x < minX) minX = x; if (y < minY) minY = y; if (z < minZ) minZ = z;
        if (x > maxX) maxX = x; if (y > maxY) maxY = y; if (z > maxZ) maxZ = z;
    }
    for (int vertex = 0; vertex < 4; ++vertex) {
        const int* e = FACE_INFO[facing][vertex];
        float newX = extentSelect6(e[0], minX, minY, minZ, maxX, maxY, maxZ);
        float newY = extentSelect6(e[1], minX, minY, minZ, maxX, maxY, maxZ);
        float newZ = extentSelect6(e[2], minX, minY, minZ, maxX, maxY, maxZ);
        int swapIdx = findVertexEq(pos, vertex, newX, newY, newZ);
        if (swapIdx == -1) return false;
        if (swapIdx != vertex) {
            std::swap(pos[swapIdx], pos[vertex]);
            std::swap(perm[swapIdx], perm[vertex]);
        }
    }
    return true;
}

} // namespace fb

} // namespace mc::render::model

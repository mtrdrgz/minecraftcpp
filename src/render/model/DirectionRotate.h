#pragma once

// 1:1 port of net.minecraft.core.Direction.rotate(Matrix4fc, Direction) +
// Direction.getApproximateNearest(float,float,float) (Direction.java:121-125, 315-328) —
// used by UnbakedCuboidGeometry.bake to remap a face's cullForDirection under the model
// state's transform. Builds on the certified render/model/Joml.h (transformDirection).
//
// Direction ordinals: DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5 (= VALUES order).
//   normalVec3f[dir] = the float unit vector (== FaceBakery DIRECTION_UNIT_VEC).
//   normal (Vec3i) has exactly one nonzero (+/-1) component, so the dot is exactly one of
//   +/-{dx,dy,dz} — no summation rounding.
//
// CRITICAL: getApproximateNearest seeds highestDot = Float.MIN_VALUE, which in Java is the
// smallest positive *subnormal* (~1.4e-45 == C++ denorm_min()), NOT numeric_limits::min()
// (the smallest *normalized*, ~1.18e-38). They differ; using min() would be a 1:1 bug.

#include "Joml.h"

#include <limits>

namespace mc::render::model {

namespace dir {

using joml::Matrix4f;
using joml::Vector3f;

// Direction.normalVec3f for ordinals DOWN..EAST (Vector3f of the int normal).
inline const Vector3f NORMAL_VEC3F[6] = {
    Vector3f{0.f, -1.f, 0.f},  // DOWN
    Vector3f{0.f, 1.f, 0.f},   // UP
    Vector3f{0.f, 0.f, -1.f},  // NORTH
    Vector3f{0.f, 0.f, 1.f},   // SOUTH
    Vector3f{-1.f, 0.f, 0.f},  // WEST
    Vector3f{1.f, 0.f, 0.f},   // EAST
};
// Direction.normal (Vec3i) integer components, same ordinal order.
inline constexpr int NORMAL_I[6][3] = {
    {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0},
};

// Direction.getApproximateNearest(dx,dy,dz) — argmax over VALUES of the signed normal dot,
// seeded NORTH / Float.MIN_VALUE. Returns a Direction ordinal (0..5).
inline int getApproximateNearest(float dx, float dy, float dz) {
    int result = 2;  // NORTH
    float highestDot = std::numeric_limits<float>::denorm_min();  // == Java Float.MIN_VALUE
    for (int d = 0; d < 6; ++d) {
        float dot = dx * (float)NORMAL_I[d][0] + dy * (float)NORMAL_I[d][1] + dz * (float)NORMAL_I[d][2];
        if (dot > highestDot) {
            highestDot = dot;
            result = d;
        }
    }
    return result;
}

// Direction.rotate(matrix, facing): transformDirection(facing.normalVec3f) then nearest.
inline int rotate(const Matrix4f& matrix, int facing) {
    Vector3f vec;
    matrix.transformDirection(NORMAL_VEC3F[facing], vec);
    return getApproximateNearest(vec.x, vec.y, vec.z);
}

}  // namespace dir

}  // namespace mc::render::model

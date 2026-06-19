#pragma once

// 1:1 port of net.minecraft.client.resources.model.cuboid.CuboidRotation (26.1.2) —
// the block-model element rotation transform (the "rotation" object on a model
// element: origin/axis/angle/rescale). Builds the Matrix4f that FaceBakery.bakeVertex
// applies via rotateVertexBy(vertex, origin, transform). Pure float math on the
// certified render/model/Joml.h. Certified by cuboid_rotation_parity.

#include "Joml.h"

#include <algorithm>
#include <cmath>

namespace mc::render::model::cuboid {

using joml::Matrix4f;
using joml::Vector3f;

// (float)(java.lang.Math.PI / 180.0) — the exact constant CuboidRotation uses.
inline constexpr float DEG_TO_RAD = static_cast<float>(3.141592653589793 / 180.0);

// Direction.Axis.getPositive().getUnitVec3f(): X->EAST, Y->UP, Z->SOUTH.
inline const Vector3f AXIS_POSITIVE_UNIT[3] = {
    Vector3f{1.f, 0.f, 0.f}, Vector3f{0.f, 1.f, 0.f}, Vector3f{0.f, 0.f, 1.f},
};

// SingleAxisRotation.transformation() — angle==0 -> identity, else rotation(rad, axis).
inline Matrix4f singleAxisTransformation(int axis, float angleDeg) {
    Matrix4f result;
    if (angleDeg == 0.0F) return result;
    const Vector3f& u = AXIS_POSITIVE_UNIT[axis];
    result.rotation(angleDeg * DEG_TO_RAD, u.x, u.y, u.z);
    return result;
}

// EulerXYZRotation.transformation() — rotationZYX(z*rad, y*rad, x*rad).
inline Matrix4f eulerTransformation(float xDeg, float yDeg, float zDeg) {
    Matrix4f result;
    result.rotationZYX(zDeg * DEG_TO_RAD, yDeg * DEG_TO_RAD, xDeg * DEG_TO_RAD);
    return result;
}

// CuboidRotation.scaleFactorForAxis — 1 / max(|transformDirection(axisUnit)|).
inline float scaleFactorForAxis(const Matrix4f& rotation, int axis) {
    Vector3f axisUnit = AXIS_POSITIVE_UNIT[axis];
    Vector3f transformed;
    rotation.transformDirection(axisUnit, transformed);
    float absX = std::fabs(transformed.x), absY = std::fabs(transformed.y), absZ = std::fabs(transformed.z);
    float maxComponent = std::max(std::max(absX, absY), absZ);
    return 1.0F / maxComponent;
}

inline Vector3f computeRescale(const Matrix4f& rotation) {
    return Vector3f{scaleFactorForAxis(rotation, 0), scaleFactorForAxis(rotation, 1), scaleFactorForAxis(rotation, 2)};
}

// CuboidRotation.computeTransform(value, rescale): apply rescale iff requested and
// the rotation isn't identity.
inline Matrix4f computeTransform(Matrix4f result, bool rescale) {
    if (rescale && !joml::matrixIsIdentity(result)) {
        Vector3f scale = computeRescale(result);
        result.scale(scale);
    }
    return result;
}

} // namespace mc::render::model::cuboid

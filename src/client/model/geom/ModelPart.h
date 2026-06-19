// 1:1 C++ port of the PURE transform math of net.minecraft.client.model.geom.ModelPart
// (Minecraft 26.1.2). This header is GL-free and self-contained: it reproduces the
// deterministic float/matrix math used to manipulate a model part's pose, with NO
// VertexConsumer / PoseStack / GPU dependency.
//
// Headline method: ModelPart.rotateBy(Quaternionf). The Java is:
//
//     public void rotateBy(Quaternionf rotation) {
//         Matrix3f oldRotation = new Matrix3f().rotationZYX(this.zRot, this.yRot, this.xRot);
//         Matrix3f newRotation = oldRotation.rotate(rotation);
//         Vector3f newAngles    = newRotation.getEulerAnglesZYX(new Vector3f());
//         this.setRotation(newAngles.x, newAngles.y, newAngles.z);
//     }
//
// It composes the part's current Z*Y*X euler rotation into a Matrix3f, post-rotates by
// a quaternion, then extracts euler angles back out — exercising three org.joml.Matrix3f
// methods. org.joml.Matrix3f.rotate(Quaternionfc) is already certified in
// render/model/Joml.h; the two euler-conversion methods are not, so they are ported here
// (rotationZYX, getEulerAnglesZYX), straight from the joml-1.10.8.jar bytecode.
//
// Also ported: the trivial pure pose mutators (storePose / loadPose / setPos /
// setRotation / offsetPos / offsetRotation / offsetScale) and PartPose, which are exact
// float copies/adds with no transcendental functions.
//
// JOML runs under its DEFAULT options here (joml.useMathFma=false, joml.fastmath=false),
// matching the certified Joml.h conventions: Math.fma == plain a*b+c (jfma), Math.sin ==
// (float)std::sin((double)x) (jsin), Math.cosFromSin == cosFromSin, Math.sqrt(float) ==
// (float)std::sqrt((double)x), Math.atan2(float,float) == (float)std::atan2((double)a,(double)b).

#pragma once

#include <cmath>

#include "render/model/Joml.h"

namespace mc::client::model::geom {

using ::mc::render::model::joml::Matrix3f;
using ::mc::render::model::joml::Quaternionf;
using ::mc::render::model::joml::Vector3f;
using ::mc::render::model::joml::cosFromSin;
using ::mc::render::model::joml::jsin;

// ── org.joml.Math.atan2(float,float) and Math.sqrt(float) ───────────────────────────
// Both narrow through double precision in the jar:
//   atan2(a,b) = (float) java.lang.Math.atan2((double)a, (double)b)
//   sqrt(a)    = (float) java.lang.Math.sqrt((double)a)
inline float jatan2(float a, float b) {
    return static_cast<float>(std::atan2(static_cast<double>(a), static_cast<double>(b)));
}
inline float jsqrtf(float a) {
    return static_cast<float>(std::sqrt(static_cast<double>(a)));
}

// ── org.joml.Matrix3f.rotationZYX(angleZ, angleY, angleX) ────────────────────────────
// Ported byte-for-byte from joml-1.10.8.jar (Matrix3f.rotationZYX). The local layout
// (sinX/cosX/.../nm20/nm21/nm22) and field writes follow the exact bytecode operand
// order. All products are plain float multiplies; the two-term fields are plain a+b.
// Returns dest (which is set in place — the JVM allocates `new Matrix3f()` first, but
// rotationZYX overwrites every one of the 9 elements, so the identity init is irrelevant).
inline Matrix3f& rotationZYX(Matrix3f& dest, float angleZ, float angleY, float angleX) {
    // Locals follow the joml-1.10.8 bytecode exactly: l4..l9 are the sin/cos pairs,
    // l10..l12 their negations, l13..l19 the staged products. (Param order is
    // angleZ, angleY, angleX; the first sin/cos pair is for angleX.)
    float sinX = jsin(angleX);
    float cosX = cosFromSin(sinX, angleX);  // l4, l5
    float sinY = jsin(angleY);
    float cosY = cosFromSin(sinY, angleY);  // l6, l7
    float sinZ = jsin(angleZ);
    float cosZ = cosFromSin(sinZ, angleZ);  // l8, l9
    float m_sinZ = -sinZ;                    // l10
    float m_sinY = -sinY;                    // l11
    float m_sinX = -sinX;                    // l12
    float nm00 = cosZ;                       // l13
    float nm01 = sinZ;                       // l14
    float nm02 = m_sinZ;                     // l15
    float nm10 = cosZ;                       // l16
    float nm20 = nm00 * sinY;                // l17 = cosZ*sinY
    float nm21 = nm01 * sinY;                // l18 = sinZ*sinY
    float nm22 = cosY;                       // l19

    dest.m00 = nm00 * cosY;
    dest.m01 = nm01 * cosY;
    dest.m02 = m_sinY;
    dest.m10 = nm02 * cosX + nm20 * sinX;
    dest.m11 = nm10 * cosX + nm21 * sinX;
    dest.m12 = nm22 * sinX;
    dest.m20 = nm02 * m_sinX + nm20 * cosX;
    dest.m21 = nm10 * m_sinX + nm21 * cosX;
    dest.m22 = nm22 * cosX;
    return dest;
}

// ── org.joml.Matrix3f.getEulerAnglesZYX(Vector3f dest) ───────────────────────────────
// Ported byte-for-byte from joml-1.10.8.jar (Matrix3f.getEulerAnglesZYX):
//   dest.x = atan2(m12, m22)
//   dest.y = atan2(-m02, sqrt(1.0f - m02*m02))
//   dest.z = atan2(m01, m00)
inline Vector3f& getEulerAnglesZYX(const Matrix3f& m, Vector3f& dest) {
    dest.x = jatan2(m.m12, m.m22);
    dest.y = jatan2(-m.m02, jsqrtf(1.0f - m.m02 * m.m02));
    dest.z = jatan2(m.m01, m.m00);
    return dest;
}

// ── net.minecraft.client.model.geom.PartPose ─────────────────────────────────────────
// record PartPose(float x,y,z, xRot,yRot,zRot, xScale,yScale,zScale)
struct PartPose {
    float x = 0, y = 0, z = 0;
    float xRot = 0, yRot = 0, zRot = 0;
    float xScale = 1, yScale = 1, zScale = 1;

    static PartPose offsetAndRotation(float ox, float oy, float oz, float rx, float ry, float rz) {
        return PartPose{ox, oy, oz, rx, ry, rz, 1.0f, 1.0f, 1.0f};
    }
    static PartPose offset(float x, float y, float z) {
        return offsetAndRotation(x, y, z, 0.0f, 0.0f, 0.0f);
    }
    static PartPose rotation(float x, float y, float z) {
        return offsetAndRotation(0.0f, 0.0f, 0.0f, x, y, z);
    }
    static PartPose ZERO() { return offsetAndRotation(0, 0, 0, 0, 0, 0); }

    PartPose translated(float dx, float dy, float dz) const {
        return PartPose{x + dx, y + dy, z + dz, xRot, yRot, zRot, xScale, yScale, zScale};
    }
    PartPose withScale(float scale) const {
        return PartPose{x, y, z, xRot, yRot, zRot, scale, scale, scale};
    }
    PartPose scaled(float sx, float sy, float sz) const {
        return PartPose{x * sx, y * sy, z * sz, xRot, yRot, zRot,
                        xScale * sx, yScale * sy, zScale * sz};
    }
    PartPose scaled(float factor) const {
        return factor == 1.0f ? *this : scaled(factor, factor, factor);
    }
};

// ── net.minecraft.client.model.geom.ModelPart (pure-math subset) ─────────────────────
// Only the GL-free transform/pose math is ported. cubes/children/render/compile, which
// need a VertexConsumer + GL, are deliberately omitted.
struct ModelPart {
    float x = 0, y = 0, z = 0;
    float xRot = 0, yRot = 0, zRot = 0;
    float xScale = 1.0f, yScale = 1.0f, zScale = 1.0f;

    void setPos(float nx, float ny, float nz) { x = nx; y = ny; z = nz; }
    void setRotation(float nxRot, float nyRot, float nzRot) { xRot = nxRot; yRot = nyRot; zRot = nzRot; }

    PartPose storePose() const {
        return PartPose::offsetAndRotation(x, y, z, xRot, yRot, zRot);
    }
    void loadPose(const PartPose& pose) {
        x = pose.x; y = pose.y; z = pose.z;
        xRot = pose.xRot; yRot = pose.yRot; zRot = pose.zRot;
        xScale = pose.xScale; yScale = pose.yScale; zScale = pose.zScale;
    }

    void offsetPos(const Vector3f& off) { x += off.x; y += off.y; z += off.z; }
    void offsetRotation(const Vector3f& off) { xRot += off.x; yRot += off.y; zRot += off.z; }
    void offsetScale(const Vector3f& off) { xScale += off.x; yScale += off.y; zScale += off.z; }

    // The headline: rotateBy(Quaternionf).
    void rotateBy(const Quaternionf& rotation) {
        Matrix3f oldRotation;
        rotationZYX(oldRotation, zRot, yRot, xRot);
        // Matrix3f.rotate mutates in place AND returns this — newRotation == oldRotation.
        Matrix3f& newRotation = oldRotation.rotate(rotation);
        Vector3f newAngles;
        getEulerAnglesZYX(newRotation, newAngles);
        setRotation(newAngles.x, newAngles.y, newAngles.z);
    }
};

}  // namespace mc::client::model::geom

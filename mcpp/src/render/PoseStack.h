// 1:1 port of com.mojang.blaze3d.vertex.PoseStack (Minecraft 26.1.2).
//
// A stack of Pose{Matrix4f pose, Matrix3f normal, bool trustedNormals}. All matrix
// math is delegated to the certified org.joml port in render/model/Joml.h (the same
// code the FaceBakery / Transformation gates verify); this header only adds the thin
// PoseStack/Pose glue and the two com.mojang.math.MatrixUtil predicates the normal
// path consults. NO value/formula/order deviates from the decompiled Java.
//
// Ported (verified by pose_stack_parity):
//   PoseStack: ctor, pushPose, popPose, last, isEmpty, setIdentity, translate,
//              scale, mulPose(Quaternionfc), mulPose(Matrix4fc), clear (test helper)
//   Pose:      set, copy, translate, scale (incl. negative-scale normal handling),
//              rotate(Quaternionfc) (pose.rotate + normal.rotate), mulPose(Matrix4fc)
//              (pose.mul + isPureTranslation / orthonormal-mul / computeNormalMatrix),
//              setIdentity, computeNormalMatrix
//   MatrixUtil: checkPropertyRaw, checkProperty, isPureTranslation
//
// UNPORTED (hard-trap / skipped, never faked): rotateAround (Quaternionfc),
//   mulPose(Transformation) — need an un-ported Transformation; Matrix4f.rotateGeneric
//   (a Pose matrix is always affine, so PoseStack never reaches it).
#pragma once

#include "model/Joml.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace mc::render {

namespace j = mc::render::model::joml;

// ── com.mojang.math.MatrixUtil (the two predicates PoseStack.Pose.mulPose uses) ──
// checkPropertyRaw(matrix, property): (matrix.properties() & property) != 0
inline bool checkPropertyRaw(const j::Matrix4f& m, int property) {
    return (m.properties & property) != 0;
}

// checkProperty(Matrix4fc, property): raw check, else determineProperties() then
// assume(properties | current) and re-check. (matrix is always a mutable Matrix4f here)
inline bool matrixUtilCheckProperty(j::Matrix4f& m, int property) {
    if (checkPropertyRaw(m, property)) return true;
    int currentProperties = m.properties;
    m.determineProperties();
    m.properties |= currentProperties; // assume(properties | current)
    return checkPropertyRaw(m, property);
}

inline bool isPureTranslation(j::Matrix4f& m) {
    return matrixUtilCheckProperty(m, j::Matrix4f::PROPERTY_TRANSLATION); // 8
}

// java.lang.Math.signum(float): 0.0f for +/-0, NaN for NaN, else copysign(1,x).
inline float javaSignum(float x) {
    return (x == 0.0f || std::isnan(x)) ? x : std::copysignf(1.0f, x);
}

// ── PoseStack.Pose ───────────────────────────────────────────────────────────
struct Pose {
    j::Matrix4f pose;          // identity
    j::Matrix3f normal;        // identity
    bool trustedNormals = true;

    void computeNormalMatrix() {
        normal.set(pose).invert().transpose();
        trustedNormals = false;
    }

    void set(const Pose& o) {
        pose = o.pose;            // Matrix4f.set copies the 16 elements + properties
        normal.set(o.normal);
        trustedNormals = o.trustedNormals;
    }

    // Matrix4f translate(float,float,float) -> pose.translate
    void translate(float xo, float yo, float zo) {
        pose.translate(xo, yo, zo);
    }

    void scale(float xScale, float yScale, float zScale) {
        pose.scale(xScale, yScale, zScale);
        if (std::fabs(xScale) == std::fabs(yScale) && std::fabs(yScale) == std::fabs(zScale)) {
            if (xScale < 0.0f || yScale < 0.0f || zScale < 0.0f) {
                normal.scale(javaSignum(xScale), javaSignum(yScale), javaSignum(zScale));
            }
        } else {
            normal.scale(1.0f / xScale, 1.0f / yScale, 1.0f / zScale);
            trustedNormals = false;
        }
    }

    void rotate(const j::Quaternionf& by) {
        pose.rotate(by);
        normal.rotate(by);
    }

    void setIdentity() {
        pose.setIdentityValues();
        pose.properties =
            j::Matrix4f::PROPERTY_IDENTITY | j::Matrix4f::PROPERTY_AFFINE
            | j::Matrix4f::PROPERTY_TRANSLATION | j::Matrix4f::PROPERTY_ORTHONORMAL;
        normal.identity();
        trustedNormals = true;
    }

    void mulPose(const j::Matrix4f& matrix) {
        pose.mul(matrix);
        // MatrixUtil.isPureTranslation / checkPropertyRaw inspect the *argument*'s
        // properties; the argument is logically const but determineProperties() may
        // refine its property bits (a pure lazy-cache update, no value change).
        j::Matrix4f& arg = const_cast<j::Matrix4f&>(matrix);
        if (!isPureTranslation(arg)) {
            if (checkPropertyRaw(arg, j::Matrix4f::PROPERTY_ORTHONORMAL)) { // 16
                j::Matrix3f m3;
                m3.set(arg);
                normal.mul(m3);
            } else {
                computeNormalMatrix();
            }
        }
    }
};

// ── PoseStack ────────────────────────────────────────────────────────────────
struct PoseStack {
    std::vector<Pose> poses;
    int lastIndex = 0;

    PoseStack() { poses.emplace_back(); }

    Pose& last() { return poses[lastIndex]; }

    bool isEmpty() const { return lastIndex == 0; }

    void translate(float xo, float yo, float zo) { last().translate(xo, yo, zo); }

    void scale(float x, float y, float z) { last().scale(x, y, z); }

    void mulPose(const j::Quaternionf& by) { last().rotate(by); }

    void mulPose(const j::Matrix4f& matrix) { last().mulPose(matrix); }

    void setIdentity() { last().setIdentity(); }

    void pushPose() {
        Pose lastPose = last();      // value copy of the current top
        lastIndex++;
        if (lastIndex >= (int)poses.size()) {
            poses.push_back(lastPose); // copy() then push
        } else {
            poses[lastIndex].set(lastPose);
        }
    }

    void popPose() {
        // Java throws NoSuchElementException when lastIndex==0; the test never pops
        // an empty stack, so mirror the guard without an exception type.
        if (lastIndex == 0) return;
        lastIndex--;
    }
};

} // namespace mc::render

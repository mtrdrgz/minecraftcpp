// 1:1 port of the PURE static helpers of com.mojang.math.MatrixUtil (Minecraft
// 26.1.2) plus the small com.mojang.math.GivensParameters record they depend on.
// Source: 26.1.2/src/com/mojang/math/MatrixUtil.java and GivensParameters.java.
//
// What is ported here (deterministic / non-iterative):
//   - mulComponentWise(Matrix4f&, float)  — scales all 16 elements by factor then
//     re-runs Matrix4f.set(16f) which ends in determineProperties() (verified from
//     the joml-1.10.8.jar bytecode: set(float..16) sets every _mRC then calls
//     determineProperties()). So the resulting `properties` bitset is recomputed.
//   - isPureTranslation(const Matrix4f&) = checkProperty(matrix, 8) — reuses the
//     existing checkProperty() in Joml.h (no duplication).
//   - isIdentity(const Matrix4f&)        = checkProperty(matrix, 4) — alias of the
//     existing matrixIsIdentity() in Joml.h.
//   - GivensParameters (record): fromUnnormalized, fromPositiveAngle, inverse,
//     cos(), sin(), aroundX/Y/Z(Quaternionf) — the cheap 2x2 helpers.
//   - approxGivensQuat(a11,a12,a22), qrGivensQuat(a1,a2): the static givens helpers.
//
// NOT ported (iterative Jacobi SVD — would risk non-bit-exact accumulation and is
// out of scope): svdDecompose, eigenvalueJacobi, stepJacobi, similarityTransform.
// Also not duplicated: the Matrix3f aroundX/Y/Z and rotate(Quaternionf) overloads
// that only svdDecompose uses.
//
// org.joml.Math primitives used (all confirmed from joml-1.10.8.jar bytecode):
//   Math.invsqrt(float) = 1.0f / (float)java.lang.Math.sqrt((double)x)  -> jinvsqrt
//   Math.sqrt(float)    = (float)java.lang.Math.sqrt((double)x)          -> jsqrt
//   Math.sin/cosFromSin -> jsin / cosFromSin (already in Joml.h)
//   Math.abs(float)     = java.lang.Math.abs(float)
//   Math.max(float,a,b) = (a > b) ? a : b   (fcmpl semantics)
// qrGivensQuat additionally uses java.lang.Math.hypot (libm) — see note in the gate.
//
// NO deviation from the source/bytecode is permitted in this file.
#pragma once

#include "Joml.h"

#include <cmath>

namespace mc::render::model::matrixutil {

namespace j = mc::render::model::joml;

// ── com.mojang.math.GivensParameters (record(float sinHalf, float cosHalf)) ──────
struct GivensParameters {
    float sinHalf;
    float cosHalf;

    // fromUnnormalized: w = Math.invsqrt(sinHalf*sinHalf + cosHalf*cosHalf);
    //                   return new GivensParameters(w*sinHalf, w*cosHalf);
    static GivensParameters fromUnnormalized(float sinHalf, float cosHalf) {
        float w = j::jinvsqrt(sinHalf * sinHalf + cosHalf * cosHalf);
        return GivensParameters{w * sinHalf, w * cosHalf};
    }

    // fromPositiveAngle: sin = Math.sin(angle/2); cos = Math.cosFromSin(sin, angle/2);
    static GivensParameters fromPositiveAngle(float angle) {
        float sin = j::jsin(angle / 2.0f);
        float cos = j::cosFromSin(sin, angle / 2.0f);
        return GivensParameters{sin, cos};
    }

    GivensParameters inverse() const { return GivensParameters{-sinHalf, cosHalf}; }

    // cos() = cosHalf*cosHalf - sinHalf*sinHalf
    float cos() const { return cosHalf * cosHalf - sinHalf * sinHalf; }
    // sin() = 2.0f * sinHalf * cosHalf
    float sin() const { return 2.0f * sinHalf * cosHalf; }

    // aroundX/Y/Z(Quaternionf): input.set(...)
    j::Quaternionf& aroundX(j::Quaternionf& input) const { return input.set(sinHalf, 0.0f, 0.0f, cosHalf); }
    j::Quaternionf& aroundY(j::Quaternionf& input) const { return input.set(0.0f, sinHalf, 0.0f, cosHalf); }
    j::Quaternionf& aroundZ(j::Quaternionf& input) const { return input.set(0.0f, 0.0f, sinHalf, cosHalf); }
};

// ── MatrixUtil static fields/helpers ─────────────────────────────────────────────
// private static final float G = 3.0F + 2.0F * Math.sqrt(2.0F);
inline float matrixUtilG() {
    return 3.0f + 2.0f * j::jsqrt(2.0f);
}

// private static final GivensParameters PI_4 = fromPositiveAngle((float)(Math.PI/4))
inline GivensParameters matrixUtilPi4() {
    // java.lang.Math.PI is the IEEE-754 double 3.141592653589793 (0x400921FB54442D18).
    constexpr double kPi = 3.141592653589793;
    return GivensParameters::fromPositiveAngle((float)(kPi / 4.0));
}

// private static GivensParameters approxGivensQuat(float a11, float a12, float a22)
//   float ch = 2.0F * (a11 - a22);
//   float sh = a12;
//   return G*sh*sh < ch*ch ? fromUnnormalized(sh, ch) : PI_4;
inline GivensParameters approxGivensQuat(float a11, float a12, float a22) {
    float ch = 2.0f * (a11 - a22);
    float sh = a12;
    float G = matrixUtilG();
    return (G * sh * sh < ch * ch) ? GivensParameters::fromUnnormalized(sh, ch) : matrixUtilPi4();
}

// private static GivensParameters qrGivensQuat(float a1, float a2)
//   float p  = (float)java.lang.Math.hypot(a1, a2);
//   float sh = p > 1.0E-6F ? a2 : 0.0F;
//   float ch = Math.abs(a1) + Math.max(p, 1.0E-6F);
//   if (a1 < 0.0F) { swap(sh, ch); }
//   return fromUnnormalized(sh, ch);
inline GivensParameters qrGivensQuat(float a1, float a2) {
    float p = (float)std::hypot((double)a1, (double)a2); // java.lang.Math.hypot
    float sh = p > 1.0e-6f ? a2 : 0.0f;
    // org.joml.Math.max(p, 1e-6f) = (p > 1e-6f) ? p : 1e-6f   (fcmpl)
    float mx = (p > 1.0e-6f) ? p : 1.0e-6f;
    float ch = std::fabs(a1) + mx; // java.lang.Math.abs(float)
    if (a1 < 0.0f) {
        float f = sh;
        sh = ch;
        ch = f;
    }
    return GivensParameters::fromUnnormalized(sh, ch);
}

// public static Matrix4f mulComponentWise(Matrix4f m, float factor)
//   return m.set(m00*f, m01*f, ... m33*f);   // set(16f) ends in determineProperties()
inline j::Matrix4f& mulComponentWise(j::Matrix4f& m, float factor) {
    // JOML set(float..16) takes column-major args p0=m00,p1=m01,p2=m02,p3=m03,p4=m10,...
    // and MatrixUtil passes m.m00()*f, m.m01()*f, ... so each element is scaled in place.
    float n00 = m.m00 * factor, n01 = m.m01 * factor, n02 = m.m02 * factor, n03 = m.m03 * factor;
    float n10 = m.m10 * factor, n11 = m.m11 * factor, n12 = m.m12 * factor, n13 = m.m13 * factor;
    float n20 = m.m20 * factor, n21 = m.m21 * factor, n22 = m.m22 * factor, n23 = m.m23 * factor;
    float n30 = m.m30 * factor, n31 = m.m31 * factor, n32 = m.m32 * factor, n33 = m.m33 * factor;
    m.m00 = n00; m.m01 = n01; m.m02 = n02; m.m03 = n03;
    m.m10 = n10; m.m11 = n11; m.m12 = n12; m.m13 = n13;
    m.m20 = n20; m.m21 = n21; m.m22 = n22; m.m23 = n23;
    m.m30 = n30; m.m31 = n31; m.m32 = n32; m.m33 = n33;
    m.determineProperties(); // set(float..16) calls determineProperties()
    return m;
}

// public static boolean isPureTranslation(Matrix4fc matrix) { return checkProperty(matrix, 8); }
inline bool isPureTranslation(const j::Matrix4f& m) {
    return j::checkProperty(m, j::Matrix4f::PROPERTY_TRANSLATION); // 8
}

// public static boolean isIdentity(Matrix4fc matrix) { return checkProperty(matrix, 4); }
inline bool isIdentity(const j::Matrix4f& m) {
    return j::checkProperty(m, j::Matrix4f::PROPERTY_IDENTITY); // 4
}

// public static boolean checkProperty(Matrix4fc, int) — re-exported (delegates to Joml.h)
inline bool checkProperty(const j::Matrix4f& m, int property) {
    return j::checkProperty(m, property);
}

// public static boolean checkPropertyRaw(Matrix4fc matrix, int property)
//   return (matrix.properties() & property) != 0;
inline bool checkPropertyRaw(const j::Matrix4f& m, int property) {
    return (m.properties & property) != 0;
}

} // namespace mc::render::model::matrixutil

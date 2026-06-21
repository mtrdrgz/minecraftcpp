// 1:1 port of the org.joml.Matrix4f projection builders that Minecraft 26.1.2
// uses to build its camera/HUD projection matrices:
//
//   - Matrix4f.setOrtho(left,right,bottom,top,zNear,zFar,zZeroToOne)
//       net.minecraft.client.renderer.Projection.getMatrix (orthographic branch)
//   - Matrix4f.setPerspective(fovy,aspect,zNear,zFar,zZeroToOne)
//       Projection.getMatrix (perspective branch)
//   - Matrix4f.perspective(fovy,aspect,zNear,zFar,zZeroToOne)
//       net.minecraft.client.Camera.createProjectionMatrixForCulling
//
// Ported VERBATIM from the bytecode of the shipped joml-1.10.8.jar
// (org.joml.Matrix4f, disassembled with javap -c -p). Column-major storage
// (mCR = column C, row R), matching JOML and the existing render/model/Joml.h.
//
// Bit-exactness notes:
//  - org.joml.Math.tan(float x) = (float) java.lang.Math.tan((double) x)
//    (org.joml.Math.tan bytecode: f2d; java.lang.Math.tan; d2f). java.lang.Math.tan
//    is a libm/StrictMath-class transcendental; std::tan matches it on this platform
//    in practice but is permitted to differ by 1 ULP on others. The parity gate
//    against the real org.joml asserts bit-equality on the GT inputs here.
//  - setOrtho seeds identity then overwrites m00/m11/m22/m30/m31/m32 (every other
//    element stays at the identity value). setPerspective zeroes the whole matrix
//    first, so only the written elements are non-zero.
//  - All arithmetic is plain float * / + - (no FMA on these paths in the bytecode).
//
// NO deviation from the bytecode is permitted in this file. Self-contained: this
// header intentionally does NOT depend on render/model/Joml.h.
#pragma once

#include <cmath>
#include <cstdint>

namespace mc::render::model::matrix4f_proj {

// JOML property bit flags (org.joml.Matrix4f).
inline constexpr int PROPERTY_PERSPECTIVE  = 1;
inline constexpr int PROPERTY_AFFINE       = 2;
inline constexpr int PROPERTY_IDENTITY     = 4;
inline constexpr int PROPERTY_TRANSLATION  = 8;
inline constexpr int PROPERTY_ORTHONORMAL  = 16;

// org.joml.Math.tan(float) = (float) java.lang.Math.tan((double) x)
inline float jtan(float x) { return (float)std::tan((double)x); }

// Float.isInfinite(float)
inline bool jIsInfinite(float x) { return std::isinf(x); }

// A minimal column-major 4x4 float matrix holding exactly JOML's 16 fields plus
// the properties bit set. The constructor leaves it in the JOML identity state.
struct Matrix4f {
    // column-major: mCR = column C, row R
    float m00 = 1, m01 = 0, m02 = 0, m03 = 0;
    float m10 = 0, m11 = 1, m12 = 0, m13 = 0;
    float m20 = 0, m21 = 0, m22 = 1, m23 = 0;
    float m30 = 0, m31 = 0, m32 = 0, m33 = 1;
    int properties = PROPERTY_AFFINE | PROPERTY_IDENTITY | PROPERTY_TRANSLATION | PROPERTY_ORTHONORMAL;

    // MemUtil.INSTANCE.identity(this): reset all 16 fields to the identity matrix.
    void identityValues() {
        m00 = 1; m01 = 0; m02 = 0; m03 = 0;
        m10 = 0; m11 = 1; m12 = 0; m13 = 0;
        m20 = 0; m21 = 0; m22 = 1; m23 = 0;
        m30 = 0; m31 = 0; m32 = 0; m33 = 1;
    }

    // MemUtil.INSTANCE.zero(this): zero all 16 fields.
    void zeroValues() {
        m00 = 0; m01 = 0; m02 = 0; m03 = 0;
        m10 = 0; m11 = 0; m12 = 0; m13 = 0;
        m20 = 0; m21 = 0; m22 = 0; m23 = 0;
        m30 = 0; m31 = 0; m32 = 0; m33 = 0;
    }

    // org.joml.Matrix4f.setOrtho(left,right,bottom,top,zNear,zFar,zZeroToOne)
    Matrix4f& setOrtho(float left, float right, float bottom, float top,
                       float zNear, float zFar, bool zZeroToOne) {
        if ((properties & PROPERTY_IDENTITY) == 0) {
            identityValues();
        }
        m00 = 2.0f / (right - left);
        m11 = 2.0f / (top - bottom);
        m22 = (zZeroToOne ? 1.0f : 2.0f) / (zNear - zFar);
        m30 = (right + left) / (left - right);
        m31 = (top + bottom) / (bottom - top);
        m32 = (zZeroToOne ? zNear : (zFar + zNear)) / (zNear - zFar);
        properties = PROPERTY_AFFINE;
        return *this;
    }

    // org.joml.Matrix4f.setPerspective(fovy,aspect,zNear,zFar,zZeroToOne)
    Matrix4f& setPerspective(float fovy, float aspect, float zNear, float zFar,
                             bool zZeroToOne) {
        zeroValues();
        float h = jtan(fovy * 0.5f);
        m00 = 1.0f / (h * aspect);
        m11 = 1.0f / h;
        bool farInf  = (zFar  > 0.0f) && jIsInfinite(zFar);
        bool nearInf = (zNear > 0.0f) && jIsInfinite(zNear);
        if (farInf) {
            float e = 1.0E-6f;
            m22 = e - 1.0f;
            m32 = (e - (zZeroToOne ? 1.0f : 2.0f)) * zNear;
        } else if (nearInf) {
            float e = 1.0E-6f;
            m22 = (zZeroToOne ? 0.0f : 1.0f) - e;
            m32 = ((zZeroToOne ? 1.0f : 2.0f) - e) * zFar;
        } else {
            m22 = (zZeroToOne ? zFar : (zFar + zNear)) / (zNear - zFar);
            m32 = (zZeroToOne ? zFar : (zFar + zFar)) * zNear / (zNear - zFar);
        }
        m23 = -1.0f;
        properties = PROPERTY_PERSPECTIVE;
        return *this;
    }

    // org.joml.Matrix4f.perspective(fovy,aspect,zNear,zFar,zZeroToOne): on an
    // identity matrix this dispatches straight to setPerspective (the only path
    // Camera.createProjectionMatrixForCulling exercises — it starts from a fresh
    // new Matrix4f()). The non-identity perspectiveGeneric path is NOT used by
    // Minecraft's projection code and is intentionally not ported here.
    Matrix4f& perspective(float fovy, float aspect, float zNear, float zFar,
                          bool zZeroToOne) {
        if ((properties & PROPERTY_IDENTITY) != 0) {
            return setPerspective(fovy, aspect, zNear, zFar, zZeroToOne);
        }
        // perspectiveGeneric: unported (not reached by the Minecraft pipeline).
#if defined(_MSC_VER)
        __assume(0);
#else
        __builtin_trap();
#endif
    }
};

} // namespace mc::render::model::matrix4f_proj

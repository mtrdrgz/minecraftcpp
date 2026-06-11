#pragma once

// 1:1 port of the PURE orthographic-projection setters of org.joml.Matrix4f
// (JOML 1.10.8 — the exact jar Minecraft 26.1.2 ships:
//  26.1.2/libs/joml-1.10.8.jar / server_run/.../joml-1.10.8.jar).
//
// These are the `set*` (not the `*` multiply) variants: each one resets the
// receiver to identity (if it is not already identity) and then overwrites the
// projection elements, leaving every other element at its identity value. They
// are reached in-engine from net.minecraft.client.renderer.Projection.setupOrtho
// -> Projection.getMatrix -> Matrix4f.setOrtho(left, right, bottom, top, zNear,
// zFar, RenderSystem.getDevice().isZZeroToOne()) for the GUI / orthographic pass.
//
// Ported methods (signatures FFFFFFZ / FFFFZ, the boolean is JOML's
// `zZeroToOne` clip-space flag):
//   setOrtho(l, r, b, t, zn, zf, zZeroToOne)
//   setOrthoLH(l, r, b, t, zn, zf, zZeroToOne)
//   setOrthoSymmetric(width, height, zn, zf, zZeroToOne)
//   setOrthoSymmetricLH(width, height, zn, zf, zZeroToOne)
//
// Disassembled from the real bytecode with javap (no transcendentals — every op
// is a plain 32-bit float add/sub/mul/div), so a faithful float replication is
// bit-exact. Verified by matrix4f_ortho_parity (ground truth:
// tools/Matrix4fOrthoParity.java vs the REAL org.joml.Matrix4f).
//
// Exact element assignments (column-major mCR, all on an identity matrix):
//   setOrtho:
//     _m00(2 / (r - l)); _m11(2 / (t - b));
//     _m22((zZeroToOne ? 1 : 2) / (zn - zf));
//     _m30((r + l) / (l - r)); _m31((t + b) / (b - t));
//     _m32((zZeroToOne ? zn : (zf + zn)) / (zn - zf));
//     properties = AFFINE (2)
//   setOrthoLH:           identical EXCEPT
//     _m22((zZeroToOne ? 1 : 2) / (zf - zn));
//     _m32((zZeroToOne ? zn : (zf + zn)) / (zn - zf));
//   setOrthoSymmetric(width, height, zn, zf, zZeroToOne):
//     _m00(2 / width); _m11(2 / height);
//     _m22((zZeroToOne ? 1 : 2) / (zn - zf));
//     _m32((zZeroToOne ? zn : (zf + zn)) / (zn - zf));
//     properties = AFFINE (2)
//   setOrthoSymmetricLH:  identical EXCEPT
//     _m22((zZeroToOne ? 1 : 2) / (zf - zn));
//
// The `set*` family resets to identity only when the matrix is not already
// identity ( (properties & PROPERTY_IDENTITY) != 0 -> skip ), matching the
// `iconst_4; iand; ifne` guard in the bytecode. We model that with
// Matrix4f::setIdentityValues() (from the certified Joml.h) so the off-diagonal
// terms left untouched keep their identity values (m33 == 1, the rest 0).
//
// SKIPPED here (separately certified, transcendental, or non-pure): the
// perspective family setPerspective*/perspective* (uses org.joml.Math.tan, a
// HotSpot-intrinsic-bearing call -> tracked under the intrinsic-gap ceiling, not
// claimed bit-exact here), the multiply-in-place ortho*/orthoSymmetric*
// (post-multiply variants), and all GL-state methods.

#include "Joml.h"

namespace mc::render::matrix4f_ortho {

namespace jm = mc::render::model::joml;

using Matrix4f = jm::Matrix4f;

// Matrix4f.setOrtho(float, float, float, float, float, float, boolean) — FFFFFFZ
inline Matrix4f& setOrtho(Matrix4f& dst, float left, float right, float bottom, float top,
                          float zNear, float zFar, bool zZeroToOne) {
    if ((dst.properties & Matrix4f::PROPERTY_IDENTITY) == 0) {
        dst.setIdentityValues();
    }
    dst.m00 = 2.0f / (right - left);
    dst.m11 = 2.0f / (top - bottom);
    dst.m22 = (zZeroToOne ? 1.0f : 2.0f) / (zNear - zFar);
    dst.m30 = (right + left) / (left - right);
    dst.m31 = (top + bottom) / (bottom - top);
    dst.m32 = (zZeroToOne ? zNear : (zFar + zNear)) / (zNear - zFar);
    dst.properties = Matrix4f::PROPERTY_AFFINE;
    return dst;
}

// Matrix4f.setOrthoLH(float, float, float, float, float, float, boolean) — FFFFFFZ
inline Matrix4f& setOrthoLH(Matrix4f& dst, float left, float right, float bottom, float top,
                            float zNear, float zFar, bool zZeroToOne) {
    if ((dst.properties & Matrix4f::PROPERTY_IDENTITY) == 0) {
        dst.setIdentityValues();
    }
    dst.m00 = 2.0f / (right - left);
    dst.m11 = 2.0f / (top - bottom);
    dst.m22 = (zZeroToOne ? 1.0f : 2.0f) / (zFar - zNear);
    dst.m30 = (right + left) / (left - right);
    dst.m31 = (top + bottom) / (bottom - top);
    dst.m32 = (zZeroToOne ? zNear : (zFar + zNear)) / (zNear - zFar);
    dst.properties = Matrix4f::PROPERTY_AFFINE;
    return dst;
}

// Matrix4f.setOrthoSymmetric(float, float, float, float, boolean) — FFFFZ
inline Matrix4f& setOrthoSymmetric(Matrix4f& dst, float width, float height,
                                   float zNear, float zFar, bool zZeroToOne) {
    if ((dst.properties & Matrix4f::PROPERTY_IDENTITY) == 0) {
        dst.setIdentityValues();
    }
    dst.m00 = 2.0f / width;
    dst.m11 = 2.0f / height;
    dst.m22 = (zZeroToOne ? 1.0f : 2.0f) / (zNear - zFar);
    dst.m32 = (zZeroToOne ? zNear : (zFar + zNear)) / (zNear - zFar);
    dst.properties = Matrix4f::PROPERTY_AFFINE;
    return dst;
}

// Matrix4f.setOrthoSymmetricLH(float, float, float, float, boolean) — FFFFZ
inline Matrix4f& setOrthoSymmetricLH(Matrix4f& dst, float width, float height,
                                     float zNear, float zFar, bool zZeroToOne) {
    if ((dst.properties & Matrix4f::PROPERTY_IDENTITY) == 0) {
        dst.setIdentityValues();
    }
    dst.m00 = 2.0f / width;
    dst.m11 = 2.0f / height;
    dst.m22 = (zZeroToOne ? 1.0f : 2.0f) / (zFar - zNear);
    dst.m32 = (zZeroToOne ? zNear : (zFar + zNear)) / (zNear - zFar);
    dst.properties = Matrix4f::PROPERTY_AFFINE;
    return dst;
}

}  // namespace mc::render::matrix4f_ortho

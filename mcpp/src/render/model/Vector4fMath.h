#pragma once

// 1:1 port of the PURE (transcendental-free) arithmetic of org.joml.Vector4f
// (JOML 1.10.8 — the exact jar Minecraft 26.1.2 ships:
//  26.1.2/libs/joml-1.10.8.jar / server_run/.../joml-1.10.8.jar).
//
// Vector4f is the homogeneous (x,y,z,w) vector JOML uses for matrix transforms.
// In-engine it is the type fed through net.minecraft.client.renderer.DynamicUniforms
// .writeTransform(..., new Vector4f(1,1,1,1), ...) and the projection / frustum
// transform paths (Vector4f.mul(Matrix4fc) carries a clip-space point through a
// projection matrix). Only the methods that are pure 32-bit float add/sub/mul/div
// (plus org.joml.Math.fma — see below) are ported here; the
// libm-bearing members (length()/normalize()/distance() use a sqrt, *Transpose uses
// nothing extra but is included, angle/normalize3 use transcendentals) are listed
// as SKIPPED so the gate claims only what is provably bit-exact.
//
// Disassembled from the real bytecode with javap (Vector4f.class from the jar above)
// so every operation and its evaluation order is taken from the source, never guessed.
// Verified by vector4f_math_parity (ground truth: tools/Vector4fMathParity.java vs the
// REAL org.joml.Vector4f / org.joml.Matrix4f).
//
// ── THE org.joml.Math.fma TRAP ───────────────────────────────────────────────
// Every JOML "fma" in these bodies is org.joml.Math.fma(float,float,float), which by
// DEFAULT (org.joml.Options.USE_MATH_FMA == false) is a PLAIN two-rounding multiply-add
// `a*b + c`, NOT java.lang.Math.fma (a single-rounding FMA). The certified Joml.h
// already exposes this as `jm::jfma(a,b,c) { return a*b + c; }`; we reuse it. The whole
// translation unit MUST be compiled with -ffp-contract=off so the C++ compiler cannot
// fuse `a*b + c` into a hardware FMA behind our back (that would be a 1-ULP bug).
//
// ── Matrix4f property dispatch (Vector4f.mul(Matrix4fc, Vector4f)) ────────────
// The receiver matrix's `properties` bit set selects the kernel (javap dispatch):
//   (props & PROPERTY_IDENTITY    /*4*/) != 0 -> dest.set(this)        (no transform)
//   (props & PROPERTY_TRANSLATION /*8*/) != 0 -> mulTranslation
//   (props & PROPERTY_AFFINE      /*2*/) != 0 -> mulAffine
//   else                                       -> mulGeneric
// These bit values match Joml.h's Matrix4f::PROPERTY_* constants exactly.
//
// SKIPPED (separately certified, transcendental, or non-pure): length()/normalize()/
// distance()/normalize3() (sqrt/invsqrt), angle()/angleCos() (acos), min/max (trivial
// but rely on Math.min/max), the *Transpose variants (use no extra math but are not on
// any audited engine path), the NIO buffer get/set overloads, and toString().

#include "Joml.h"

namespace mc::render::vector4f_math {

namespace jm = mc::render::model::joml;

using Matrix4f = jm::Matrix4f;

// org.joml.Vector4f — fields in JOML declaration order are (x, y, z, w); the accessors
// x()/y()/z()/w() return them directly.
struct Vector4f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    Vector4f() = default;
    Vector4f(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    // ── set(Vector4fc) ───────────────────────────────────────────────────────
    // Guards self-assignment (if_acmpne), then copies each component.
    Vector4f& set(const Vector4f& v) {
        if (&v == this) {
            return *this;
        }
        x = v.x;
        y = v.y;
        z = v.z;
        w = v.w;
        return *this;
    }

    // ── add / sub / mul / div (component-wise, this OP arg) ───────────────────
    // add(Vector4fc, Vector4f dest): dest.c = this.c + v.c
    Vector4f& add(const Vector4f& v, Vector4f& dest) const {
        dest.x = x + v.x;
        dest.y = y + v.y;
        dest.z = z + v.z;
        dest.w = w + v.w;
        return dest;
    }

    // sub(Vector4fc, Vector4f dest): dest.c = this.c - v.c
    Vector4f& sub(const Vector4f& v, Vector4f& dest) const {
        dest.x = x - v.x;
        dest.y = y - v.y;
        dest.z = z - v.z;
        dest.w = w - v.w;
        return dest;
    }

    // mul(Vector4fc, Vector4f dest): dest.c = this.c * v.c
    Vector4f& mul(const Vector4f& v, Vector4f& dest) const {
        dest.x = x * v.x;
        dest.y = y * v.y;
        dest.z = z * v.z;
        dest.w = w * v.w;
        return dest;
    }

    // div(Vector4fc, Vector4f dest): dest.c = this.c / v.c
    Vector4f& div(const Vector4f& v, Vector4f& dest) const {
        dest.x = x / v.x;
        dest.y = y / v.y;
        dest.z = z / v.z;
        dest.w = w / v.w;
        return dest;
    }

    // ── negate(Vector4f dest): dest.c = -this.c (fneg) ────────────────────────
    Vector4f& negate(Vector4f& dest) const {
        dest.x = -x;
        dest.y = -y;
        dest.z = -z;
        dest.w = -w;
        return dest;
    }

    // ── fma(Vector4fc a, Vector4fc b, Vector4f dest): dest.c = fma(a.c, b.c, this.c)
    // All FOUR components (x,y,z,w).
    Vector4f& fma(const Vector4f& a, const Vector4f& b, Vector4f& dest) const {
        dest.x = jm::jfma(a.x, b.x, x);
        dest.y = jm::jfma(a.y, b.y, y);
        dest.z = jm::jfma(a.z, b.z, z);
        dest.w = jm::jfma(a.w, b.w, w);
        return dest;
    }

    // fma(float a, Vector4fc b, Vector4f dest): dest.c = fma(a, b.c, this.c) — all 4.
    Vector4f& fma(float a, const Vector4f& b, Vector4f& dest) const {
        dest.x = jm::jfma(a, b.x, x);
        dest.y = jm::jfma(a, b.y, y);
        dest.z = jm::jfma(a, b.z, z);
        dest.w = jm::jfma(a, b.w, w);
        return dest;
    }

    // ── lerp(Vector4fc other, float t, Vector4f dest) ─────────────────────────
    // dest.c = fma(other.c - this.c, t, this.c)   — all 4 components.
    Vector4f& lerp(const Vector4f& other, float t, Vector4f& dest) const {
        dest.x = jm::jfma(other.x - x, t, x);
        dest.y = jm::jfma(other.y - y, t, y);
        dest.z = jm::jfma(other.z - z, t, z);
        dest.w = jm::jfma(other.w - w, t, w);
        return dest;
    }

    // ── dot(Vector4fc): x*ox + (y*oy + (z*oz + (w*ow))) ───────────────────────
    // javap order: fmul on the w term, then three fma folds (right-associated).
    float dot(const Vector4f& v) const {
        return jm::jfma(x, v.x, jm::jfma(y, v.y, jm::jfma(z, v.z, w * v.w)));
    }

    // ── lengthSquared(): x*x + (y*y + (z*z + (w*w))) ──────────────────────────
    float lengthSquared() const {
        return jm::jfma(x, x, jm::jfma(y, y, jm::jfma(z, z, w * w)));
    }

    // ── distanceSquared(Vector4fc) -> distanceSquared(ox,oy,oz,ow) ────────────
    // dc = this.c - o.c; then dx*dx + (dy*dy + (dz*dz + (dw*dw))).
    float distanceSquared(const Vector4f& v) const {
        float dx = x - v.x;
        float dy = y - v.y;
        float dz = z - v.z;
        float dw = w - v.w;
        return jm::jfma(dx, dx, jm::jfma(dy, dy, jm::jfma(dz, dz, dw * dw)));
    }

    // ── matrix-transform kernels (mul(Matrix4fc, Vector4f dest)) ──────────────
    // mulGeneric: full 4x4. Per output component, javap pushes the m30/m3c*w term
    // first (fmul) then folds with three fma's (right-associated):
    //   x' = fma(m00,x, fma(m10,y, fma(m20,z, m30*w)))   etc.
    Vector4f& mulGeneric(const Matrix4f& m, Vector4f& dest) const {
        float vx = x;
        float vy = y;
        float vz = z;
        float vw = w;
        float rx = jm::jfma(m.m00, vx, jm::jfma(m.m10, vy, jm::jfma(m.m20, vz, m.m30 * vw)));
        float ry = jm::jfma(m.m01, vx, jm::jfma(m.m11, vy, jm::jfma(m.m21, vz, m.m31 * vw)));
        float rz = jm::jfma(m.m02, vx, jm::jfma(m.m12, vy, jm::jfma(m.m22, vz, m.m32 * vw)));
        float rw = jm::jfma(m.m03, vx, jm::jfma(m.m13, vy, jm::jfma(m.m23, vz, m.m33 * vw)));
        dest.x = rx;
        dest.y = ry;
        dest.z = rz;
        dest.w = rw;
        return dest;
    }

    // mulAffine: identical to mulGeneric for x,y,z; w' = the ORIGINAL w (stored direct).
    Vector4f& mulAffine(const Matrix4f& m, Vector4f& dest) const {
        float vx = x;
        float vy = y;
        float vz = z;
        float vw = w;
        float rx = jm::jfma(m.m00, vx, jm::jfma(m.m10, vy, jm::jfma(m.m20, vz, m.m30 * vw)));
        float ry = jm::jfma(m.m01, vx, jm::jfma(m.m11, vy, jm::jfma(m.m21, vz, m.m31 * vw)));
        float rz = jm::jfma(m.m02, vx, jm::jfma(m.m12, vy, jm::jfma(m.m22, vz, m.m32 * vw)));
        dest.x = rx;
        dest.y = ry;
        dest.z = rz;
        dest.w = vw;
        return dest;
    }

    // mulTranslation: x' = fma(m30, w, x), y' = fma(m31, w, y), z' = fma(m32, w, z),
    // w' = w. (Only the translation column contributes.)
    Vector4f& mulTranslation(const Matrix4f& m, Vector4f& dest) const {
        float vx = x;
        float vy = y;
        float vz = z;
        float vw = w;
        float rx = jm::jfma(m.m30, vw, vx);
        float ry = jm::jfma(m.m31, vw, vy);
        float rz = jm::jfma(m.m32, vw, vz);
        dest.x = rx;
        dest.y = ry;
        dest.z = rz;
        dest.w = vw;
        return dest;
    }

    // mul(Matrix4fc, Vector4f dest): property-dispatched (see header comment).
    Vector4f& mul(const Matrix4f& m, Vector4f& dest) const {
        int props = m.properties;
        if ((props & Matrix4f::PROPERTY_IDENTITY) != 0) {
            return dest.set(*this);
        }
        if ((props & Matrix4f::PROPERTY_TRANSLATION) != 0) {
            return mulTranslation(m, dest);
        }
        if ((props & Matrix4f::PROPERTY_AFFINE) != 0) {
            return mulAffine(m, dest);
        }
        return mulGeneric(m, dest);
    }
};

}  // namespace mc::render::vector4f_math

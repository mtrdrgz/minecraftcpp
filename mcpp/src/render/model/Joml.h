// 1:1 port of the org.joml (JOML 1.10.8) subset used by the Minecraft block
// model baking pipeline. Ported from the exact bytecode of the shipped
// joml-1.10.8.jar (disassembly reference: mcpp/build_dbg/joml/*.txt).
//
// Bit-exactness rules observed from the bytecode:
//  - org.joml.Math.sin(float)  = (float) java.lang.Math.sin((double) x)
//    (Options.FASTMATH defaults to false). C++: (float) std::sin((double) x).
//  - org.joml.Math.cosFromSin  goes through cosFromSinInternal (FASTMATH off).
//  - org.joml.Math.fma(float)  = java.lang.Math.fma (Runtime.HAS_Math_fma is
//    true on JDK 9+): a TRUE fused multiply-add. C++: std::fmaf.
//    Where the bytecode shows plain fmul/fadd we use plain * + (the build is
//    -ffp-contract=off so the compiler cannot fuse them behind our back).
//  - Matrix4f tracks the JOML `properties` bit set; several operations
//    dispatch on it (mul, scale, translate, rotate, mulPosition), so it is
//    replicated exactly.
//
// NO deviation from the bytecode is permitted in this file.
#pragma once

#include <cmath>
#include <cstdint>

namespace mc::render::model::joml {

// ── org.joml.Math ────────────────────────────────────────────────────────────
inline float jfma(float a, float b, float c) { return std::fmaf(a, b, c); }

inline float jsin(float x) { return (float)std::sin((double)x); }

inline float jsqrt(float x) { return (float)std::sqrt((double)x); }

inline float jinvsqrt(float x) { return 1.0f / (float)std::sqrt((double)x); }

// Math.cosFromSinInternal(sin, angle) — FASTMATH=false path
inline float cosFromSin(float sinv, float angle) {
    float cosv = jsqrt(1.0f - sinv * sinv);
    float a = angle + 1.5707964f;          // PIHalf_f
    float b = a - (float)(int)(a / 6.2831855f) * 6.2831855f; // PI2_f
    if ((double)b < 0.0) {
        b = 6.2831855f + b;
    }
    if (b >= 3.1415927f) {                 // PI_f
        return -cosv;
    }
    return cosv;
}

// Math.absEqualsOne(float): bit test |x| == 1.0f
inline bool absEqualsOne(float x) {
    union { float f; uint32_t i; } u{x};
    return (u.i & 0x7FFFFFFFu) == 0x3F800000u;
}

// ── org.joml.Vector3f ────────────────────────────────────────────────────────
struct Matrix4f;

struct Vector3f {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vector3f() = default;
    Vector3f(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vector3f& set(float x_, float y_, float z_) { x = x_; y = y_; z = z_; return *this; }

    float get(int i) const { return i == 0 ? x : (i == 1 ? y : z); }

    // div(float): inv = 1/scalar then component-wise multiply
    Vector3f& div(float scalar) {
        float inv = 1.0f / scalar;
        x = x * inv; y = y * inv; z = z * inv;
        return *this;
    }

    Vector3f& sub(const Vector3f& v) { x = x - v.x; y = y - v.y; z = z - v.z; return *this; }
    Vector3f& add(const Vector3f& v) { x = x + v.x; y = y + v.y; z = z + v.z; return *this; }
    Vector3f& mul(const Vector3f& v) { x = x * v.x; y = y * v.y; z = z * v.z; return *this; }

    // dot: fma(x, vx, fma(y, vy, z*vz))
    float dot(const Vector3f& v) const { return jfma(x, v.x, jfma(y, v.y, z * v.z)); }

    bool isFinite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }

    // normalize(): invLength = invsqrt(fma(x,x,fma(y,y,z*z)))
    Vector3f& normalize() {
        float invLength = jinvsqrt(jfma(x, x, jfma(y, y, z * z)));
        x = x * invLength; y = y * invLength; z = z * invLength;
        return *this;
    }
};

// ── org.joml.Quaternionf ─────────────────────────────────────────────────────
struct Quaternionf {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;

    Quaternionf& set(float x_, float y_, float z_, float w_) { x = x_; y = y_; z = z_; w = w_; return *this; }

    // rotateX(angle): sin = sin(angle*0.5), cos = cosFromSin(sin, angle*0.5)
    // set(w*sin + x*cos, y*cos + z*sin, z*cos - y*sin, w*cos - x*sin)
    Quaternionf& rotateX(float angle) {
        float s = jsin(angle * 0.5f);
        float c = cosFromSin(s, angle * 0.5f);
        return set(w * s + x * c, y * c + z * s, z * c - y * s, w * c - x * s);
    }

    // rotateY(angle): set(x*cos - z*sin, w*sin + y*cos, x*sin + z*cos, w*cos - y*sin)
    Quaternionf& rotateY(float angle) {
        float s = jsin(angle * 0.5f);
        float c = cosFromSin(s, angle * 0.5f);
        return set(x * c - z * s, w * s + y * c, x * s + z * c, w * c - y * s);
    }
};

// ── org.joml.Matrix3f (minimal: SymmetricGroup3 / OctahedralGroup use) ───────
struct Matrix3f {
    // column-major like JOML: mCR = column C, row R
    float m00 = 1, m01 = 0, m02 = 0;
    float m10 = 0, m11 = 1, m12 = 0;
    float m20 = 0, m21 = 0, m22 = 1;

    Matrix3f& zero() {
        m00 = m01 = m02 = m10 = m11 = m12 = m20 = m21 = m22 = 0.0f;
        return *this;
    }

    // Matrix3f.set(int column, int row, float)
    Matrix3f& set(int column, int row, float value) {
        float* cols[3][3] = { { &m00, &m01, &m02 }, { &m10, &m11, &m12 }, { &m20, &m21, &m22 } };
        *cols[column][row] = value;
        return *this;
    }

    Matrix3f& scaling(float x, float y, float z) {
        zero();
        m00 = x; m11 = y; m22 = z;
        return *this;
    }

    // Matrix3f.mul(Matrix3fc right): nm = fma chains (JOML 1.10.8)
    Matrix3f& mul(const Matrix3f& r) {
        float nm00 = jfma(m00, r.m00, jfma(m10, r.m01, m20 * r.m02));
        float nm01 = jfma(m01, r.m00, jfma(m11, r.m01, m21 * r.m02));
        float nm02 = jfma(m02, r.m00, jfma(m12, r.m01, m22 * r.m02));
        float nm10 = jfma(m00, r.m10, jfma(m10, r.m11, m20 * r.m12));
        float nm11 = jfma(m01, r.m10, jfma(m11, r.m11, m21 * r.m12));
        float nm12 = jfma(m02, r.m10, jfma(m12, r.m11, m22 * r.m12));
        float nm20 = jfma(m00, r.m20, jfma(m10, r.m21, m20 * r.m22));
        float nm21 = jfma(m01, r.m20, jfma(m11, r.m21, m21 * r.m22));
        float nm22 = jfma(m02, r.m20, jfma(m12, r.m21, m22 * r.m22));
        m00 = nm00; m01 = nm01; m02 = nm02;
        m10 = nm10; m11 = nm11; m12 = nm12;
        m20 = nm20; m21 = nm21; m22 = nm22;
        return *this;
    }
};

// ── org.joml.Matrix4f ────────────────────────────────────────────────────────
struct Matrix4f {
    static constexpr int PROPERTY_PERSPECTIVE = 1;
    static constexpr int PROPERTY_AFFINE = 2;
    static constexpr int PROPERTY_IDENTITY = 4;
    static constexpr int PROPERTY_TRANSLATION = 8;
    static constexpr int PROPERTY_ORTHONORMAL = 16;

    // column-major: mCR = column C, row R
    float m00 = 1, m01 = 0, m02 = 0, m03 = 0;
    float m10 = 0, m11 = 1, m12 = 0, m13 = 0;
    float m20 = 0, m21 = 0, m22 = 1, m23 = 0;
    float m30 = 0, m31 = 0, m32 = 0, m33 = 1;
    mutable int properties = PROPERTY_IDENTITY | PROPERTY_AFFINE | PROPERTY_TRANSLATION | PROPERTY_ORTHONORMAL;

    Matrix4f() = default;

    // Matrix4f(Matrix3fc): set(mat) -> 3x3 copied, rest identity, properties = AFFINE
    explicit Matrix4f(const Matrix3f& m) {
        m00 = m.m00; m01 = m.m01; m02 = m.m02; m03 = 0;
        m10 = m.m10; m11 = m.m11; m12 = m.m12; m13 = 0;
        m20 = m.m20; m21 = m.m21; m22 = m.m22; m23 = 0;
        m30 = 0; m31 = 0; m32 = 0; m33 = 1;
        properties = PROPERTY_AFFINE;
    }

    void setIdentityValues() {
        m00 = 1; m01 = 0; m02 = 0; m03 = 0;
        m10 = 0; m11 = 1; m12 = 0; m13 = 0;
        m20 = 0; m21 = 0; m22 = 1; m23 = 0;
        m30 = 0; m31 = 0; m32 = 0; m33 = 1;
    }

    // Matrix4f.determineProperties()
    Matrix4f& determineProperties() const {
        int p = 0;
        if (m03 == 0.0f && m13 == 0.0f) {
            if (m23 == 0.0f && m33 == 1.0f) {
                p |= PROPERTY_AFFINE;
                if (m00 == 1.0f && m01 == 0.0f && m02 == 0.0f && m10 == 0.0f && m11 == 1.0f && m12 == 0.0f
                    && m20 == 0.0f && m21 == 0.0f && m22 == 1.0f) {
                    p |= PROPERTY_TRANSLATION | PROPERTY_ORTHONORMAL;
                    if (m30 == 0.0f && m31 == 0.0f && m32 == 0.0f) {
                        p |= PROPERTY_IDENTITY;
                    }
                }
            } else if (m01 == 0.0f && m02 == 0.0f && m10 == 0.0f && m12 == 0.0f && m20 == 0.0f && m21 == 0.0f
                       && m30 == 0.0f && m31 == 0.0f && m33 == 0.0f) {
                p |= PROPERTY_PERSPECTIVE;
            }
        }
        properties = p;
        return const_cast<Matrix4f&>(*this);
    }

    bool isFinite() const {
        return std::isfinite(m00) && std::isfinite(m01) && std::isfinite(m02) && std::isfinite(m03)
            && std::isfinite(m10) && std::isfinite(m11) && std::isfinite(m12) && std::isfinite(m13)
            && std::isfinite(m20) && std::isfinite(m21) && std::isfinite(m22) && std::isfinite(m23)
            && std::isfinite(m30) && std::isfinite(m31) && std::isfinite(m32) && std::isfinite(m33);
    }

    // ── translation / translate ──────────────────────────────────────────────
    Matrix4f& translation(float x, float y, float z) {
        if ((properties & PROPERTY_IDENTITY) == 0) setIdentityValues();
        m30 = x; m31 = y; m32 = z;
        properties = PROPERTY_AFFINE | PROPERTY_TRANSLATION | PROPERTY_ORTHONORMAL; // 26
        return *this;
    }

    Matrix4f& translate(float x, float y, float z) {
        if ((properties & PROPERTY_IDENTITY) != 0) return translation(x, y, z);
        // translateGeneric
        m30 = jfma(m00, x, jfma(m10, y, jfma(m20, z, m30)));
        m31 = jfma(m01, x, jfma(m11, y, jfma(m21, z, m31)));
        m32 = jfma(m02, x, jfma(m12, y, jfma(m22, z, m32)));
        m33 = jfma(m03, x, jfma(m13, y, jfma(m23, z, m33)));
        properties &= ~(PROPERTY_PERSPECTIVE | PROPERTY_IDENTITY); // & -6
        return *this;
    }

    // ── scaling / scale ──────────────────────────────────────────────────────
    Matrix4f& scaling(float x, float y, float z) {
        if ((properties & PROPERTY_IDENTITY) == 0) setIdentityValues();
        bool one = absEqualsOne(x) && absEqualsOne(y) && absEqualsOne(z);
        m00 = x; m11 = y; m22 = z;
        properties = PROPERTY_AFFINE | (one ? PROPERTY_ORTHONORMAL : 0);
        return *this;
    }

    Matrix4f& scale(float x, float y, float z) {
        if ((properties & PROPERTY_IDENTITY) != 0) return scaling(x, y, z);
        // scaleGeneric
        bool one = absEqualsOne(x) && absEqualsOne(y) && absEqualsOne(z);
        m00 = m00 * x; m01 = m01 * x; m02 = m02 * x; m03 = m03 * x;
        m10 = m10 * y; m11 = m11 * y; m12 = m12 * y; m13 = m13 * y;
        m20 = m20 * z; m21 = m21 * z; m22 = m22 * z; m23 = m23 * z;
        properties &= ~(PROPERTY_PERSPECTIVE | PROPERTY_IDENTITY | PROPERTY_TRANSLATION
                        | (one ? 0 : PROPERTY_ORTHONORMAL));
        return *this;
    }

    Matrix4f& scale(const Vector3f& v) { return scale(v.x, v.y, v.z); }

    // ── rotation builders ────────────────────────────────────────────────────
    Matrix4f& rotationX(float ang) {
        float s = jsin(ang), c = cosFromSin(s, ang);
        if ((properties & PROPERTY_IDENTITY) == 0) setIdentityValues();
        m11 = c; m12 = s; m21 = -s; m22 = c;
        properties = PROPERTY_AFFINE | PROPERTY_ORTHONORMAL; // 18
        return *this;
    }

    Matrix4f& rotationY(float ang) {
        float s = jsin(ang), c = cosFromSin(s, ang);
        if ((properties & PROPERTY_IDENTITY) == 0) setIdentityValues();
        m00 = c; m02 = -s; m20 = s; m22 = c;
        properties = PROPERTY_AFFINE | PROPERTY_ORTHONORMAL;
        return *this;
    }

    Matrix4f& rotationZ(float ang) {
        float s = jsin(ang), c = cosFromSin(s, ang);
        if ((properties & PROPERTY_IDENTITY) == 0) setIdentityValues();
        m00 = c; m01 = s; m10 = -s; m11 = c;
        properties = PROPERTY_AFFINE | PROPERTY_ORTHONORMAL;
        return *this;
    }

    // rotation(angle, x, y, z) — axis fast paths then rotationInternal
    Matrix4f& rotation(float angle, float x, float y, float z) {
        if (y == 0.0f && z == 0.0f && absEqualsOne(x)) return rotationX(x * angle);
        if (x == 0.0f && z == 0.0f && absEqualsOne(y)) return rotationY(y * angle);
        if (x == 0.0f && y == 0.0f && absEqualsOne(z)) return rotationZ(z * angle);
        // rotationInternal
        float sinA = jsin(angle);
        float cosA = cosFromSin(sinA, angle);
        float C = 1.0f - cosA;
        float xy = x * y, xz = x * z, yz = y * z;
        if ((properties & PROPERTY_IDENTITY) == 0) setIdentityValues();
        m00 = cosA + x * x * C;
        m10 = xy * C - z * sinA;
        m20 = xz * C + y * sinA;
        m01 = xy * C + z * sinA;
        m11 = cosA + y * y * C;
        m21 = yz * C - x * sinA;
        m02 = xz * C - y * sinA;
        m12 = yz * C + x * sinA;
        m22 = cosA + z * z * C;
        properties = PROPERTY_AFFINE | PROPERTY_ORTHONORMAL;
        return *this;
    }

    Matrix4f& rotation(float angle, const Vector3f& axis) { return rotation(angle, axis.x, axis.y, axis.z); }

    // rotationZYX(angleZ, angleY, angleX)
    Matrix4f& rotationZYX(float angleZ, float angleY, float angleX) {
        float sinX = jsin(angleX), cosX = cosFromSin(sinX, angleX);
        float sinY = jsin(angleY), cosY = cosFromSin(sinY, angleY);
        float sinZ = jsin(angleZ), cosZ = cosFromSin(sinZ, angleZ);
        float nm20 = cosZ * sinY;  // local 10
        float nm21 = sinZ * sinY;  // local 11
        m00 = cosZ * cosY;
        m01 = sinZ * cosY;
        m02 = -sinY;
        m03 = 0.0f;
        m10 = -sinZ * cosX + nm20 * sinX;
        m11 = cosZ * cosX + nm21 * sinX;
        m12 = cosY * sinX;
        m13 = 0.0f;
        m20 = (-sinZ) * (-sinX) + nm20 * cosX;
        m21 = cosZ * (-sinX) + nm21 * cosX;
        m22 = cosY * cosX;
        m23 = 0.0f;
        m30 = 0.0f; m31 = 0.0f; m32 = 0.0f; m33 = 1.0f;
        properties = PROPERTY_AFFINE | PROPERTY_ORTHONORMAL;
        return *this;
    }

    // rotation(Quaternionfc)
    Matrix4f& rotation(const Quaternionf& q) {
        float w2 = q.w * q.w;
        float x2 = q.x * q.x;
        float y2 = q.y * q.y;
        float z2 = q.z * q.z;
        float zw = q.z * q.w; float dzw = zw + zw;
        float xy = q.x * q.y; float dxy = xy + xy;
        float xz = q.x * q.z; float dxz = xz + xz;
        float yw = q.y * q.w; float dyw = yw + yw;
        float yz = q.y * q.z; float dyz = yz + yz;
        float xw = q.x * q.w; float dxw = xw + xw;
        if ((properties & PROPERTY_IDENTITY) == 0) setIdentityValues();
        m00 = w2 + x2 - z2 - y2;
        m01 = dxy + dzw;
        m02 = dxz - dyw;
        m10 = -dzw + dxy;
        m11 = y2 - z2 + w2 - x2;
        m12 = dyz + dxw;
        m20 = dyw + dxz;
        m21 = dyz - dxw;
        m22 = z2 - y2 - x2 + w2;
        properties = PROPERTY_AFFINE | PROPERTY_ORTHONORMAL;
        return *this;
    }

    // rotate(Quaternionfc): the pipeline only ever rotates an IDENTITY matrix
    // (Transformation.compose with null translation); other dispatch arms are
    // unreachable here and intentionally hard-fail instead of approximating.
    Matrix4f& rotate(const Quaternionf& q) {
        if ((properties & PROPERTY_IDENTITY) != 0) return rotation(q);
        // rotateTranslation / rotateAffine / rotateGeneric: unreachable in the
        // block-model pipeline (would need a faithful port before use).
        __builtin_trap();
    }

    // ── mul ─────────────────────────────────────────────────────────────────
    Matrix4f& mul(const Matrix4f& right) {
        if ((properties & PROPERTY_IDENTITY) != 0) { *this = right; return *this; }
        if ((right.properties & PROPERTY_IDENTITY) != 0) return *this;
        if ((properties & PROPERTY_TRANSLATION) != 0 && (right.properties & PROPERTY_AFFINE) != 0)
            return mulTranslationAffine(right);
        if ((properties & PROPERTY_AFFINE) != 0 && (right.properties & PROPERTY_AFFINE) != 0)
            return mulAffine(right);
        if ((properties & PROPERTY_PERSPECTIVE) != 0 && (right.properties & PROPERTY_AFFINE) != 0)
            __builtin_trap(); // mulPerspectiveAffine: unreachable in this pipeline
        if ((right.properties & PROPERTY_AFFINE) != 0)
            return mulAffineR(right);
        return mul0(right);
    }

    Matrix4f& mulAffine(const Matrix4f& right) {
        float l00 = m00, l01 = m01, l02 = m02;
        float l10 = m10, l11 = m11, l12 = m12;
        float l20 = m20, l21 = m21, l22 = m22;
        float r00 = right.m00, r01 = right.m01, r02 = right.m02;
        float r10 = right.m10, r11 = right.m11, r12 = right.m12;
        float r20 = right.m20, r21 = right.m21, r22 = right.m22;
        float r30 = right.m30, r31 = right.m31, r32 = right.m32;
        float nm00 = jfma(l00, r00, jfma(l10, r01, l20 * r02));
        float nm01 = jfma(l01, r00, jfma(l11, r01, l21 * r02));
        float nm02 = jfma(l02, r00, jfma(l12, r01, l22 * r02));
        float nm10 = jfma(l00, r10, jfma(l10, r11, l20 * r12));
        float nm11 = jfma(l01, r10, jfma(l11, r11, l21 * r12));
        float nm12 = jfma(l02, r10, jfma(l12, r11, l22 * r12));
        float nm20 = jfma(l00, r20, jfma(l10, r21, l20 * r22));
        float nm21 = jfma(l01, r20, jfma(l11, r21, l21 * r22));
        float nm22 = jfma(l02, r20, jfma(l12, r21, l22 * r22));
        float nm30 = jfma(l00, r30, jfma(l10, r31, jfma(l20, r32, m30)));
        float nm31 = jfma(l01, r30, jfma(l11, r31, jfma(l21, r32, m31)));
        float nm32 = jfma(l02, r30, jfma(l12, r31, jfma(l22, r32, m32)));
        int props = PROPERTY_AFFINE | (properties & right.properties & PROPERTY_ORTHONORMAL);
        m00 = nm00; m01 = nm01; m02 = nm02; /* m03 unchanged */
        m10 = nm10; m11 = nm11; m12 = nm12; /* m13 unchanged */
        m20 = nm20; m21 = nm21; m22 = nm22; /* m23 unchanged */
        m30 = nm30; m31 = nm31; m32 = nm32; /* m33 unchanged */
        properties = props;
        return *this;
    }

    // this is a pure translation, right is affine
    Matrix4f& mulTranslationAffine(const Matrix4f& right) {
        float t30 = m30, t31 = m31, t32 = m32;
        float t03 = m03, t13 = m13, t23 = m23, t33 = m33;
        m00 = right.m00; m01 = right.m01; m02 = right.m02; m03 = t03;
        m10 = right.m10; m11 = right.m11; m12 = right.m12; m13 = t13;
        m20 = right.m20; m21 = right.m21; m22 = right.m22; m23 = t23;
        m30 = right.m30 + t30; m31 = right.m31 + t31; m32 = right.m32 + t32; m33 = t33;
        properties = PROPERTY_AFFINE | (right.properties & PROPERTY_ORTHONORMAL);
        return *this;
    }

    Matrix4f& mulAffineR(const Matrix4f& right) {
        float nm00 = jfma(m00, right.m00, jfma(m10, right.m01, m20 * right.m02));
        float nm01 = jfma(m01, right.m00, jfma(m11, right.m01, m21 * right.m02));
        float nm02 = jfma(m02, right.m00, jfma(m12, right.m01, m22 * right.m02));
        float nm03 = jfma(m03, right.m00, jfma(m13, right.m01, m23 * right.m02));
        float nm10 = jfma(m00, right.m10, jfma(m10, right.m11, m20 * right.m12));
        float nm11 = jfma(m01, right.m10, jfma(m11, right.m11, m21 * right.m12));
        float nm12 = jfma(m02, right.m10, jfma(m12, right.m11, m22 * right.m12));
        float nm13 = jfma(m03, right.m10, jfma(m13, right.m11, m23 * right.m12));
        float nm20 = jfma(m00, right.m20, jfma(m10, right.m21, m20 * right.m22));
        float nm21 = jfma(m01, right.m20, jfma(m11, right.m21, m21 * right.m22));
        float nm22 = jfma(m02, right.m20, jfma(m12, right.m21, m22 * right.m22));
        float nm23 = jfma(m03, right.m20, jfma(m13, right.m21, m23 * right.m22));
        float nm30 = jfma(m00, right.m30, jfma(m10, right.m31, jfma(m20, right.m32, m30)));
        float nm31 = jfma(m01, right.m30, jfma(m11, right.m31, jfma(m21, right.m32, m31)));
        float nm32 = jfma(m02, right.m30, jfma(m12, right.m31, jfma(m22, right.m32, m32)));
        float nm33 = jfma(m03, right.m30, jfma(m13, right.m31, jfma(m23, right.m32, m33)));
        m00 = nm00; m01 = nm01; m02 = nm02; m03 = nm03;
        m10 = nm10; m11 = nm11; m12 = nm12; m13 = nm13;
        m20 = nm20; m21 = nm21; m22 = nm22; m23 = nm23;
        m30 = nm30; m31 = nm31; m32 = nm32; m33 = nm33;
        properties = properties & ~(PROPERTY_IDENTITY | PROPERTY_PERSPECTIVE | PROPERTY_TRANSLATION | PROPERTY_ORTHONORMAL);
        return *this;
    }

    Matrix4f& mul0(const Matrix4f& right) {
        float nm00 = jfma(m00, right.m00, jfma(m10, right.m01, jfma(m20, right.m02, m30 * right.m03)));
        float nm01 = jfma(m01, right.m00, jfma(m11, right.m01, jfma(m21, right.m02, m31 * right.m03)));
        float nm02 = jfma(m02, right.m00, jfma(m12, right.m01, jfma(m22, right.m02, m32 * right.m03)));
        float nm03 = jfma(m03, right.m00, jfma(m13, right.m01, jfma(m23, right.m02, m33 * right.m03)));
        float nm10 = jfma(m00, right.m10, jfma(m10, right.m11, jfma(m20, right.m12, m30 * right.m13)));
        float nm11 = jfma(m01, right.m10, jfma(m11, right.m11, jfma(m21, right.m12, m31 * right.m13)));
        float nm12 = jfma(m02, right.m10, jfma(m12, right.m11, jfma(m22, right.m12, m32 * right.m13)));
        float nm13 = jfma(m03, right.m10, jfma(m13, right.m11, jfma(m23, right.m12, m33 * right.m13)));
        float nm20 = jfma(m00, right.m20, jfma(m10, right.m21, jfma(m20, right.m22, m30 * right.m23)));
        float nm21 = jfma(m01, right.m20, jfma(m11, right.m21, jfma(m21, right.m22, m31 * right.m23)));
        float nm22 = jfma(m02, right.m20, jfma(m12, right.m21, jfma(m22, right.m22, m32 * right.m23)));
        float nm23 = jfma(m03, right.m20, jfma(m13, right.m21, jfma(m23, right.m22, m33 * right.m23)));
        float nm30 = jfma(m00, right.m30, jfma(m10, right.m31, jfma(m20, right.m32, m30 * right.m33)));
        float nm31 = jfma(m01, right.m30, jfma(m11, right.m31, jfma(m21, right.m32, m31 * right.m33)));
        float nm32 = jfma(m02, right.m30, jfma(m12, right.m31, jfma(m22, right.m32, m32 * right.m33)));
        float nm33 = jfma(m03, right.m30, jfma(m13, right.m31, jfma(m23, right.m32, m33 * right.m33)));
        m00 = nm00; m01 = nm01; m02 = nm02; m03 = nm03;
        m10 = nm10; m11 = nm11; m12 = nm12; m13 = nm13;
        m20 = nm20; m21 = nm21; m22 = nm22; m23 = nm23;
        m30 = nm30; m31 = nm31; m32 = nm32; m33 = nm33;
        properties = 0;
        return *this;
    }

    // ── invertAffine (transcribed from bytecode, locals annotated) ───────────
    Matrix4f invertAffine() const {
        float m11m00 = m00 * m11;            // local 2
        float m10m01 = m01 * m10;            // local 3
        float m10m02 = m02 * m10;            // local 4
        float m12m00 = m00 * m12;            // local 5
        float m12m01 = m01 * m12;            // local 6
        float m11m02 = m02 * m11;            // local 7
        float det = (m11m00 - m10m01) * m22 + (m10m02 - m12m00) * m21 + (m12m01 - m11m02) * m20; // local 8
        float s = 1.0f / det;                // local 9
        float m10m22 = m10 * m22;            // local 10
        float m10m21 = m10 * m21;            // local 11
        float m11m22 = m11 * m22;            // local 12
        float m11m20 = m11 * m20;            // local 13
        float m12m21 = m12 * m21;            // local 14
        float m12m20 = m12 * m20;            // local 15
        float m20m02 = m20 * m02;            // local 16
        float m20m01 = m20 * m01;            // local 17
        float m21m02 = m21 * m02;            // local 18
        float m21m00 = m21 * m00;            // local 19
        float m22m01 = m22 * m01;            // local 20
        float m22m00 = m22 * m00;            // local 21
        float nm31 = (m20m02 * m31 - m20m01 * m32 + m21m00 * m32 - m21m02 * m30 + m22m01 * m30 - m22m00 * m31) * s; // local 22
        float nm32 = (m11m02 * m30 - m12m01 * m30 + m12m00 * m31 - m10m02 * m31 + m10m01 * m32 - m11m00 * m32) * s; // local 23
        Matrix4f dest;
        dest.m00 = (m11m22 - m12m21) * s;
        dest.m01 = (m21m02 - m22m01) * s;
        dest.m02 = (m12m01 - m11m02) * s;
        dest.m03 = 0.0f;
        dest.m10 = (m12m20 - m10m22) * s;
        dest.m11 = (m22m00 - m20m02) * s;
        dest.m12 = (m10m02 - m12m00) * s;
        dest.m13 = 0.0f;
        dest.m20 = (m10m21 - m11m20) * s;
        dest.m21 = (m20m01 - m21m00) * s;
        dest.m22 = (m11m00 - m10m01) * s;
        dest.m23 = 0.0f;
        dest.m30 = (m10m22 * m31 - m10m21 * m32 + m11m20 * m32 - m11m22 * m30 + m12m21 * m30 - m12m20 * m31) * s;
        dest.m31 = nm31;
        dest.m32 = nm32;
        dest.m33 = 1.0f;
        dest.properties = PROPERTY_AFFINE;
        return dest;
    }

    // ── transform ────────────────────────────────────────────────────────────
    // Vector3f.mulPosition(Matrix4fc): property-dispatched
    void transformPosition(Vector3f& v) const {
        if ((properties & PROPERTY_IDENTITY) != 0) return;
        if ((properties & PROPERTY_TRANSLATION) != 0) {
            // mulPositionTranslation: v + (m30, m31, m32)
            v.x = v.x + m30;
            v.y = v.y + m31;
            v.z = v.z + m32;
            return;
        }
        // mulPositionGeneric
        float x = v.x, y = v.y, z = v.z;
        v.x = jfma(m00, x, jfma(m10, y, jfma(m20, z, m30)));
        v.y = jfma(m01, x, jfma(m11, y, jfma(m21, z, m31)));
        v.z = jfma(m02, x, jfma(m12, y, jfma(m22, z, m32)));
    }

    // Vector3f.mulDirection(Matrix4fc, dest): always generic
    void transformDirection(const Vector3f& in, Vector3f& dest) const {
        float x = in.x, y = in.y, z = in.z;
        dest.x = jfma(m00, x, jfma(m10, y, m20 * z));
        dest.y = jfma(m01, x, jfma(m11, y, m21 * z));
        dest.z = jfma(m02, x, jfma(m12, y, m22 * z));
    }

    void transformDirection(Vector3f& v) const { transformDirection(v, v); }
};

// ── org.joml.GeometryUtils.normal(v0, v1, v2, dest) ─────────────────────────
// cross(v1-v0, v2-v0) then normalize()
inline void geometryNormal(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, Vector3f& dest) {
    dest.x = (v1.y - v0.y) * (v2.z - v0.z) - (v1.z - v0.z) * (v2.y - v0.y);
    dest.y = (v1.z - v0.z) * (v2.x - v0.x) - (v1.x - v0.x) * (v2.z - v0.z);
    dest.z = (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x);
    dest.normalize();
}

// ── com.mojang.math.MatrixUtil.isIdentity ────────────────────────────────────
inline bool checkProperty(const Matrix4f& m, int property) {
    if ((m.properties & property) != 0) return true;
    int current = m.properties;
    m.determineProperties();
    m.properties |= current; // assume(properties | current)
    return (m.properties & property) != 0;
}

inline bool matrixIsIdentity(const Matrix4f& m) { return checkProperty(m, Matrix4f::PROPERTY_IDENTITY); }

} // namespace mc::render::model::joml

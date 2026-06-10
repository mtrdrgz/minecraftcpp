// 1:1 port of the pure org.joml.Quaternionf operations (JOML 1.10.8, the exact
// joml-1.10.8.jar shipped with Minecraft 26.1.2) used by the render pipeline:
//   mul(Quaternionfc) / conjugate / normalize / dot / lengthSquared / invert /
//   rotationXYZ.
//
// SELF-CONTAINED on purpose: this header does NOT include or modify the shared
// render/model/Joml.h (which carries a separate Quaternionf with only rotateX/
// rotateY). Every constant/formula/order below is transcribed VERBATIM from the
// org.joml.Quaternionf bytecode (javap -p -c org.joml.Quaternionf against the real
// jar). No deviation is permitted.
//
// Bit-exactness facts established from the bytecode of the shipped jar:
//
//  * org.joml.Math.fma(float,float,float):
//        return Runtime.HAS_Math_fma ? java.lang.Math.fma(a,b,c) : a*b + c;
//    where Runtime.HAS_Math_fma = Options.USE_MATH_FMA && hasMathFma(), and
//    Options.USE_MATH_FMA = hasOption(System.getProperty("joml.useMathFma",
//    "false")) — i.e. FALSE by default. So under the default config the ground
//    truth runs under, Math.fma is the PLAIN two-rounding a*b + c. We mirror it
//    with jq_fma below (and the build is -ffp-contract=off so the compiler cannot
//    silently fuse it into a hardware FMA).
//
//  * org.joml.Math.invsqrt(float x) = (float)(1.0 / 1.0)? NO — bytecode is
//        return 1.0f / (float)java.lang.Math.sqrt((double)x);
//    i.e. fconst_1 ; (double)x ; Math.sqrt ; (float) ; fdiv. We mirror that exactly
//    (the float divide happens AFTER narrowing the double sqrt back to float).
//
//  * org.joml.Math.sin(float) with Options.FASTMATH=false (the default) =
//        (float) java.lang.Math.sin((double) x).
//    cosFromSin with FASTMATH=false goes through cosFromSinInternal. These are the
//    ONLY libm-dependent ops here and are used solely by rotationXYZ; the
//    deterministic battery (mul/conjugate/normalize/dot/lengthSquared/invert) never
//    touches a transcendental and is therefore bit-exact regardless of libm. The
//    rotationXYZ rows match the same way the certified Joml.h rotationX/Y/Z rows do:
//    (float)std::sin((double)x) == (float)Math.sin((double)x).
#pragma once

#include <cmath>
#include <cstdint>

namespace mc::render::model::quat {

// org.joml.Math.fma — PLAIN a*b + c (USE_MATH_FMA defaults to false). See header note.
inline float jq_fma(float a, float b, float c) { return a * b + c; }

// org.joml.Math.invsqrt(float): 1.0f / (float)java.lang.Math.sqrt((double)x).
inline float jq_invsqrt(float x) { return 1.0f / (float)std::sqrt((double)x); }

// org.joml.Math.sin(float), FASTMATH=false default: (float)java.lang.Math.sin((double)x).
inline float jq_sin(float x) { return (float)std::sin((double)x); }

// org.joml.Math.sqrt(float) helper for cosFromSinInternal: (float)Math.sqrt((double)x).
inline float jq_sqrt(float x) { return (float)std::sqrt((double)x); }

// org.joml.Math.cosFromSin(sin, angle) with FASTMATH=false -> cosFromSinInternal.
// Transcribed identically to the certified Joml.h::cosFromSin.
inline float jq_cosFromSin(float sinv, float angle) {
    float cosv = jq_sqrt(1.0f - sinv * sinv);
    float a = angle + 1.5707964f;                                 // PIHalf_f
    float b = a - (float)(int)(a / 6.2831855f) * 6.2831855f;      // PI2_f
    if ((double)b < 0.0) {
        b = 6.2831855f + b;
    }
    if (b >= 3.1415927f) {                                        // PI_f
        return -cosv;
    }
    return cosv;
}

// ── org.joml.Quaternionf ─────────────────────────────────────────────────────
// Field order/defaults match org.joml.Quaternionf: x,y,z,w with identity (0,0,0,1).
struct Quaternionf {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;

    Quaternionf() = default;
    Quaternionf(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    Quaternionf& set(float x_, float y_, float z_, float w_) {
        x = x_; y = y_; z = z_; w = w_; return *this;
    }

    // mul(Quaternionfc q) -> mul(q, this). Transcribed from bytecode; Math.fma is
    // the plain jq_fma. dest.set(
    //   fma(w, q.x, fma(x, q.w, fma(y, q.z, -z * q.y))),
    //   fma(w, q.y, fma(-x, q.z, fma(y, q.w,  z * q.x))),
    //   fma(w, q.z, fma(x, q.y, fma(-y, q.x,  z * q.w))),
    //   fma(w, q.w, fma(-x, q.x, fma(-y, q.y, -z * q.z))))
    Quaternionf& mul(const Quaternionf& q) {
        float nx = jq_fma(w, q.x, jq_fma(x, q.w, jq_fma(y, q.z, -z * q.y)));
        float ny = jq_fma(w, q.y, jq_fma(-x, q.z, jq_fma(y, q.w, z * q.x)));
        float nz = jq_fma(w, q.z, jq_fma(x, q.y, jq_fma(-y, q.x, z * q.w)));
        float nw = jq_fma(w, q.w, jq_fma(-x, q.x, jq_fma(-y, q.y, -z * q.z)));
        return set(nx, ny, nz, nw);
    }

    // conjugate() -> conjugate(this): negate x,y,z; keep w.
    Quaternionf& conjugate() {
        x = -x; y = -y; z = -z; /* w unchanged */
        return *this;
    }

    // dot(Quaternionf q): plain fmul/fadd, left-associated (NOT fma):
    //   ((x*q.x + y*q.y) + z*q.z) + w*q.w
    float dot(const Quaternionf& q) const {
        return ((x * q.x + y * q.y) + z * q.z) + w * q.w;
    }

    // lengthSquared(): fma(x, x, fma(y, y, fma(z, z, w*w)))
    float lengthSquared() const {
        return jq_fma(x, x, jq_fma(y, y, jq_fma(z, z, w * w)));
    }

    // normalize() -> normalize(this):
    //   invNorm = Math.invsqrt(fma(x,x, fma(y,y, fma(z,z, w*w))));
    //   x*=invNorm; y*=invNorm; z*=invNorm; w*=invNorm;
    Quaternionf& normalize() {
        float invNorm = jq_invsqrt(jq_fma(x, x, jq_fma(y, y, jq_fma(z, z, w * w))));
        x = x * invNorm; y = y * invNorm; z = z * invNorm; w = w * invNorm;
        return *this;
    }

    // invert() -> invert(this):
    //   invNorm = 1.0f / fma(x,x, fma(y,y, fma(z,z, w*w)));   // NOTE: 1/||q||^2, not invsqrt
    //   x = -x * invNorm; y = -y * invNorm; z = -z * invNorm; w =  w * invNorm;
    Quaternionf& invert() {
        float invNorm = 1.0f / jq_fma(x, x, jq_fma(y, y, jq_fma(z, z, w * w)));
        x = -x * invNorm; y = -y * invNorm; z = -z * invNorm; w = w * invNorm;
        return *this;
    }

    // rotationXYZ(angleX, angleY, angleZ): builds a quaternion from intrinsic
    // X*Y*Z Euler angles. Uses Math.sin/cosFromSin (libm — see header note).
    // Bytecode locals:
    //   sx = sin(angleX*0.5); cx = cosFromSin(sx, angleX*0.5);
    //   sy = sin(angleY*0.5); cy = cosFromSin(sy, angleY*0.5);
    //   sz = sin(angleZ*0.5); cz = cosFromSin(sz, angleZ*0.5);
    //   cycz = cy*cz; sysz = sy*sz; sycz = sy*cz; cysz = cy*sz;
    //   w = cx*cycz - sx*sysz;
    //   x = sx*cycz + cx*sysz;
    //   y = cx*sycz - sx*cysz;
    //   z = cx*cysz + sx*sycz;
    Quaternionf& rotationXYZ(float angleX, float angleY, float angleZ) {
        float sx = jq_sin(angleX * 0.5f);
        float cx = jq_cosFromSin(sx, angleX * 0.5f);
        float sy = jq_sin(angleY * 0.5f);
        float cy = jq_cosFromSin(sy, angleY * 0.5f);
        float sz = jq_sin(angleZ * 0.5f);
        float cz = jq_cosFromSin(sz, angleZ * 0.5f);
        float cycz = cy * cz;
        float sysz = sy * sz;
        float sycz = sy * cz;
        float cysz = cy * sz;
        w = cx * cycz - sx * sysz;
        x = sx * cycz + cx * sysz;
        y = cx * sycz - sx * cysz;
        z = cx * cysz + sx * sycz;
        return *this;
    }
};

} // namespace mc::render::model::quat

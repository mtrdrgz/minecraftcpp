#pragma once

// Bit-exact 1:1 port of the pure orientation math used by
// net.minecraft.client.Camera#setRotation (Minecraft Java Edition 26.1.2).
//
// Source (26.1.2/src/net/minecraft/client/Camera.java, lines 330-338):
//
//   private static final Vector3f FORWARDS = new Vector3f(0.0F, 0.0F, -1.0F);
//   private static final Vector3f UP       = new Vector3f(0.0F, 1.0F,  0.0F);
//   private static final Vector3f LEFT     = new Vector3f(-1.0F, 0.0F, 0.0F);
//   ...
//   protected void setRotation(final float yRot, final float xRot) {
//      this.xRot = xRot;
//      this.yRot = yRot;
//      this.rotation.rotationYXZ((float) Math.PI - yRot * (float) (Math.PI / 180.0),
//                                -xRot * (float) (Math.PI / 180.0), 0.0F);
//      FORWARDS.rotate(this.rotation, this.forwards);
//      UP.rotate(this.rotation, this.up);
//      LEFT.rotate(this.rotation, this.left);
//      ...
//   }
//
// Camera itself cannot be reached by reflection (its field initialisers call
// Minecraft.getInstance() and allocate GL resources), so the math is replicated
// here as a pure function of (yRot, xRot) and certified against a Java harness
// (CameraRotationMathParity.java) that drives the REAL org.joml.Quaternionf and
// org.joml.Vector3f from the exact joml-1.10.8.jar the engine links.
//
// The two JOML primitives this exercises are GENUINELY UNGATED elsewhere in the
// C++ port: org.joml.Quaternionf#rotationYXZ(float,float,float) and
// org.joml.Vector3f#rotate(Quaternionfc) (== Quaternionf#transform). Neither is
// covered by render/model/Joml.h or render/model/QuaternionMath.h. They are
// transcribed VERBATIM from the bytecode of the shipped jar (javap -p -c).
//
// Bit-exactness facts (identical to the certified QuaternionMath.h):
//   * org.joml.Math.fma(float,float,float) = a*b + c   (USE_MATH_FMA defaults
//     false). Mirrored by jfma; build is -ffp-contract=off so the compiler
//     cannot fuse it into a hardware FMA.
//   * org.joml.Math.sin(float) (FASTMATH=false default) = (float)Math.sin((double)x).
//   * cosFromSin(sin,angle) (FASTMATH=false) routes through cosFromSinInternal.
//   * The angle pre-scaling uses Java's exact literals:
//       (float) Math.PI                       == 3.1415927f
//       (float) (Math.PI / 180.0)             == 0.017453292f
//     We compute them as Java does: (float)M_PI and (float)(M_PI/180.0) at
//     double precision, then narrow — so the constants are bit-identical.

#include <cmath>
#include <cstdint>

namespace mc {
namespace client {
namespace camera_rot {

// ── org.joml.Math helpers (FASTMATH=false, USE_MATH_FMA=false defaults) ──────

// org.joml.Math.fma — PLAIN a*b + c.
inline float jfma(float a, float b, float c) { return a * b + c; }

// org.joml.Math.sin(float): (float)java.lang.Math.sin((double)x).
inline float jsin(float x) { return static_cast<float>(std::sin(static_cast<double>(x))); }

// org.joml.Math.sqrt(float) helper: (float)java.lang.Math.sqrt((double)x).
inline float jsqrt(float x) { return static_cast<float>(std::sqrt(static_cast<double>(x))); }

// org.joml.Math.cosFromSin(sin, angle), FASTMATH=false -> cosFromSinInternal.
// Transcribed identically to render/model/QuaternionMath.h::jq_cosFromSin.
inline float jcosFromSin(float sinv, float angle) {
    float cosv = jsqrt(1.0f - sinv * sinv);
    float a = angle + 1.5707964f;                                  // PIHalf_f
    float b = a - static_cast<float>(static_cast<int>(a / 6.2831855f)) * 6.2831855f; // PI2_f
    if (static_cast<double>(b) < 0.0) {
        b = 6.2831855f + b;
    }
    if (b >= 3.1415927f) {                                         // PI_f
        return -cosv;
    }
    return cosv;
}

// Java float literals produced from double-precision constants, narrowed.
//   (float) Math.PI            and  (float)(Math.PI / 180.0)
// java.lang.Math.PI == 3.141592653589793 (the exact double); we use that literal
// so the narrowed floats are bit-identical to Java's.
inline constexpr double kPi = 3.141592653589793;
inline float pi_f()    { return static_cast<float>(kPi); }
inline float deg2rad() { return static_cast<float>(kPi / 180.0); }

// ── org.joml.Quaternionf (only the pieces setRotation needs) ─────────────────
struct Quaternionf {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;

    // rotationYXZ(angleY, angleX, angleZ): intrinsic Y*X*Z Euler -> unit quaternion.
    // Bytecode (org.joml.Quaternionf, joml-1.10.8.jar):
    //   sx = sin(angleX*0.5);  cx = cosFromSin(sx, angleX*0.5);
    //   sy = sin(angleY*0.5);  cy = cosFromSin(sy, angleY*0.5);
    //   sz = sin(angleZ*0.5);  cz = cosFromSin(sz, angleZ*0.5);
    //   x10 = cy*sx;  x11 = sy*cx;  x12 = sy*sx;  x13 = cy*cx;
    //   x = x10*cz + x11*sz;
    //   y = x11*cz - x10*sz;
    //   z = x13*sz - x12*cz;
    //   w = x13*cz + x12*sz;
    // NOTE: the final four lines are plain fmul/fadd/fsub (NO Math.fma).
    Quaternionf& rotationYXZ(float angleY, float angleX, float angleZ) {
        float sx = jsin(angleX * 0.5f);
        float cx = jcosFromSin(sx, angleX * 0.5f);
        float sy = jsin(angleY * 0.5f);
        float cy = jcosFromSin(sy, angleY * 0.5f);
        float sz = jsin(angleZ * 0.5f);
        float cz = jcosFromSin(sz, angleZ * 0.5f);
        float x10 = cy * sx;
        float x11 = sy * cx;
        float x12 = sy * sx;
        float x13 = cy * cx;
        x = x10 * cz + x11 * sz;
        y = x11 * cz - x10 * sz;
        z = x13 * sz - x12 * cz;
        w = x13 * cz + x12 * sz;
        return *this;
    }
};

// ── org.joml.Vector3f (only rotate(Quaternionfc, dest) == transform) ─────────
struct Vector3f {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vector3f() = default;
    Vector3f(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    // rotate(Quaternionfc q, Vector3f dest) delegates to q.transform(this, dest).
    // Quaternionf.transform(float vx, float vy, float vz, Vector3f dest) bytecode:
    //   xx=q.x*q.x; yy=q.y*q.y; zz=q.z*q.z; ww=q.w*q.w;
    //   xy=q.x*q.y; xz=q.x*q.z; yz=q.y*q.z; xw=q.x*q.w; zw=q.z*q.w; yw=q.y*q.w;
    //   k = 1.0f / (xx + yy + zz + ww);
    //   dest.x = fma((xx-yy-zz+ww)*k, vx, fma(2*(xy-zw)*k, vy, 2*(xz+yw)*k * vz));
    //   dest.y = fma(2*(xy+zw)*k, vx, fma((yy-xx-zz+ww)*k, vy, 2*(yz-xw)*k * vz));
    //   dest.z = fma(2*(xz-yw)*k, vx, fma(2*(yz+xw)*k, vy, (zz-xx-yy+ww)*k * vz));
    // The innermost addend of each row is a plain fmul (NOT fma); the two outer
    // combines per row are Math.fma (== plain a*b+c here).
    Vector3f rotateInto(const Quaternionf& q) const {
        float vx = x, vy = y, vz = z;
        float xx = q.x * q.x;
        float yy = q.y * q.y;
        float zz = q.z * q.z;
        float ww = q.w * q.w;
        float xy = q.x * q.y;
        float xz = q.x * q.z;
        float yz = q.y * q.z;
        float xw = q.x * q.w;
        float zw = q.z * q.w;
        float yw = q.y * q.w;
        float k = 1.0f / (xx + yy + zz + ww);
        Vector3f dest;
        dest.x = jfma((xx - yy - zz + ww) * k, vx,
                      jfma(2.0f * (xy - zw) * k, vy, 2.0f * (xz + yw) * k * vz));
        dest.y = jfma(2.0f * (xy + zw) * k, vx,
                      jfma((yy - xx - zz + ww) * k, vy, 2.0f * (yz - xw) * k * vz));
        dest.z = jfma(2.0f * (xz - yw) * k, vx,
                      jfma(2.0f * (yz + xw) * k, vy, (zz - xx - yy + ww) * k * vz));
        return dest;
    }
};

// Aggregate result of Camera#setRotation: the orientation quaternion plus the
// three rotated basis vectors (forwards/up/left) that the camera publishes.
struct CameraOrientation {
    Quaternionf rotation;
    Vector3f forwards;
    Vector3f up;
    Vector3f left;
};

// 1:1 port of net.minecraft.client.Camera#setRotation(yRot, xRot).
//   FORWARDS = (0, 0, -1);  UP = (0, 1, 0);  LEFT = (-1, 0, 0).
inline CameraOrientation setRotation(float yRot, float xRot) {
    const Vector3f FORWARDS(0.0f, 0.0f, -1.0f);
    const Vector3f UP(0.0f, 1.0f, 0.0f);
    const Vector3f LEFT(-1.0f, 0.0f, 0.0f);

    CameraOrientation o;
    o.rotation.rotationYXZ(pi_f() - yRot * deg2rad(), -xRot * deg2rad(), 0.0f);
    o.forwards = FORWARDS.rotateInto(o.rotation);
    o.up = UP.rotateInto(o.rotation);
    o.left = LEFT.rotateInto(o.rotation);
    return o;
}

} // namespace camera_rot
} // namespace client
} // namespace mc

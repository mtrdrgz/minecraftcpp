// 1:1 port of com.mojang.math.Axis.of(Vector3f) and the org.joml.Quaternionf
// arbitrary-axis rotation builder it wraps (Minecraft 26.1.2).
//
//   26.1.2/src/com/mojang/math/Axis.java:
//       static Axis of(final Vector3f vector) {
//           return angle -> new Quaternionf().rotationAxis(angle, vector);
//       }
//       Quaternionf rotation(float angle);                       // the SAM
//       default Quaternionf rotationDegrees(float angle) {
//           return this.rotation(angle * (float)(Math.PI / 180.0));
//       }
//
// The six axis-aligned constants (XN/XP/YN/YP/ZN/ZP) and their rotation/
// rotationDegrees are ALREADY certified in render/model/MathAxis.h via
// Quaternionf.rotationX/Y/Z. This gate covers the DISTINCT, ungated path:
// Axis.of(arbitrary vector) -> Quaternionf.rotationAxis(angle, x, y, z), which
// normalizes the axis with org.joml.Math.invsqrt (a code path that the X/Y/Z
// builders never touch).
//
// From joml-1.10.8 bytecode (javap -p -c org.joml.Quaternionf):
//
//   public Quaternionf rotationAxis(float angle, Vector3fc axis):
//       return rotationAxis(angle, axis.x(), axis.y(), axis.z());
//
//   public Quaternionf rotationAxis(float angle, float x, float y, float z):
//       float hangle    = angle / 2.0f;                  // fdiv by 2.0f (NOT *0.5f)
//       float sinAngle  = Math.sin(hangle);              // (float)Math.sin((double)h)
//       float invVLength = Math.invsqrt(x*x + y*y + z*z); // plain fmul/fadd, NOT fma
//       return set(x * invVLength * sinAngle,
//                  y * invVLength * sinAngle,
//                  z * invVLength * sinAngle,
//                  Math.cosFromSin(sinAngle, hangle));
//
// where (with org.joml.Options.FASTMATH=false, the default):
//   * org.joml.Math.sin(float x)      = (float) java.lang.Math.sin((double) x)
//   * org.joml.Math.invsqrt(float x)  = 1.0f / (float) java.lang.Math.sqrt((double) x)
//   * org.joml.Math.cosFromSin(s, a)  = cosFromSinInternal(s, a) (see Joml.h)
// and Quaternionf.set(x,y,z,w) is plain field assignment.
//
// These are EXACTLY the jsin / jinvsqrt / cosFromSin / Quaternionf helpers
// already certified in render/model/Joml.h, reused verbatim. Because sin/sqrt go
// through libm, results carry libm's ≤1-ULP behavior — the bit-exact match holds
// for the same reason the certified Joml.h jsin/cosFromSin rows hold.
//
// 1:1 TRAPS honored:
//   * hangle uses `angle / 2.0f` (the bytecode's fdiv), not `angle * 0.5f`.
//   * The axis length-squared is `x*x + y*y + z*z` evaluated with plain
//     fmul/fadd left-to-right — NOT org.joml.Math.fma — per the bytecode.
//   * DEG_TO_RAD is the float narrowing of the double Math.PI/180.0 == 0x3c8efa35,
//     applied as a Java float multiply (compile-time constant) before rotation().
//
// NO deviation from the source/bytecode is permitted in this file.

#pragma once

#include "render/model/Joml.h"

namespace mc::render::model::math_axis_arbitrary {

using ::mc::render::model::joml::Quaternionf;
using ::mc::render::model::joml::Vector3f;
using ::mc::render::model::joml::jsin;
using ::mc::render::model::joml::jinvsqrt;
using ::mc::render::model::joml::cosFromSin;

// (float)(Math.PI / 180.0): the double 0.017453292519943295 narrowed to float.
// == 0x3c8efa35. Identical to MathAxis.h's DEG_TO_RAD.
inline constexpr float DEG_TO_RAD = 0.017453292f;

// org.joml.Quaternionf.rotationAxis(float angle, float x, float y, float z)
// on a fresh (identity) Quaternionf. The builder overwrites every field via set,
// so the prior identity contents are irrelevant.
inline Quaternionf rotationAxis(float angle, float x, float y, float z) {
    float hangle = angle / 2.0f;
    float sinAngle = jsin(hangle);
    // Length-squared: plain fmul/fadd, left-associated, NOT fma (per bytecode).
    float invVLength = jinvsqrt(x * x + y * y + z * z);
    Quaternionf q;
    q.set(x * invVLength * sinAngle,
          y * invVLength * sinAngle,
          z * invVLength * sinAngle,
          cosFromSin(sinAngle, hangle));
    return q;
}

// org.joml.Quaternionf.rotationAxis(float angle, Vector3fc axis)
inline Quaternionf rotationAxis(float angle, const Vector3f& axis) {
    return rotationAxis(angle, axis.x, axis.y, axis.z);
}

// com.mojang.math.Axis.of(vector).rotation(angle):
//     angle -> new Quaternionf().rotationAxis(angle, vector)
inline Quaternionf of_rotation(const Vector3f& axis, float angle) {
    return rotationAxis(angle, axis);
}

// com.mojang.math.Axis.of(vector).rotationDegrees(angle):
//     default rotationDegrees(a) = rotation(a * (float)(Math.PI/180.0))
inline Quaternionf of_rotationDegrees(const Vector3f& axis, float angle) {
    return of_rotation(axis, angle * DEG_TO_RAD);
}

} // namespace mc::render::model::math_axis_arbitrary

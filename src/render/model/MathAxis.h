// 1:1 port of com.mojang.math.Axis (Minecraft 26.1.2,
// 26.1.2/src/com/mojang/math/Axis.java).
//
// Axis is a @FunctionalInterface with six constant providers:
//     XN = angle -> new Quaternionf().rotationX(-angle);
//     XP = angle -> new Quaternionf().rotationX( angle);
//     YN = angle -> new Quaternionf().rotationY(-angle);
//     YP = angle -> new Quaternionf().rotationY( angle);
//     ZN = angle -> new Quaternionf().rotationZ(-angle);
//     ZP = angle -> new Quaternionf().rotationZ( angle);
// plus
//     Quaternionf rotation(float angle);                       // the SAM
//     default Quaternionf rotationDegrees(float angle) {
//         return this.rotation(angle * (float)(Math.PI / 180.0));
//     }
//
// The quaternion is built by org.joml.Quaternionf.rotationX/Y/Z. From the
// joml-1.10.8 bytecode (javap -c org.joml.Quaternionf):
//     rotationX(a): s = Math.sin(a*0.5f); c = Math.cosFromSin(s, a*0.5f);
//                   set(s, 0, 0, c);   // (x,y,z,w)
//     rotationY(a): ... set(0, s, 0, c);
//     rotationZ(a): ... set(0, 0, s, c);
// where org.joml.Math.sin (Options.FASTMATH=false, the default) is
//     (float) java.lang.Math.sin((double) x)
// and Math.cosFromSin goes through cosFromSinInternal (FASTMATH off).
//
// These are EXACTLY the jsin / cosFromSin helpers already certified in
// render/model/Joml.h, so we reuse them verbatim. The Quaternionf produced is
// a fresh value (the lambda calls `new Quaternionf()` then a rotationX/Y/Z
// builder, so any prior identity contents are overwritten — the builder is a
// straight `set`, not a multiply-into-current like rotateX/Y/Z).
//
// NOTE: rotationX/Y/Z use java.lang.Math.sin (libm), NOT the Mth sin TABLE, so
// results are subject to libm's 1-ULP tolerance — see notes in the parity gate.
//
// rotationDegrees multiplies by the float constant (float)(Math.PI / 180.0),
// computed at compile time in Java as the double Math.PI/180.0 then narrowed to
// float == 0.017453292f (0x3c8efa35). We reproduce that exact constant below.
//
// NO deviation from the source/bytecode is permitted in this file.
#pragma once

#include <cmath>
#include <cstdint>

#include "render/model/Joml.h"

namespace mc::render::model::math_axis {

using ::mc::render::model::joml::Quaternionf;
using ::mc::render::model::joml::jsin;
using ::mc::render::model::joml::cosFromSin;

// (float)(Math.PI / 180.0): Math.PI/180.0 is the double 0.017453292519943295,
// narrowed to float. == 0x3c8efa35.
inline constexpr float DEG_TO_RAD = 0.017453292f;

// new Quaternionf().rotationX(angle) — set(s, 0, 0, c)
inline Quaternionf rotationX(float angle) {
    float s = jsin(angle * 0.5f);
    float c = cosFromSin(s, angle * 0.5f);
    Quaternionf q;
    q.set(s, 0.0f, 0.0f, c);
    return q;
}

// new Quaternionf().rotationY(angle) — set(0, s, 0, c)
inline Quaternionf rotationY(float angle) {
    float s = jsin(angle * 0.5f);
    float c = cosFromSin(s, angle * 0.5f);
    Quaternionf q;
    q.set(0.0f, s, 0.0f, c);
    return q;
}

// new Quaternionf().rotationZ(angle) — set(0, 0, s, c)
inline Quaternionf rotationZ(float angle) {
    float s = jsin(angle * 0.5f);
    float c = cosFromSin(s, angle * 0.5f);
    Quaternionf q;
    q.set(0.0f, 0.0f, s, c);
    return q;
}

// The six Axis constants, expressed as rotation(angle) functions.
inline Quaternionf XN_rotation(float angle) { return rotationX(-angle); }
inline Quaternionf XP_rotation(float angle) { return rotationX(angle); }
inline Quaternionf YN_rotation(float angle) { return rotationY(-angle); }
inline Quaternionf YP_rotation(float angle) { return rotationY(angle); }
inline Quaternionf ZN_rotation(float angle) { return rotationZ(-angle); }
inline Quaternionf ZP_rotation(float angle) { return rotationZ(angle); }

// default rotationDegrees(angle) = rotation(angle * (float)(Math.PI/180.0))
inline Quaternionf XN_rotationDegrees(float angle) { return XN_rotation(angle * DEG_TO_RAD); }
inline Quaternionf XP_rotationDegrees(float angle) { return XP_rotation(angle * DEG_TO_RAD); }
inline Quaternionf YN_rotationDegrees(float angle) { return YN_rotation(angle * DEG_TO_RAD); }
inline Quaternionf YP_rotationDegrees(float angle) { return YP_rotation(angle * DEG_TO_RAD); }
inline Quaternionf ZN_rotationDegrees(float angle) { return ZN_rotation(angle * DEG_TO_RAD); }
inline Quaternionf ZP_rotationDegrees(float angle) { return ZP_rotation(angle * DEG_TO_RAD); }

} // namespace mc::render::model::math_axis

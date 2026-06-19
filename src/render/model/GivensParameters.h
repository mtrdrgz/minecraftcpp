// 1:1 port of com.mojang.math.GivensParameters (Minecraft 26.1.2).
//
// A Givens rotation expressed by the (sin, cos) of HALF the rotation angle —
// the half-angle form a quaternion uses. Mojang's MatrixUtil SVD/QR machinery
// builds these from raw (sinHalf, cosHalf) pairs (fromUnnormalized) or from a
// positive angle (fromPositiveAngle), then applies them to a Quaternionf or a
// Matrix3f about the X/Y/Z axis. Pure float math; NO GL/GPU/window calls.
//
// Source (com/mojang/math/GivensParameters.java):
//   record GivensParameters(float sinHalf, float cosHalf)
//   fromUnnormalized(s,c): w = org.joml.Math.invsqrt(s*s + c*c); (w*s, w*c)
//   fromPositiveAngle(a):  sin = Math.sin(a/2); cos = Math.cosFromSin(sin, a/2)
//   inverse():             (-sinHalf, cosHalf)
//   cos():                 cosHalf*cosHalf - sinHalf*sinHalf   (full-angle cos)
//   sin():                 2.0F*sinHalf*cosHalf                (full-angle sin)
//   aroundX/Y/Z(Quaternionf): input.set(...) — half-angle quaternion
//   aroundX/Y/Z(Matrix3f):    full-angle rotation block via cos()/sin()
//
// 1:1 bit-exactness rules (all certified by the existing Joml.h gates):
//   * org.joml.Math.invsqrt(float) bytecode is fconst_1 / f2d / Math.sqrt / d2f /
//     fdiv  ==  1.0f / (float)sqrt((double)x).  -> joml::jinvsqrt.
//   * org.joml.Math.sin(float)      (FASTMATH off) == (float)sin((double)x).
//     -> joml::jsin.
//   * org.joml.Math.cosFromSin(s,a) (FASTMATH off) == cosFromSinInternal(s,a).
//     -> joml::cosFromSin.
//   * cos()/sin() and fromUnnormalized's w*s, w*c are PLAIN float mul/sub/add
//     (no fma); the project builds with -ffp-contract=off so the compiler can
//     not fuse them. Matches the Java fmul/fsub bytecode exactly.
//   * Quaternionf.set / Matrix3f field writes mirror org.joml semantics in Joml.h.
//
// No deviation from the source is permitted in this file.
#pragma once

#include "Joml.h"

namespace mc::render::model {

struct GivensParameters {
    float sinHalf;
    float cosHalf;

    GivensParameters(float sinHalf_, float cosHalf_) : sinHalf(sinHalf_), cosHalf(cosHalf_) {}

    // GivensParameters.fromUnnormalized(sinHalf, cosHalf)
    //   float w = Math.invsqrt(sinHalf*sinHalf + cosHalf*cosHalf);
    //   return new GivensParameters(w*sinHalf, w*cosHalf);
    static GivensParameters fromUnnormalized(float sinHalf, float cosHalf) {
        float w = joml::jinvsqrt(sinHalf * sinHalf + cosHalf * cosHalf);
        return GivensParameters(w * sinHalf, w * cosHalf);
    }

    // GivensParameters.fromPositiveAngle(angle)
    //   float sin = Math.sin(angle / 2.0F);
    //   float cos = Math.cosFromSin(sin, angle / 2.0F);
    //   return new GivensParameters(sin, cos);
    static GivensParameters fromPositiveAngle(float angle) {
        float sin = joml::jsin(angle / 2.0f);
        float cos = joml::cosFromSin(sin, angle / 2.0f);
        return GivensParameters(sin, cos);
    }

    // GivensParameters.inverse(): new GivensParameters(-sinHalf, cosHalf)
    GivensParameters inverse() const { return GivensParameters(-sinHalf, cosHalf); }

    // GivensParameters.cos(): cosHalf*cosHalf - sinHalf*sinHalf  (cos of the FULL angle)
    float cos() const { return cosHalf * cosHalf - sinHalf * sinHalf; }

    // GivensParameters.sin(): 2.0F * sinHalf * cosHalf  (sin of the FULL angle)
    float sin() const { return 2.0f * sinHalf * cosHalf; }

    // GivensParameters.aroundX(Quaternionf): input.set(sinHalf, 0, 0, cosHalf)
    joml::Quaternionf& aroundX(joml::Quaternionf& input) const {
        return input.set(sinHalf, 0.0f, 0.0f, cosHalf);
    }
    // GivensParameters.aroundY(Quaternionf): input.set(0, sinHalf, 0, cosHalf)
    joml::Quaternionf& aroundY(joml::Quaternionf& input) const {
        return input.set(0.0f, sinHalf, 0.0f, cosHalf);
    }
    // GivensParameters.aroundZ(Quaternionf): input.set(0, 0, sinHalf, cosHalf)
    joml::Quaternionf& aroundZ(joml::Quaternionf& input) const {
        return input.set(0.0f, 0.0f, sinHalf, cosHalf);
    }

    // GivensParameters.aroundX(Matrix3f): full-angle X rotation block.
    // Field write order mirrors the Java source.
    joml::Matrix3f& aroundX(joml::Matrix3f& input) const {
        input.m01 = 0.0f;
        input.m02 = 0.0f;
        input.m10 = 0.0f;
        input.m20 = 0.0f;
        float c = cos();
        float s = sin();
        input.m11 = c;
        input.m22 = c;
        input.m12 = s;
        input.m21 = -s;
        input.m00 = 1.0f;
        return input;
    }

    // GivensParameters.aroundY(Matrix3f): full-angle Y rotation block.
    joml::Matrix3f& aroundY(joml::Matrix3f& input) const {
        input.m01 = 0.0f;
        input.m10 = 0.0f;
        input.m12 = 0.0f;
        input.m21 = 0.0f;
        float c = cos();
        float s = sin();
        input.m00 = c;
        input.m22 = c;
        input.m02 = -s;
        input.m20 = s;
        input.m11 = 1.0f;
        return input;
    }

    // GivensParameters.aroundZ(Matrix3f): full-angle Z rotation block.
    joml::Matrix3f& aroundZ(joml::Matrix3f& input) const {
        input.m02 = 0.0f;
        input.m12 = 0.0f;
        input.m20 = 0.0f;
        input.m21 = 0.0f;
        float c = cos();
        float s = sin();
        input.m00 = c;
        input.m11 = c;
        input.m01 = s;
        input.m10 = -s;
        input.m22 = 1.0f;
        return input;
    }
};

} // namespace mc::render::model

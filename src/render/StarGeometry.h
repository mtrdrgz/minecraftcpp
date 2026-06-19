// 1:1 port of the star-field geometry generation in
// net.minecraft.client.renderer.SkyRenderer.buildStars() (Minecraft 26.1.2).
//
// buildStars() is a PURE, GL-FREE computation: it seeds a thread-local
// Xoroshiro RandomSource with the fixed seed 10842L and, for each of 1500
// candidate stars, emits four quad-corner positions computed entirely from
// org.joml Vector3f/Matrix3f math (normalize / rotateTowards / rotateZ / mul).
// The only "GL" touch in the original method is handing the finished vertex
// bytes to a GpuBuffer — the position math that decides where every star sits
// is the part ported here, and it is the part that determines the rendered
// night sky bit-for-bit.
//
// Bit-exactness rules (verified against the shipped joml-1.10.8.jar bytecode):
//  - org.joml.Math.fma(float,float,float): Runtime.HAS_Math_fma =
//    Options.USE_MATH_FMA (default FALSE) && hasMathFma(), so the dispatch
//    takes the plain "a*b+c" branch — NOT java.lang.Math.fma. We reuse
//    joml::jfma which is exactly a*b+c.
//  - org.joml.Math.sin(float)   = (float) java.lang.Math.sin((double) x)
//    (Options.FASTMATH default FALSE) -> joml::jsin.
//  - org.joml.Math.sqrt(float)  = (float) java.lang.Math.sqrt((double) x)
//    -> joml::jsqrt; invsqrt(float) = 1.0f / sqrt(float) -> joml::jinvsqrt.
//  - org.joml.Math.cosFromSin   -> cosFromSinInternal (FASTMATH off)
//    -> joml::cosFromSin.
//  - Matrix3f is column-major (mCR = column C, row R), matching joml and the
//    repo's render/model/Joml.h. new Matrix3f() starts as the identity.
//  - The build is compiled with -ffp-contract=off so the compiler cannot fuse
//    the plain "*" then "+" sequences behind our back.
//
// NO deviation from the bytecode is permitted in this file.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "render/model/Joml.h"
#include "world/level/levelgen/RandomSource.h"
#include "world/level/levelgen/Mth.h"

namespace mc::render::star {

using mc::render::model::joml::Vector3f;
using mc::render::model::joml::Matrix3f;
using mc::render::model::joml::jfma;
using mc::render::model::joml::jsin;
using mc::render::model::joml::jsqrt;
using mc::render::model::joml::jinvsqrt;
using mc::render::model::joml::cosFromSin;

// org.joml.Vector3f.negate() -> negate(this): x=-x; y=-y; z=-z.
inline Vector3f negate(Vector3f v) {
    v.x = -v.x;
    v.y = -v.y;
    v.z = -v.z;
    return v;
}

// org.joml.Vector3f.normalize(float length) -> normalize(length, this):
//   scalar = Math.invsqrt(Math.fma(x,x, Math.fma(y,y, z*z))) * length;
//   x*=scalar; y*=scalar; z*=scalar.
inline Vector3f normalize(Vector3f v, float length) {
    float scalar = jinvsqrt(jfma(v.x, v.x, jfma(v.y, v.y, v.z * v.z))) * length;
    v.x = v.x * scalar;
    v.y = v.y * scalar;
    v.z = v.z * scalar;
    return v;
}

// org.joml.Vector3f.mul(Matrix3fc m) -> mul(m, this):
//   vx=x; vy=y; vz=z;
//   x = fma(m00, vx, fma(m10, vy, m20*vz));
//   y = fma(m01, vx, fma(m11, vy, m21*vz));
//   z = fma(m02, vx, fma(m12, vy, m22*vz));
inline Vector3f mulMat3(Vector3f v, const Matrix3f& m) {
    float vx = v.x, vy = v.y, vz = v.z;
    v.x = jfma(m.m00, vx, jfma(m.m10, vy, m.m20 * vz));
    v.y = jfma(m.m01, vx, jfma(m.m11, vy, m.m21 * vz));
    v.z = jfma(m.m02, vx, jfma(m.m12, vy, m.m22 * vz));
    return v;
}

// org.joml.Matrix3f.rotateTowards(Vector3fc dir, Vector3fc up)
//   -> rotateTowards(dirX,dirY,dirZ, upX,upY,upZ, this).
// All plain fmul/fadd/fsub in the bytecode (no Math.fma in this method).
inline Matrix3f rotateTowards(Matrix3f t, const Vector3f& dir, const Vector3f& up) {
    float dirX = dir.x, dirY = dir.y, dirZ = dir.z;
    float upX = up.x, upY = up.y, upZ = up.z;

    // normalize direction
    float invDirLength = jinvsqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    float ndirX = dirX * invDirLength;
    float ndirY = dirY * invDirLength;
    float ndirZ = dirZ * invDirLength;

    // left = up x ndir
    float leftX = upY * ndirZ - upZ * ndirY;
    float leftY = upZ * ndirX - upX * ndirZ;
    float leftZ = upX * ndirY - upY * ndirX;
    // normalize left
    float invLeftLength = jinvsqrt(leftX * leftX + leftY * leftY + leftZ * leftZ);
    leftX = leftX * invLeftLength;
    leftY = leftY * invLeftLength;
    leftZ = leftZ * invLeftLength;

    // up' = ndir x left
    float upnX = ndirY * leftZ - ndirZ * leftY;
    float upnY = ndirZ * leftX - ndirX * leftZ;
    float upnZ = ndirX * leftY - ndirY * leftX;

    // rotation basis (column-major rn)
    float rn00 = leftX, rn01 = leftY, rn02 = leftZ;
    float rn10 = upnX,  rn11 = upnY,  rn12 = upnZ;
    float rn20 = ndirX, rn21 = ndirY, rn22 = ndirZ;

    // dest = this * RN  (plain mul/add)
    float nm00 = t.m00 * rn00 + t.m10 * rn01 + t.m20 * rn02;
    float nm01 = t.m01 * rn00 + t.m11 * rn01 + t.m21 * rn02;
    float nm02 = t.m02 * rn00 + t.m12 * rn01 + t.m22 * rn02;
    float nm10 = t.m00 * rn10 + t.m10 * rn11 + t.m20 * rn12;
    float nm11 = t.m01 * rn10 + t.m11 * rn11 + t.m21 * rn12;
    float nm12 = t.m02 * rn10 + t.m12 * rn11 + t.m22 * rn12;
    t.m20 = t.m00 * rn20 + t.m10 * rn21 + t.m20 * rn22; // written first (matches bytecode)
    t.m21 = t.m01 * rn20 + t.m11 * rn21 + t.m21 * rn22;
    t.m22 = t.m02 * rn20 + t.m12 * rn21 + t.m22 * rn22;
    t.m00 = nm00; t.m01 = nm01; t.m02 = nm02;
    t.m10 = nm10; t.m11 = nm11; t.m12 = nm12;
    return t;
}

// org.joml.Matrix3f.rotateZ(float ang) -> rotateZ(ang, this).
//   sin = Math.sin(ang); cos = Math.cosFromSin(sin, ang);
//   lm00=cos; lm01=sin; lm10=-sin; lm11=cos;  (plain mul/add)
inline Matrix3f rotateZ(Matrix3f t, float ang) {
    float sin = jsin(ang);
    float cos = cosFromSin(sin, ang);
    float lm00 = cos;
    float lm01 = sin;
    float lm10 = -sin;
    float lm11 = cos;
    float nm00 = t.m00 * lm00 + t.m10 * lm01;
    float nm01 = t.m01 * lm00 + t.m11 * lm01;
    float nm02 = t.m02 * lm00 + t.m12 * lm01;
    t.m10 = t.m00 * lm10 + t.m10 * lm11;
    t.m11 = t.m01 * lm10 + t.m11 * lm11;
    t.m12 = t.m02 * lm10 + t.m12 * lm11;
    t.m00 = nm00; t.m01 = nm01; t.m02 = nm02;
    // m20/m21/m22 copied through unchanged
    return t;
}

// One emitted star vertex (DefaultVertexFormat.POSITION: three floats).
struct StarVertex {
    float x, y, z;
};

// Faithful re-drive of SkyRenderer.buildStars() position math. Returns the
// vertex stream in the exact emission order (4 vertices per accepted star,
// in the order the QUADS BufferBuilder received them).
inline std::vector<StarVertex> buildStars() {
    std::shared_ptr<mc::levelgen::RandomSource> random =
        mc::levelgen::RandomSource::createThreadLocalInstance(10842LL);

    std::vector<StarVertex> out;

    for (int i = 0; i < 1500; ++i) {
        float x = random->nextFloat() * 2.0f - 1.0f;
        float y = random->nextFloat() * 2.0f - 1.0f;
        float z = random->nextFloat() * 2.0f - 1.0f;
        float starSize = 0.15f + random->nextFloat() * 0.1f;
        float lengthSq = mc::levelgen::mth::lengthSquared(x, y, z);
        if (!(lengthSq <= 0.010000001f) && !(lengthSq >= 1.0f)) {
            Vector3f starCenter = normalize(Vector3f(x, y, z), 100.0f);
            // (float)(random.nextDouble() * (float)Math.PI * 2.0)
            float zRot = (float)(random->nextDouble() * 3.1415927f * 2.0);
            Matrix3f rotation = rotateZ(
                rotateTowards(Matrix3f(), negate(starCenter), Vector3f(0.0f, 1.0f, 0.0f)),
                -zRot);

            Vector3f v0 = mulMat3(Vector3f(starSize, -starSize, 0.0f), rotation);
            v0.add(starCenter);
            Vector3f v1 = mulMat3(Vector3f(starSize, starSize, 0.0f), rotation);
            v1.add(starCenter);
            Vector3f v2 = mulMat3(Vector3f(-starSize, starSize, 0.0f), rotation);
            v2.add(starCenter);
            Vector3f v3 = mulMat3(Vector3f(-starSize, -starSize, 0.0f), rotation);
            v3.add(starCenter);

            out.push_back({v0.x, v0.y, v0.z});
            out.push_back({v1.x, v1.y, v1.z});
            out.push_back({v2.x, v2.y, v2.z});
            out.push_back({v3.x, v3.y, v3.z});
        }
    }

    return out;
}

}  // namespace mc::render::star

#pragma once

// 1:1 port of the PURE, GL-free interpolation math behind
// net.minecraft.client.animation.AnimationChannel.Interpolations (Minecraft
// 26.1.2) — the per-channel keyframe interpolators used to drive entity model
// animations (AnimationChannel.Targets POSITION/ROTATION/SCALE).
//
//   * Interpolations.LINEAR   — AnimationChannel.java:14-18
//   * Interpolations.CATMULLROM — AnimationChannel.java:19-30
//
// These are pure functions of (alpha, keyframes[], prev, next, targetScale) and a
// destination Vector3f; they touch no GL/GPU/window/registry state. The only
// dependencies are org.joml.Vector3f (lerp/mul) and net.minecraft.util.Mth
// (catmullrom), both already certified in this repo.
//
// Source classes / formulae reproduced VERBATIM:
//
//   LINEAR (AnimationChannel.java:14-18):
//     Vector3fc point0 = keyframes[prev].postTarget();
//     Vector3fc point1 = keyframes[next].preTarget();
//     return point0.lerp(point1, alpha, vector).mul(targetScale);
//
//   CATMULLROM (AnimationChannel.java:19-30):
//     Vector3fc point0 = keyframes[Math.max(0, prev - 1)].postTarget();
//     Vector3fc point1 = keyframes[prev].postTarget();
//     Vector3fc point2 = keyframes[next].postTarget();
//     Vector3fc point3 = keyframes[Math.min(keyframes.length - 1, next + 1)].postTarget();
//     vector.set(Mth.catmullrom(alpha, p0.x, p1.x, p2.x, p3.x) * targetScale, ...y, ...z);
//     return vector;
//
// ── 1:1 BIT-EXACT TRAPS (verified against the shipped joml-1.10.8.jar bytecode) ──
//
//   * Vector3f.lerp(Vector3fc other, float t, Vector3f dest) — disassembled:
//       dest.x = org.joml.Math.fma(other.x() - this.x, t, this.x);   (+ y, + z)
//     org.joml.Math.fma(float,float,float) dispatches on org.joml.Runtime.HAS_Math_fma.
//     EMPIRICALLY, in the JVM the ground truth runs under, that flag resolves to
//     FALSE, so org.joml.Math.fma takes the plain two-rounding path: a*b + c (verified
//     against the real jar: org.joml.Math.fma(-3, 0.9f, 3) == (-3*0.9f + 3), which is
//     bit-DISTINCT from java.lang.Math.fma's single-rounding result). So lerp MUST use
//     plain a*b+c, NOT std::fmaf — and the C++ build MUST compile with -ffp-contract=off
//     (the project default, see mcpp/cmake/CompilerFlags.cmake) so the compiler does
//     not silently fuse `a*b+c` into a hardware FMA, which would reintroduce the 1-ULP
//     bug. `this` is point0, `other` is point1, so the per-component value is
//       (point1.c - point0.c) * alpha + point0.c   (no fusion).
//
//   * Vector3f.mul(float) -> mul(scalar, this): plain component-wise fmul
//       this.x = this.x * scalar;  (disassembled getfield/fload/fmul/putfield)
//     no FMA, so plain `c * scale`. Applied to the lerp RESULT, in place.
//
//   * Mth.catmullrom(alpha, p0,p1,p2,p3) (Mth.java:564-572): reused VERBATIM from
//     the certified mc::levelgen::mth header (all-float Horner-free polynomial with
//     the exact 0.5F/2.0F/5.0F/4.0F/3.0F literal ordering). The CATMULLROM result
//     per component is catmullrom(...) * targetScale (plain float multiply).
//
//   * Index clamps use the integer Math.max/Math.min exactly:
//       max(0, prev-1)  and  min(numKeyframes-1, next+1).
//
// Nothing here is unported: both interpolators are fully translated. (The
// AnimationChannel record, its Target functional interface and KeyframeAnimations
// applier loop are separate classes and out of scope for THIS gate.)
//
// Source: 26.1.2/src/net/minecraft/client/animation/AnimationChannel.java
//         26.1.2/src/net/minecraft/util/Mth.java (catmullrom)
//         joml-1.10.8.jar org.joml.Vector3f.{lerp,mul}, org.joml.Math.fma

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "world/level/levelgen/Mth.h"

namespace mc::client::animation {

namespace mth = mc::levelgen::mth;

// Minimal Vector3f value type (x,y,z) matching org.joml.Vector3f component layout.
struct Vec3f {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

// net.minecraft.client.animation.Keyframe (record) — only the two targets matter
// for interpolation (timestamp/interpolation are used by the OUTER applier, not by
// the interpolator lambdas themselves). The single-arg Keyframe ctor sets
// preTarget == postTarget; callers building these mirror that.
struct Keyframe {
    Vec3f preTarget;
    Vec3f postTarget;
};

// org.joml.Math.fma(a,b,c) under the GT's JVM (HAS_Math_fma=false): plain a*b+c.
// Compiled -ffp-contract=off (project default) so the compiler cannot fuse it.
inline float jomlFma(float a, float b, float c) { return a * b + c; }

// org.joml.Vector3f.lerp(other, t, dest).mul(targetScale), with `this` = point0.
// dest.c = jomlFma(point1.c - point0.c, alpha, point0.c); then dest.c *= targetScale.
inline Vec3f linearLerpScaled(const Vec3f& point0, const Vec3f& point1, float alpha,
                              float targetScale) {
    Vec3f dest;
    dest.x = jomlFma(point1.x - point0.x, alpha, point0.x);
    dest.y = jomlFma(point1.y - point0.y, alpha, point0.y);
    dest.z = jomlFma(point1.z - point0.z, alpha, point0.z);
    // Vector3f.mul(float): plain component-wise multiply in place.
    dest.x = dest.x * targetScale;
    dest.y = dest.y * targetScale;
    dest.z = dest.z * targetScale;
    return dest;
}

// AnimationChannel.Interpolations.LINEAR — AnimationChannel.java:14-18.
//   point0 = keyframes[prev].postTarget(); point1 = keyframes[next].preTarget();
//   return point0.lerp(point1, alpha, vector).mul(targetScale);
inline Vec3f linear(const std::vector<Keyframe>& keyframes, float alpha, int prev,
                    int next, float targetScale) {
    const Vec3f& point0 = keyframes[static_cast<std::size_t>(prev)].postTarget;
    const Vec3f& point1 = keyframes[static_cast<std::size_t>(next)].preTarget;
    return linearLerpScaled(point0, point1, alpha, targetScale);
}

// AnimationChannel.Interpolations.CATMULLROM — AnimationChannel.java:19-30.
inline Vec3f catmullrom(const std::vector<Keyframe>& keyframes, float alpha, int prev,
                        int next, float targetScale) {
    const int n = static_cast<int>(keyframes.size());
    const Vec3f& point0 = keyframes[static_cast<std::size_t>(std::max(0, prev - 1))].postTarget;
    const Vec3f& point1 = keyframes[static_cast<std::size_t>(prev)].postTarget;
    const Vec3f& point2 = keyframes[static_cast<std::size_t>(next)].postTarget;
    const Vec3f& point3 = keyframes[static_cast<std::size_t>(std::min(n - 1, next + 1))].postTarget;
    Vec3f v;
    v.x = mth::catmullrom(alpha, point0.x, point1.x, point2.x, point3.x) * targetScale;
    v.y = mth::catmullrom(alpha, point0.y, point1.y, point2.y, point3.y) * targetScale;
    v.z = mth::catmullrom(alpha, point0.z, point1.z, point2.z, point3.z) * targetScale;
    return v;
}

}  // namespace mc::client::animation

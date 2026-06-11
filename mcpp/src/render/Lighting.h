// 1:1 port of the PURE rendering math in com.mojang.blaze3d.platform.Lighting
// (Minecraft 26.1.2) — the directional-diffuse light setup. Everything here is
// deterministic JOML float math: six normalized constant light directions plus
// the pose-matrix transforms that derive the per-Entry light directions written
// to the lighting UBO. NO GL / GPU / window state is touched (the real class'
// GpuBuffer/RenderSystem plumbing is intentionally NOT ported — only the math
// that decides where the lights point, which is all that affects output pixels).
//
// Source: com/mojang/blaze3d/platform/Lighting.java
//   static {
//     DIFFUSE_LIGHT_0           = new Vector3f( 0.2,  1.0, -0.7).normalize();
//     DIFFUSE_LIGHT_1           = new Vector3f(-0.2,  1.0,  0.7).normalize();
//     NETHER_DIFFUSE_LIGHT_0    = new Vector3f( 0.2,  1.0, -0.7).normalize();
//     NETHER_DIFFUSE_LIGHT_1    = new Vector3f(-0.2, -1.0,  0.7).normalize();
//     INVENTORY_DIFFUSE_LIGHT_0 = new Vector3f( 0.2, -1.0,  1.0).normalize();
//     INVENTORY_DIFFUSE_LIGHT_1 = new Vector3f(-0.2, -1.0,  0.0).normalize();
//     UBO_SIZE = new Std140SizeCalculator().putVec3().putVec3().get();
//   }
//   ctor:
//     flatPose   = new Matrix4f().rotationY((float)(-PI/8)).rotateX((float)(3*PI/4));
//     item3DPose = new Matrix4f().scaling(1,-1,1)
//                                .rotateYXZ(1.0821041f, 3.2375858f, 0.0f)
//                                .rotateYXZ((float)(-PI/8),(float)(3*PI/4),0.0f);
//     ITEMS_FLAT   <- flatPose.transformDirection(DIFFUSE_LIGHT_{0,1})
//     ITEMS_3D     <- item3DPose.transformDirection(DIFFUSE_LIGHT_{0,1})
//     ENTITY_IN_UI <- INVENTORY_DIFFUSE_LIGHT_{0,1}            (identity pose)
//     PLAYER_SKIN  <- new Matrix4f().transformDirection(INVENTORY_DIFFUSE_LIGHT_{0,1})
//   updateLevel: LEVEL  <- DIFFUSE_LIGHT_{0,1}  (DEFAULT)  / NETHER_DIFFUSE_LIGHT_{0,1} (NETHER)
//
// 1:1 traps observed:
//  - org.joml Options.USE_MATH_FMA defaults to false -> Math.fma is plain a*b+c
//    (jfma), NOT a fused std::fmaf. Build is -ffp-contract=off so the compiler
//    cannot re-fuse. rotateX/rotateYXZ below use jfma exactly where JOML does.
//  - rotationY/cosFromSin go through (float)Math.sin((double)x); matched by jsin.
//  - the angle constants are DOUBLE expressions narrowed to float: e.g.
//    (float)(-Math.PI/8.0), (float)(Math.PI*3.0/4.0). Computed in double here too.
//  - Std140 layout: putVec3() align(16) AND adds 16 (vec3 padded to 16 in std140),
//    so UBO_SIZE = 16 + 16 = 32 (NOT 12+12 — the size step is the padded 16).
//  - PLAYER_SKIN uses an *identity* Matrix4f.transformDirection; with USE_MATH_FMA
//    off the generic transform still routes through jfma, so the result is the
//    input bit-for-bit (multiply by 1/0 + add). It is kept explicit, not assumed.
#pragma once

#include "model/Joml.h"

#include <cmath>

namespace mc::render::lighting {

namespace j = mc::render::model::joml;

// java.lang.Math.PI — the exact double 3.141592653589793 (0x1.921fb54442d18p+1).
// The angle constants in Lighting are DOUBLE expressions (e.g. (float)(-PI/8))
// narrowed to float; we reproduce them in double first, then narrow, to match.
inline constexpr double MATH_PI = 0x1.921fb54442d18p+1;

// ── Matrix4f.rotateX(float) — JOML 1.10.8 rotateXInternal path ───────────────
// Lighting only ever calls this on a non-identity, non-pure-translation matrix
// (result of rotationY), so the fast-paths are not reachable; we still guard
// them to mirror the public method faithfully.
inline j::Matrix4f& mat_rotateX(j::Matrix4f& m, float ang) {
    if ((m.properties & j::Matrix4f::PROPERTY_IDENTITY) != 0) return m.rotationX(ang);
    // (no pure-translation case is reached in Lighting; rotationX().setTranslation
    //  would be required to handle it — intentionally omitted as unreachable.)
    float sin = j::jsin(ang);
    float cos = j::cosFromSin(sin, ang);
    float lm10 = m.m10, lm11 = m.m11, lm12 = m.m12, lm13 = m.m13;
    float lm20 = m.m20, lm21 = m.m21, lm22 = m.m22, lm23 = m.m23;
    float nm10 = j::jfma(lm10, cos, lm20 * sin);
    float nm11 = j::jfma(lm11, cos, lm21 * sin);
    float nm12 = j::jfma(lm12, cos, lm22 * sin);
    float nm13 = j::jfma(lm13, cos, lm23 * sin);
    m.m20 = j::jfma(lm10, -sin, lm20 * cos);
    m.m21 = j::jfma(lm11, -sin, lm21 * cos);
    m.m22 = j::jfma(lm12, -sin, lm22 * cos);
    m.m23 = j::jfma(lm13, -sin, lm23 * cos);
    m.m10 = nm10; m.m11 = nm11; m.m12 = nm12; m.m13 = nm13;
    // m00..m03, m30..m33 unchanged
    m.properties &= ~(j::Matrix4f::PROPERTY_PERSPECTIVE | j::Matrix4f::PROPERTY_IDENTITY
                      | j::Matrix4f::PROPERTY_TRANSLATION);
    return m;
}

// ── Matrix4f.rotateYXZ(angleY, angleX, angleZ) — JOML 1.10.8 (always generic) ─
inline j::Matrix4f& mat_rotateYXZ(j::Matrix4f& m, float angleY, float angleX, float angleZ) {
    float sinX = j::jsin(angleX); float cosX = j::cosFromSin(sinX, angleX);
    float sinY = j::jsin(angleY); float cosY = j::cosFromSin(sinY, angleY);
    float sinZ = j::jsin(angleZ); float cosZ = j::cosFromSin(sinZ, angleZ);
    float m_sinX = -sinX, m_sinY = -sinY, m_sinZ = -sinZ;
    // rotateY
    float nm20 = j::jfma(m.m00, sinY, m.m20 * cosY);
    float nm21 = j::jfma(m.m01, sinY, m.m21 * cosY);
    float nm22 = j::jfma(m.m02, sinY, m.m22 * cosY);
    float nm23 = j::jfma(m.m03, sinY, m.m23 * cosY);
    float nm00 = j::jfma(m.m00, cosY, m.m20 * m_sinY);
    float nm01 = j::jfma(m.m01, cosY, m.m21 * m_sinY);
    float nm02 = j::jfma(m.m02, cosY, m.m22 * m_sinY);
    float nm03 = j::jfma(m.m03, cosY, m.m23 * m_sinY);
    // rotateX
    float nm10 = j::jfma(m.m10, cosX, nm20 * sinX);
    float nm11 = j::jfma(m.m11, cosX, nm21 * sinX);
    float nm12 = j::jfma(m.m12, cosX, nm22 * sinX);
    float nm13 = j::jfma(m.m13, cosX, nm23 * sinX);
    m.m20 = j::jfma(m.m10, m_sinX, nm20 * cosX);
    m.m21 = j::jfma(m.m11, m_sinX, nm21 * cosX);
    m.m22 = j::jfma(m.m12, m_sinX, nm22 * cosX);
    m.m23 = j::jfma(m.m13, m_sinX, nm23 * cosX);
    // rotateZ
    m.m00 = j::jfma(nm00, cosZ, nm10 * sinZ);
    m.m01 = j::jfma(nm01, cosZ, nm11 * sinZ);
    m.m02 = j::jfma(nm02, cosZ, nm12 * sinZ);
    m.m03 = j::jfma(nm03, cosZ, nm13 * sinZ);
    m.m10 = j::jfma(nm00, m_sinZ, nm10 * cosZ);
    m.m11 = j::jfma(nm01, m_sinZ, nm11 * cosZ);
    m.m12 = j::jfma(nm02, m_sinZ, nm12 * cosZ);
    m.m13 = j::jfma(nm03, m_sinZ, nm13 * cosZ);
    // m30..m33 unchanged
    m.properties &= ~(j::Matrix4f::PROPERTY_PERSPECTIVE | j::Matrix4f::PROPERTY_IDENTITY
                      | j::Matrix4f::PROPERTY_TRANSLATION);
    return m;
}

// ── Lighting.UBO_SIZE — Std140: putVec3() aligns size to 16 then adds 16 ─────
// First putVec3: align(16) of 0 = 0, +16 -> 16. Second putVec3: align(16) of 16
// = 16, +16 -> 32. (vec3 occupies a padded 16 bytes in std140.)
inline int uboSize() { return 32; }

// The six raw, normalized diffuse-light constants (Vector3f.normalize()).
inline j::Vector3f diffuseLight0()          { j::Vector3f v(0.2f, 1.0f, -0.7f);  v.normalize(); return v; }
inline j::Vector3f diffuseLight1()          { j::Vector3f v(-0.2f, 1.0f, 0.7f);  v.normalize(); return v; }
inline j::Vector3f netherDiffuseLight0()    { j::Vector3f v(0.2f, 1.0f, -0.7f);  v.normalize(); return v; }
inline j::Vector3f netherDiffuseLight1()    { j::Vector3f v(-0.2f, -1.0f, 0.7f); v.normalize(); return v; }
inline j::Vector3f inventoryDiffuseLight0() { j::Vector3f v(0.2f, -1.0f, 1.0f);  v.normalize(); return v; }
inline j::Vector3f inventoryDiffuseLight1() { j::Vector3f v(-0.2f, -1.0f, 0.0f); v.normalize(); return v; }

// flatPose = rotationY((float)(-PI/8)) then rotateX((float)(3*PI/4)).
inline j::Matrix4f flatPose() {
    j::Matrix4f m;
    m.rotationY((float)(-MATH_PI / 8.0));
    mat_rotateX(m, (float)(MATH_PI * 3.0 / 4.0));
    return m;
}

// item3DPose = scaling(1,-1,1) then two rotateYXZ.
inline j::Matrix4f item3DPose() {
    j::Matrix4f m;
    m.scaling(1.0f, -1.0f, 1.0f);
    mat_rotateYXZ(m, 1.0821041f, 3.2375858f, 0.0f);
    mat_rotateYXZ(m, (float)(-MATH_PI / 8.0), (float)(MATH_PI * 3.0 / 4.0), 0.0f);
    return m;
}

// Per-Entry light directions written into the UBO by the constructor.
inline j::Vector3f itemsFlat0() { j::Vector3f d; flatPose().transformDirection(diffuseLight0(), d); return d; }
inline j::Vector3f itemsFlat1() { j::Vector3f d; flatPose().transformDirection(diffuseLight1(), d); return d; }
inline j::Vector3f items3d0()   { j::Vector3f d; item3DPose().transformDirection(diffuseLight0(), d); return d; }
inline j::Vector3f items3d1()   { j::Vector3f d; item3DPose().transformDirection(diffuseLight1(), d); return d; }
// ENTITY_IN_UI is written with the inventory constants directly (no transform).
inline j::Vector3f entityInUi0() { return inventoryDiffuseLight0(); }
inline j::Vector3f entityInUi1() { return inventoryDiffuseLight1(); }
// PLAYER_SKIN passes the inventory constants through an identity Matrix4f.
inline j::Vector3f playerSkin0() { j::Matrix4f m; j::Vector3f d; m.transformDirection(inventoryDiffuseLight0(), d); return d; }
inline j::Vector3f playerSkin1() { j::Matrix4f m; j::Vector3f d; m.transformDirection(inventoryDiffuseLight1(), d); return d; }
// updateLevel writes the raw level constants.
inline j::Vector3f levelDefault0() { return diffuseLight0(); }
inline j::Vector3f levelDefault1() { return diffuseLight1(); }
inline j::Vector3f levelNether0()  { return netherDiffuseLight0(); }
inline j::Vector3f levelNether1()  { return netherDiffuseLight1(); }

} // namespace mc::render::lighting

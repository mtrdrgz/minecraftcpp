// 1:1 port of the PURE billboard-quad geometry produced by
// net.minecraft.client.renderer.state.level.QuadParticleRenderState
// (Minecraft 26.1.2): renderRotatedQuad / renderVertex.
//
// This is the math that turns one extracted particle
//   (worldPos x,y,z; camera-facing Quaternionf xRot,yRot,zRot,wRot; scale;
//    u0,u1,v0,v1; packed color; light)
// into the four QUADS vertices the particle mesh is built from. Every visible
// particle in the game runs through this every frame.
//
// GL-free: in vanilla the four computed positions are pushed straight into a
// VertexConsumer. We expose the same four (position, uv, color, light) vertices
// as plain structs — the VertexConsumer is only a sink, never math.
//
// QuadParticleRenderState.renderRotatedQuad (QuadParticleRenderState.java:159-181):
//   Quaternionf rotation = new Quaternionf(xRot, yRot, zRot, wRot);
//   renderVertex(b, rotation, x,y,z,  1, -1, scale, u1, v1, color, light);
//   renderVertex(b, rotation, x,y,z,  1,  1, scale, u1, v0, color, light);
//   renderVertex(b, rotation, x,y,z, -1,  1, scale, u0, v0, color, light);
//   renderVertex(b, rotation, x,y,z, -1, -1, scale, u0, v1, color, light);
// renderVertex (QuadParticleRenderState.java:183-199):
//   Vector3f scratch = new Vector3f(nx, ny, 0).rotate(rotation).mul(scale).add(x,y,z);
//   builder.addVertex(scratch.x, scratch.y, scratch.z).setUv(u,v).setColor(color).setLight(light);
//
// 1:1 traps observed and reproduced here:
//  - org.joml.Vector3f.rotate(Quaternionfc) delegates to Quaternionf.transform,
//    whose float body (disassembled from joml-1.10.8.jar) uses org.joml.Math.fma.
//    With Options.USE_MATH_FMA=false (the default the ground truth runs under),
//    that fma is a PLAIN two-rounding a*b+c — NOT std::fmaf. We route it through
//    joml::jfma (plain a*b+c), and the build is -ffp-contract=off so the compiler
//    cannot fuse it behind our back. Using std::fmaf would be a 1-ULP bug.
//  - The transform normalises by k = 1/(xx+yy+zz+ww); for the camera-facing unit
//    quaternions this is ~1.0 but is applied exactly as a float reciprocal+mul.
//  - The new Vector3f(nx, ny, 0.0f) constructor stores nx/ny verbatim; corner
//    order and (nx,ny)->(u,v) pairing are fixed by the four calls above.
//  - mul(scale): component-wise float multiply; add(x,y,z): component-wise float
//    add — applied AFTER the rotation (rotate -> mul -> add), order is load-bearing.
//
// NO deviation from the Java/JOML bytecode is permitted in this file.
#pragma once

#include "render/model/Joml.h"

#include <array>

namespace mc::render::state::level {

namespace joml = mc::render::model::joml;

// org.joml.Vector3f.rotate(Quaternionfc) == q.transform(this, this).
// Transcribed from org.joml.Quaternionf.transform(float,float,float,Vector3f)
// in joml-1.10.8.jar (javap -c). Math.fma -> joml::jfma (plain a*b+c, the
// USE_MATH_FMA=false default). Writes the rotated vector back into v.
inline void vector3fRotate(joml::Vector3f& v, const joml::Quaternionf& q) {
    const float vx = v.x, vy = v.y, vz = v.z;
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float ww = q.w * q.w;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float xw = q.x * q.w;
    const float zw = q.z * q.w;
    const float yw = q.y * q.w;
    const float k = 1.0f / (xx + yy + zz + ww);
    const float rx = joml::jfma((xx - yy - zz + ww) * k, vx,
                       joml::jfma(2.0f * (xy - zw) * k, vy,
                                  2.0f * (xz + yw) * k * vz));
    const float ry = joml::jfma(2.0f * (xy + zw) * k, vx,
                       joml::jfma((yy - xx - zz + ww) * k, vy,
                                  2.0f * (yz - xw) * k * vz));
    const float rz = joml::jfma(2.0f * (xz - yw) * k, vx,
                       joml::jfma(2.0f * (yz + xw) * k, vy,
                                  (zz - xx - yy + ww) * k * vz));
    v.x = rx;
    v.y = ry;
    v.z = rz;
}

struct QuadVertex {
    float x, y, z;   // position
    float u, v;      // texture coords (passed through verbatim)
    int color;       // packed ARGB (passed through verbatim)
    int light;       // packed light coords (passed through verbatim)
};

// QuadParticleRenderState.renderVertex: Vector3f(nx,ny,0).rotate(q).mul(scale).add(x,y,z)
inline QuadVertex renderVertex(const joml::Quaternionf& rotation, float x, float y, float z,
                               float nx, float ny, float scale, float u, float v,
                               int color, int light) {
    joml::Vector3f scratch(nx, ny, 0.0f);
    vector3fRotate(scratch, rotation);
    // Vector3f.mul(float) is component*scalar; Vector3f.add(float,float,float) adds.
    scratch.x = scratch.x * scale;
    scratch.y = scratch.y * scale;
    scratch.z = scratch.z * scale;
    scratch.x = scratch.x + x;
    scratch.y = scratch.y + y;
    scratch.z = scratch.z + z;
    return QuadVertex{scratch.x, scratch.y, scratch.z, u, v, color, light};
}

// QuadParticleRenderState.renderRotatedQuad: builds the four billboard corners.
// rotation = new Quaternionf(xRot, yRot, zRot, wRot) (4-arg ctor stores verbatim).
inline std::array<QuadVertex, 4> renderRotatedQuad(
    float x, float y, float z,
    float xRot, float yRot, float zRot, float wRot,
    float scale, float u0, float u1, float v0, float v1,
    int color, int light) {
    joml::Quaternionf rotation;
    rotation.x = xRot;
    rotation.y = yRot;
    rotation.z = zRot;
    rotation.w = wRot;
    return {
        renderVertex(rotation, x, y, z,  1.0f, -1.0f, scale, u1, v1, color, light),
        renderVertex(rotation, x, y, z,  1.0f,  1.0f, scale, u1, v0, color, light),
        renderVertex(rotation, x, y, z, -1.0f,  1.0f, scale, u0, v0, color, light),
        renderVertex(rotation, x, y, z, -1.0f, -1.0f, scale, u0, v1, color, light),
    };
}

}  // namespace mc::render::state::level

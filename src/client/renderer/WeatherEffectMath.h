#pragma once

// 1:1 port of the PURE, deterministic, world-free geometry math from
// net.minecraft.client.renderer.WeatherEffectRenderer (Minecraft 26.1.2).
//
// Two pieces are ported here, both pure float/double arithmetic with no GPU,
// no Camera/Level, no RandomSource:
//
//   1) The constructor's precomputed column-orientation tables
//      (columnSizeX / columnSizeZ, 1024 entries each), built from a 32x32 grid:
//
//        for (int z = 0; z < 32; z++)
//           for (int x = 0; x < 32; x++) {
//              float deltaX = x - 16;
//              float deltaZ = z - 16;
//              float distance = Mth.length(deltaX, deltaZ);
//              this.columnSizeX[z * 32 + x] = -deltaZ / distance;
//              this.columnSizeZ[z * 32 + x] =  deltaX / distance;
//           }
//      (WeatherEffectRenderer.java:68-78)
//      NOTE: index (16,16) gives deltaX=deltaZ=0 -> distance=0 -> -0.0/0 = NaN.
//      That entry is never read in-game (a column directly under the camera has
//      no quad billboard), but the table value is faithfully reproduced (NaN),
//      and the parity gate compares it as raw bits.
//
//   2) The per-column billboard geometry from renderInstances (the loop body),
//      which turns one ColumnInstance + cameraPos + maxAlpha + radius + intensity
//      into a packed color, a light value (passed through), and the 4 quad
//      vertices' (x,y,z, u,v):
//
//        float relativeX = (float)(column.x + 0.5 - cameraPos.x);
//        float relativeZ = (float)(column.z + 0.5 - cameraPos.z);
//        float distanceSq = (float)Mth.lengthSquared(relativeX, relativeZ);
//        float alpha = Mth.lerp(Math.min(distanceSq / radiusSq, 1.0F), maxAlpha, 0.5F) * intensity;
//        int color = ARGB.white(alpha);
//        int index = (column.z - Mth.floor(cameraPos.z) + 16) * 32 + column.x - Mth.floor(cameraPos.x) + 16;
//        float halfSizeX = this.columnSizeX[index] / 2.0F;
//        float halfSizeZ = this.columnSizeZ[index] / 2.0F;
//        float x0 = relativeX - halfSizeX;  float x1 = relativeX + halfSizeX;
//        float y1 = (float)(column.topY - cameraPos.y);
//        float y0 = (float)(column.bottomY - cameraPos.y);
//        float z0 = relativeZ - halfSizeZ;  float z1 = relativeZ + halfSizeZ;
//        float u0 = column.uOffset + 0.0F;  float u1 = column.uOffset + 1.0F;
//        float v0 = column.bottomY * 0.25F + column.vOffset;
//        float v1 = column.topY    * 0.25F + column.vOffset;
//        // vertices: (x0,y1,z0,u0,v0) (x1,y1,z1,u1,v0) (x1,y0,z1,u1,v1) (x0,y0,z0,u0,v1)
//      (WeatherEffectRenderer.java:244-266)
//
// where radiusSq = radius * radius (int*int -> int, stored in a float — line 242).
//
// 1:1 TRAPS faithfully handled:
//   * Mth.length(float,float) / Mth.lengthSquared(float,float): there is NO float
//     overload of lengthSquared, so the float args WIDEN TO DOUBLE for
//     lengthSquared(double,double) = x*x + y*y (computed in double), then sqrt, then
//     (float). (Mth.java:676-686). We must NOT compute x*x+y*y in float first.
//   * relativeX/Z, y0/y1: computed in double (int + 0.5 - double / int - double),
//     then narrowed with (float).
//   * Mth.floor(double) = (int)Math.floor(v) for the table index. (Mth.java:65-67)
//   * ARGB.white(float) = as8BitChannel(alpha) << 24 | 0x00FFFFFF, where
//     as8BitChannel(v) = Mth.floor(v * 255.0F) = (int)Math.floor((double)(v*255.0F)).
//     (ARGB.java:188-226, Mth.java:61-63)
//   * Mth.lerp(a,p0,p1) = p0 + a*(p1-p0). (Mth.java:532-534)
//   * Math.min(float,float): replicated faithfully (Java semantics for -0.0/NaN).
//   * column.bottomY/topY are ints: bottomY * 0.25F = (float)bottomY * 0.25F.
//   * All single-statement float ops compile under -ffp-contract=off (no FMA).
//
// SKIPPED (no portable pure math): extractRenderState / render / renderWeather /
// createRainColumnInstance / createSnowColumnInstance / tickRainParticles /
// getPrecipitationAt — these touch the Level, RandomSource, GPU render passes,
// heightmaps, particles and sound, and carry no standalone deterministic formula
// beyond what is gated here. They are listed as unported.

#include <cmath>
#include <cstdint>

#include "../../world/level/levelgen/Mth.h"

namespace mc::client::renderer::weather {

namespace mth = mc::levelgen::mth;

// ── Mth.length / lengthSquared, the FLOAT-arg overloads, ported EXACTLY ──
// length(float,float) = (float)Math.sqrt(lengthSquared((double)x,(double)y))
// lengthSquared(double,double) = x*x + y*y  (args widen to double FIRST).
inline float lengthFF(float x, float y) {
    double dx = static_cast<double>(x);
    double dy = static_cast<double>(y);
    return static_cast<float>(std::sqrt(dx * dx + dy * dy));
}
// (float)Mth.lengthSquared(float,float) — double accumulate, then (float) cast.
inline float lengthSquaredFF_asFloat(float x, float y) {
    double dx = static_cast<double>(x);
    double dy = static_cast<double>(y);
    return static_cast<float>(dx * dx + dy * dy);
}

// java.lang.Math.min(float,float): returns the smaller; -0.0 < +0.0; NaN propagates.
inline float jmin(float a, float b) {
    if (a != a) return a;             // a is NaN
    if (b != b) return b;             // b is NaN
    if (a == 0.0f && b == 0.0f) {     // distinguish -0.0 from +0.0
        // Math.min returns -0.0 if either is -0.0
        if (std::signbit(a) || std::signbit(b)) return -0.0f;
        return a;
    }
    return a <= b ? a : b;
}

// ARGB.as8BitChannel(float) = Mth.floor(value * 255.0F).  (ARGB.java:225-227)
inline int as8BitChannel(float value) {
    return mth::floor(value * 255.0F);
}

// ARGB.white(float) = as8BitChannel(alpha) << 24 | 0x00FFFFFF.  (ARGB.java:188-190)
inline int32_t argbWhite(float alpha) {
    return (as8BitChannel(alpha) << 24) | 0x00FFFFFF;
}

// ── 1) Constructor table init (WeatherEffectRenderer.java:68-78). ──
// Caller supplies two float[1024] buffers (columnSizeX, columnSizeZ).
inline void buildColumnSizeTables(float (&columnSizeX)[1024], float (&columnSizeZ)[1024]) {
    for (int z = 0; z < 32; z++) {
        for (int x = 0; x < 32; x++) {
            float deltaX = static_cast<float>(x - 16);
            float deltaZ = static_cast<float>(z - 16);
            float distance = lengthFF(deltaX, deltaZ);
            columnSizeX[z * 32 + x] = -deltaZ / distance;
            columnSizeZ[z * 32 + x] = deltaX / distance;
        }
    }
}

// One column billboard's emitted geometry, exactly as renderInstances writes it.
// Vertices are in the order WeatherEffectRenderer emits them (QUADS):
//   v[0] = (x0, y1, z0)  uv (u0, v0)
//   v[1] = (x1, y1, z1)  uv (u1, v0)
//   v[2] = (x1, y0, z1)  uv (u1, v1)
//   v[3] = (x0, y0, z0)  uv (u0, v1)
struct ColumnGeometry {
    int32_t color;       // ARGB.white(alpha)
    int32_t light;       // column.lightCoords, passed through
    float vx[4];
    float vy[4];
    float vz[4];
    float vu[4];
    float vv[4];
};

// Port of the renderInstances loop body (WeatherEffectRenderer.java:244-266).
// columnSizeX/columnSizeZ are the constructor tables built above.
inline ColumnGeometry renderColumnInstance(const float (&columnSizeX)[1024],
                                           const float (&columnSizeZ)[1024],
                                           int columnX, int columnZ,
                                           int columnBottomY, int columnTopY,
                                           float columnUOffset, float columnVOffset,
                                           int columnLightCoords,
                                           double cameraPosX, double cameraPosY, double cameraPosZ,
                                           float maxAlpha, int radius, float intensity) {
    float radiusSq = static_cast<float>(radius * radius);   // int*int -> int -> float

    float relativeX = static_cast<float>(columnX + 0.5 - cameraPosX);
    float relativeZ = static_cast<float>(columnZ + 0.5 - cameraPosZ);
    float distanceSq = lengthSquaredFF_asFloat(relativeX, relativeZ);
    float alpha = mth::lerpF(jmin(distanceSq / radiusSq, 1.0F), maxAlpha, 0.5F) * intensity;
    int32_t color = argbWhite(alpha);
    int index = (columnZ - mth::floor(cameraPosZ) + 16) * 32 + columnX - mth::floor(cameraPosX) + 16;
    float halfSizeX = columnSizeX[index] / 2.0F;
    float halfSizeZ = columnSizeZ[index] / 2.0F;
    float x0 = relativeX - halfSizeX;
    float x1 = relativeX + halfSizeX;
    float y1 = static_cast<float>(columnTopY - cameraPosY);
    float y0 = static_cast<float>(columnBottomY - cameraPosY);
    float z0 = relativeZ - halfSizeZ;
    float z1 = relativeZ + halfSizeZ;
    float u0 = columnUOffset + 0.0F;
    float u1 = columnUOffset + 1.0F;
    float v0 = static_cast<float>(columnBottomY) * 0.25F + columnVOffset;
    float v1 = static_cast<float>(columnTopY) * 0.25F + columnVOffset;

    ColumnGeometry g;
    g.color = color;
    g.light = columnLightCoords;
    g.vx[0] = x0; g.vy[0] = y1; g.vz[0] = z0; g.vu[0] = u0; g.vv[0] = v0;
    g.vx[1] = x1; g.vy[1] = y1; g.vz[1] = z1; g.vu[1] = u1; g.vv[1] = v0;
    g.vx[2] = x1; g.vy[2] = y0; g.vz[2] = z1; g.vu[2] = u1; g.vv[2] = v1;
    g.vx[3] = x0; g.vy[3] = y0; g.vz[3] = z0; g.vu[3] = u0; g.vv[3] = v1;
    return g;
}

}  // namespace mc::client::renderer::weather

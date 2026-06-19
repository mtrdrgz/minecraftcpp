// 1:1 port of net.minecraft.client.renderer.culling.Frustum (MC 26.1.2) together
// with the org.joml.FrustumIntersection subset it uses (JOML 1.10.8).
//
// SOURCE OF TRUTH (transcribed VERBATIM from bytecode, no deviation):
//   - net/minecraft/client/renderer/culling/Frustum.java (26.1.2/src)
//   - org.joml.FrustumIntersection.set(Matrix4fc, boolean)  -> 6-plane extraction
//   - org.joml.FrustumIntersection.intersectAab(f,f,f,f,f,f) -> plane/AABB test
//   - org.joml.Math.invsqrt(float) = 1.0f / (float)Math.sqrt((double)x)
//     (disassembled: build_dbg_joml_Math.txt L630). Math.sqrt is correctly
//     rounded -> std::sqrt matches; the (float)cast is the JOML d2f.
//
// The Frustum constructor does:  projection.mul(modelView, this.matrix);
//                                this.intersection.set(this.matrix);
// i.e. the 6 planes are extracted from the COMBINED matrix proj*modelView.
// The combined matrix is supplied to this port directly (column-major, the JOML
// mXY accessor = column X, row Y); reproducing the property-dispatched
// Matrix4f.mul itself is out of scope for this gate (see notes). Everything
// downstream of the combined matrix — the plane extraction and the AABB test —
// is ported here EXACTLY and gated bit-for-bit against the real classes.
#pragma once

#include <cmath>
#include <cstdint>

namespace mc::render::frustum {

// org.joml.Math.invsqrt(float): fconst_1, f2d, Math.sqrt, d2f, fdiv  (L630)
inline float jInvsqrt(float x) { return 1.0f / (float)std::sqrt((double)x); }

// org.joml.FrustumIntersection — only the fields/methods Frustum touches.
//
// Column-major naming follows org.joml.Matrix4fc accessors used by set():
//   m00..m33 where mCR == column C, row R (matches mc::render::model::joml::Matrix4f).
struct FrustumIntersection {
    static constexpr int PLANE_NX = 0;
    static constexpr int PLANE_PX = 1;
    static constexpr int PLANE_NY = 2;
    static constexpr int PLANE_PY = 3;
    static constexpr int PLANE_NZ = 4;
    static constexpr int PLANE_PZ = 5;
    static constexpr int INTERSECT = -2;
    static constexpr int INSIDE = -1;

    // plane coefficients (a,b,c) and offset (W) for each of the 6 planes
    float nxX = 0, nxY = 0, nxZ = 0, nxW = 0;
    float pxX = 0, pxY = 0, pxZ = 0, pxW = 0;
    float nyX = 0, nyY = 0, nyZ = 0, nyW = 0;
    float pyX = 0, pyY = 0, pyZ = 0, pyW = 0;
    float nzX = 0, nzY = 0, nzZ = 0, nzW = 0;
    float pzX = 0, pzY = 0, pzZ = 0, pzW = 0;

    // org.joml.FrustumIntersection.set(Matrix4fc m) -> set(m, true)
    // set(m, allowTest): extract the 6 frustum planes from the clip matrix and
    // (allowTest=true) normalize each plane's (x,y,z,w) by invsqrt(x*x+y*y+z*z).
    // The 1-arg set used by Frustum always passes allowTest = true (iconst_1).
    void set(float m00, float m01, float m02, float m03,
             float m10, float m11, float m12, float m13,
             float m20, float m21, float m22, float m23,
             float m30, float m31, float m32, float m33,
             bool allowTest = true) {
        // PLANE_NX : (m03+m00, m13+m10, m23+m20, m33+m30)
        nxX = m03 + m00; nxY = m13 + m10; nxZ = m23 + m20; nxW = m33 + m30;
        if (allowTest) {
            float invl = jInvsqrt(nxX * nxX + nxY * nxY + nxZ * nxZ);
            nxX *= invl; nxY *= invl; nxZ *= invl; nxW *= invl;
        }
        // PLANE_PX : (m03-m00, m13-m10, m23-m20, m33-m30)
        pxX = m03 - m00; pxY = m13 - m10; pxZ = m23 - m20; pxW = m33 - m30;
        if (allowTest) {
            float invl = jInvsqrt(pxX * pxX + pxY * pxY + pxZ * pxZ);
            pxX *= invl; pxY *= invl; pxZ *= invl; pxW *= invl;
        }
        // PLANE_NY : (m03+m01, m13+m11, m23+m21, m33+m31)
        nyX = m03 + m01; nyY = m13 + m11; nyZ = m23 + m21; nyW = m33 + m31;
        if (allowTest) {
            float invl = jInvsqrt(nyX * nyX + nyY * nyY + nyZ * nyZ);
            nyX *= invl; nyY *= invl; nyZ *= invl; nyW *= invl;
        }
        // PLANE_PY : (m03-m01, m13-m11, m23-m21, m33-m31)
        pyX = m03 - m01; pyY = m13 - m11; pyZ = m23 - m21; pyW = m33 - m31;
        if (allowTest) {
            float invl = jInvsqrt(pyX * pyX + pyY * pyY + pyZ * pyZ);
            pyX *= invl; pyY *= invl; pyZ *= invl; pyW *= invl;
        }
        // PLANE_NZ : (m03+m02, m13+m12, m23+m22, m33+m32)
        nzX = m03 + m02; nzY = m13 + m12; nzZ = m23 + m22; nzW = m33 + m32;
        if (allowTest) {
            float invl = jInvsqrt(nzX * nzX + nzY * nzY + nzZ * nzZ);
            nzX *= invl; nzY *= invl; nzZ *= invl; nzW *= invl;
        }
        // PLANE_PZ : (m03-m02, m13-m12, m23-m22, m33-m32)
        pzX = m03 - m02; pzY = m13 - m12; pzZ = m23 - m22; pzW = m33 - m32;
        if (allowTest) {
            float invl = jInvsqrt(pzX * pzX + pzY * pzY + pzZ * pzZ);
            pzX *= invl; pzY *= invl; pzZ *= invl; pzW *= invl;
        }
    }

    // org.joml.FrustumIntersection.intersectAab(minX,minY,minZ,maxX,maxY,maxZ)
    // Returns the index (0..5) of the first plane that culls the box (OUTSIDE),
    // or INTERSECT (-2) if every plane's near corner is also inside (fully
    // inside the frustum), or INSIDE (-1) if it straddles at least one plane.
    //
    // For each plane:
    //   outside corner test: d_out = X*(X<0?minX:maxX) + Y*(Y<0?minY:maxY)
    //                              + Z*(Z<0?minZ:maxZ);  if d_out < -W -> cull.
    //   inside  corner test: d_in  = X*(X<0?maxX:minX) + Y*(Y<0?maxY:minY)
    //                              + Z*(Z<0?maxZ:minZ);  inside &= (d_in >= -W).
    // `plane` is advanced to the next index after each plane passes, so the
    // early return reports the failing plane's index (verbatim from bytecode).
    int intersectAab(float minX, float minY, float minZ,
                     float maxX, float maxY, float maxZ) const {
        int plane = 0;
        int inside = 1;

        // --- PLANE_NX ---
        if (nxX * (nxX < 0 ? minX : maxX) + nxY * (nxY < 0 ? minY : maxY)
                + nxZ * (nxZ < 0 ? minZ : maxZ) >= -nxW) {
            plane = 1;
            inside &= (nxX * (nxX < 0 ? maxX : minX) + nxY * (nxY < 0 ? maxY : minY)
                       + nxZ * (nxZ < 0 ? maxZ : minZ) >= -nxW) ? 1 : 0;

            // --- PLANE_PX ---
            if (pxX * (pxX < 0 ? minX : maxX) + pxY * (pxY < 0 ? minY : maxY)
                    + pxZ * (pxZ < 0 ? minZ : maxZ) >= -pxW) {
                plane = 2;
                inside &= (pxX * (pxX < 0 ? maxX : minX) + pxY * (pxY < 0 ? maxY : minY)
                           + pxZ * (pxZ < 0 ? maxZ : minZ) >= -pxW) ? 1 : 0;

                // --- PLANE_NY ---
                if (nyX * (nyX < 0 ? minX : maxX) + nyY * (nyY < 0 ? minY : maxY)
                        + nyZ * (nyZ < 0 ? minZ : maxZ) >= -nyW) {
                    plane = 3;
                    inside &= (nyX * (nyX < 0 ? maxX : minX) + nyY * (nyY < 0 ? maxY : minY)
                               + nyZ * (nyZ < 0 ? maxZ : minZ) >= -nyW) ? 1 : 0;

                    // --- PLANE_PY ---
                    if (pyX * (pyX < 0 ? minX : maxX) + pyY * (pyY < 0 ? minY : maxY)
                            + pyZ * (pyZ < 0 ? minZ : maxZ) >= -pyW) {
                        plane = 4;
                        inside &= (pyX * (pyX < 0 ? maxX : minX) + pyY * (pyY < 0 ? maxY : minY)
                                   + pyZ * (pyZ < 0 ? maxZ : minZ) >= -pyW) ? 1 : 0;

                        // --- PLANE_NZ ---
                        if (nzX * (nzX < 0 ? minX : maxX) + nzY * (nzY < 0 ? minY : maxY)
                                + nzZ * (nzZ < 0 ? minZ : maxZ) >= -nzW) {
                            plane = 5;
                            inside &= (nzX * (nzX < 0 ? maxX : minX) + nzY * (nzY < 0 ? maxY : minY)
                                       + nzZ * (nzZ < 0 ? maxZ : minZ) >= -nzW) ? 1 : 0;

                            // --- PLANE_PZ ---
                            if (pzX * (pzX < 0 ? minX : maxX) + pzY * (pzY < 0 ? minY : maxY)
                                    + pzZ * (pzZ < 0 ? minZ : maxZ) >= -pzW) {
                                inside &= (pzX * (pzX < 0 ? maxX : minX) + pzY * (pzY < 0 ? maxY : minY)
                                           + pzZ * (pzZ < 0 ? maxZ : minZ) >= -pzW) ? 1 : 0;
                                return inside != 0 ? INTERSECT : INSIDE;
                            }
                        }
                    }
                }
            }
        }
        return plane;
    }
};

// net.minecraft.client.renderer.culling.Frustum
//
// Holds the extracted intersection planes plus the camera position. AABB tests
// are done in camera-relative float space, exactly as cubeInFrustum() does.
struct Frustum {
    FrustumIntersection intersection;
    double camX = 0.0, camY = 0.0, camZ = 0.0;

    // Frustum(modelView, projection) -> calculateFrustum(modelView, projection):
    //   projection.mul(modelView, this.matrix); this.intersection.set(this.matrix);
    // Here the COMBINED matrix (proj*modelView) is supplied directly.
    void setCombinedMatrix(float m00, float m01, float m02, float m03,
                           float m10, float m11, float m12, float m13,
                           float m20, float m21, float m22, float m23,
                           float m30, float m31, float m32, float m33) {
        intersection.set(m00, m01, m02, m03, m10, m11, m12, m13,
                         m20, m21, m22, m23, m30, m31, m32, m33, true);
    }

    // Frustum.prepare(camX, camY, camZ)
    void prepare(double cx, double cy, double cz) { camX = cx; camY = cy; camZ = cz; }

    // Frustum.cubeInFrustum(minX..maxZ): convert to camera-relative floats and
    // delegate to intersectAab. (Java casts each (coord - cam) double to float.)
    int cubeInFrustum(double minX, double minY, double minZ,
                      double maxX, double maxY, double maxZ) const {
        float x1 = (float)(minX - camX);
        float y1 = (float)(minY - camY);
        float z1 = (float)(minZ - camZ);
        float x2 = (float)(maxX - camX);
        float y2 = (float)(maxY - camY);
        float z2 = (float)(maxZ - camZ);
        return intersection.intersectAab(x1, y1, z1, x2, y2, z2);
    }

    // Frustum.isVisible(AABB): result == INTERSECT(-2) || result == INSIDE(-1)
    bool isVisible(double minX, double minY, double minZ,
                   double maxX, double maxY, double maxZ) const {
        int r = cubeInFrustum(minX, minY, minZ, maxX, maxY, maxZ);
        return r == FrustumIntersection::INTERSECT || r == FrustumIntersection::INSIDE;
    }
};

} // namespace mc::render::frustum

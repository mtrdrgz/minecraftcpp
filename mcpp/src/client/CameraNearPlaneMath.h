#pragma once

// Bit-exact 1:1 port of the pure near-plane geometry math published by
// net.minecraft.client.Camera (Minecraft Java Edition 26.1.2):
//
//   * Camera#getNearPlane(float fov)                  (Camera.java, lines 418-426)
//   * Camera.NearPlane#getTopLeft / getTopRight /
//       getBottomLeft / getBottomRight / getPointOnPlane (lines 495-525)
//
// Source (26.1.2/src/net/minecraft/client/Camera.java):
//
//   public Camera.NearPlane getNearPlane(final float fov) {
//      double aspectRatio = (double)this.projection.width() / this.projection.height();
//      double planeHeight = Math.tan(fov * (float)(Math.PI / 180.0) / 2.0) * this.projection.zNear();
//      double planeWidth = planeHeight * aspectRatio;
//      Vec3 forwardsVec3 = new Vec3(this.forwards).scale(this.projection.zNear());
//      Vec3 leftVec3     = new Vec3(this.left).scale(planeWidth);
//      Vec3 upVec3       = new Vec3(this.up).scale(planeHeight);
//      return new Camera.NearPlane(forwardsVec3, leftVec3, upVec3);
//   }
//
//   public static class NearPlane {
//      private final Vec3 forward, left, up;
//      public Vec3 getTopLeft()     { return forward.add(up).add(left); }
//      public Vec3 getTopRight()    { return forward.add(up).subtract(left); }
//      public Vec3 getBottomLeft()  { return forward.subtract(up).add(left); }
//      public Vec3 getBottomRight() { return forward.subtract(up).subtract(left); }
//      public Vec3 getPointOnPlane(float x, float y) {
//         return forward.add(up.scale(y)).subtract(left.scale(x));
//      }
//   }
//
// The camera's forwards/up/left basis is itself produced by the already-certified
// Camera#setRotation (see client/CameraRotationMath.h, which drives the REAL
// org.joml Quaternionf#rotationYXZ + Vector3f#rotate). This file consumes that
// basis and ports ONLY the additional near-plane arithmetic, which is genuinely
// ungated elsewhere in the port.
//
// Bit-exactness facts (read straight from the bytecode of the shipped classes):
//
//  * projection.width()/height()/zNear() are float field getters
//    (net.minecraft.client.renderer.Projection). The real source casts the
//    numerator explicitly:
//        double aspectRatio = (double)this.projection.width() / this.projection.height();
//    so `width()` is widened to double FIRST and the division is a DOUBLE divide
//    (the float `height()` is promoted to double by binary numeric promotion). We
//    reproduce that exactly: (double)width / (double)height. (A float/float divide
//    widened afterwards would differ in the low mantissa bits.)
//
//  * planeHeight: `fov * (float)(Math.PI/180.0)` is float*float -> float; `/ 2.0`
//    is float/double -> double (2.0 is a double literal). So the argument to
//    Math.tan is a double. Math.tan(double) is java.lang.Math.tan (a libm/
//    StrictMath-class transcendental); std::tan(double) matches it on this
//    platform for the finite physical inputs the gate exercises. The product with
//    zNear is double * (double)((float)zNear).
//
//  * new Vec3(Vector3fc) widens each float component to double via (double)f.
//    Vec3.scale(s) = multiply(s,s,s) = new Vec3(x*s, y*s, z*s) (all double).
//    Vec3.add / Vec3.subtract are plain double + / - .
//
//  * getPointOnPlane takes float x,y; up.scale((double)y) and left.scale((double)x)
//    widen the float arg to double in the multiply.
//
// NO deviation from the bytecode is permitted in this file.

#include <cmath>
#include <cstdint>

#include "client/CameraRotationMath.h"

namespace mc {
namespace client {
namespace camera_nearplane {

// (float)(Math.PI / 180.0) == 0.017453292f, computed exactly as Java narrows it.
inline constexpr double kPi = 3.141592653589793;
inline float deg2rad_f() { return static_cast<float>(kPi / 180.0); }

// net.minecraft.world.phys.Vec3 — only the constructors/ops the near-plane math
// needs. Components are doubles, matching net.minecraft.world.phys.Vec3.
struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    // new Vec3(Vector3fc vec): this(vec.x(), vec.y(), vec.z()) — float widened to double.
    explicit Vec3(const camera_rot::Vector3f& v)
        : x(static_cast<double>(v.x)),
          y(static_cast<double>(v.y)),
          z(static_cast<double>(v.z)) {}

    // add(double x,y,z): new Vec3(this.x+x, this.y+y, this.z+z)
    Vec3 add(double ax, double ay, double az) const { return Vec3(x + ax, y + ay, z + az); }
    // add(Vec3 vec): add(vec.x, vec.y, vec.z)
    Vec3 add(const Vec3& v) const { return add(v.x, v.y, v.z); }
    // subtract(double x,y,z): add(-x, -y, -z)
    Vec3 subtract(double sx, double sy, double sz) const { return add(-sx, -sy, -sz); }
    // subtract(Vec3 vec): subtract(vec.x, vec.y, vec.z)
    Vec3 subtract(const Vec3& v) const { return subtract(v.x, v.y, v.z); }
    // scale(double s): multiply(s,s,s) -> new Vec3(this.x*s, this.y*s, this.z*s)
    Vec3 scale(double s) const { return Vec3(x * s, y * s, z * s); }
};

// Camera.NearPlane: holds the three scaled basis vectors and derives the corners.
struct NearPlane {
    Vec3 forward;
    Vec3 left;
    Vec3 up;

    Vec3 getTopLeft()     const { return forward.add(up).add(left); }
    Vec3 getTopRight()    const { return forward.add(up).subtract(left); }
    Vec3 getBottomLeft()  const { return forward.subtract(up).add(left); }
    Vec3 getBottomRight() const { return forward.subtract(up).subtract(left); }

    // getPointOnPlane(float x, float y): forward.add(up.scale(y)).subtract(left.scale(x))
    Vec3 getPointOnPlane(float px, float py) const {
        return forward.add(up.scale(static_cast<double>(py)))
                      .subtract(left.scale(static_cast<double>(px)));
    }
};

// Camera#getNearPlane(float fov): builds the NearPlane from the camera's
// forwards/up/left basis and the projection's width/height/zNear.
inline NearPlane getNearPlane(float fov, float projWidth, float projHeight, float projZNear,
                              const camera_rot::Vector3f& forwards,
                              const camera_rot::Vector3f& up,
                              const camera_rot::Vector3f& left) {
    // (double)this.projection.width() / this.projection.height(): the numerator is
    // cast to double first, so this is a DOUBLE division (height is promoted).
    double aspectRatio = static_cast<double>(projWidth) / static_cast<double>(projHeight);
    // `fov * (float)(Math.PI/180.0)` is float*float -> float; `/ 2.0` promotes to
    // double; Math.tan(double); the product with zNear is double * (double)float.
    double planeHeight = std::tan(static_cast<double>(fov * deg2rad_f()) / 2.0)
                       * static_cast<double>(projZNear);
    double planeWidth = planeHeight * aspectRatio;

    NearPlane plane;
    plane.forward = Vec3(forwards).scale(static_cast<double>(projZNear));
    plane.left = Vec3(left).scale(planeWidth);
    plane.up = Vec3(up).scale(planeHeight);
    return plane;
}

// Convenience: drive the full chain from (yRot, xRot, fov, projection dims) by
// first computing the orientation basis via the certified setRotation port.
inline NearPlane getNearPlaneFromRotation(float yRot, float xRot, float fov,
                                          float projWidth, float projHeight, float projZNear) {
    camera_rot::CameraOrientation o = camera_rot::setRotation(yRot, xRot);
    return getNearPlane(fov, projWidth, projHeight, projZNear, o.forwards, o.up, o.left);
}

} // namespace camera_nearplane
} // namespace client
} // namespace mc

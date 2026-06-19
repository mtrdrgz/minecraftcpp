#pragma once

// 1:1 port of the PURE physics integrator of the particle base class
// net.minecraft.client.particle.Particle (Minecraft Java Edition 26.1.2).
//
// Particle.java carries a small deterministic kinematics core that, when the
// particle has NO collision physics (hasPhysics == false), touches neither the
// ClientLevel, Blaze3D, nor any registry — it is pure double/float arithmetic
// over an axis-aligned bounding box. That core is exactly:
//
//   protected void setSize(final float w, final float h)        (Particle.java:125-134)
//   public void setPos(final double x, final double y, final double z) (136-143)
//   public void move(double xa, double ya, double za)           (145-175)
//   protected void setLocationFromBoundingbox()                 (177-182)
//   public void tick()                                          (90-112)
//   public Particle scale(final float scale)                    (77-80)
//   public Particle setPower(final float power)                 (64-69)
//   public void setParticleSpeed(double xd, double yd, double zd)(71-75)
//
// move() has a collision branch guarded by `this.hasPhysics && ...`. With
// hasPhysics == false (every "floaty"/non-colliding particle: note, portal,
// firework spark, fly-toward, suspended-town, ...), that branch is skipped and
// `this.level` is never read, so the entire integrator is hermetic. This port
// reproduces ONLY that hermetic surface; the collision path (which calls
// Entity.collideBoundingBox(level, ...)) is a documented no-op here (asserted
// by keeping hasPhysics == false) — it is NOT silently emulated.
//
// AABB pieces touched by the integrator (1:1, net.minecraft.world.phys.AABB):
//   AABB(double minX,minY,minZ, double maxX,maxY,maxZ)          (AABB.java:22-29)
//        = min/max each pair (corner-sorting)
//   AABB move(double xa, double ya, double za)                  (AABB.java:219-221)
//        = new AABB(minX+xa, minY+ya, minZ+za, maxX+xa, maxY+ya, maxZ+za)
//   the INITIAL_AABB is AABB(0,0,0,0,0,0)                       (Particle.java:16)
// We re-port just these two AABB operations locally (rather than pulling the
// glm-backed world/phys/AABB.h) so this gate stays self-contained; the math is
// identical and the corner-sorting is shown explicitly.
//
// 1:1 arithmetic notes (read straight from the source):
//   * fields default exactly as declared in Particle.java:
//       bb = INITIAL_AABB; onGround=false; hasPhysics=true; stoppedByCollision=false;
//       removed=false; bbWidth=0.6F; bbHeight=1.8F; age=0; gravity=0F (field);
//       friction=0.98F; speedUpWhenYMotionIsBlocked=false.
//   * tick(): xo/yo/zo := x/y/z; if (age++ >= lifetime) remove(); else
//       yd -= 0.04 * gravity;  (0.04 is double; gravity float WIDENS to double)
//       move(xd, yd, zd);
//       if (speedUpWhenYMotionIsBlocked && y == yo) { xd *= 1.1; zd *= 1.1; }
//       xd *= friction; yd *= friction; zd *= friction;  (friction float WIDENS)
//       if (onGround) { xd *= 0.7F; zd *= 0.7F; }         (0.7F float WIDENS)
//   * move(): with hasPhysics==false the collide branch is skipped. Then
//       if (xa!=0||ya!=0||za!=0) { bb = bb.move(xa,ya,za); setLocationFromBoundingbox(); }
//       if (abs(originalYa) >= 1.0E-5F && abs(ya) < 1.0E-5F) stoppedByCollision=true;
//         (1.0E-5F is a FLOAT literal; Math.abs(double) compared against it ->
//          float WIDENS to double for the comparison)
//       onGround = (originalYa != ya) && (originalYa < 0.0);
//       if (originalXa != xa) xd = 0.0;
//       if (originalZa != za) zd = 0.0;
//     (xa/ya/za are never reassigned when hasPhysics==false, so originalXa==xa
//      etc. always hold here; reproduced faithfully regardless.)
//   * setSize(w,h): only acts when w!=bbWidth || h!=bbHeight; recenters the bb
//       around its current min-corner using
//         newMinX = (minX + maxX - w) / 2.0;  newMinZ = (minZ + maxZ - w) / 2.0;
//         bb = AABB(newMinX, minY, newMinZ, newMinX+bbWidth, minY+bbHeight, newMinZ+bbWidth);
//       NOTE the Java uses bbWidth/bbHeight (already updated to w/h) for the maxes.
//   * setPos(x,y,z): w = bbWidth/2.0F (float divide, then WIDENS); h = bbHeight;
//       bb = AABB(x-w, y, z-w, x+w, y+h, z+w).
//   * setLocationFromBoundingbox(): x=(minX+maxX)/2.0; y=minY; z=(minZ+maxZ)/2.0.
//   * scale(s):    setSize(0.2F*s, 0.2F*s).
//   * setPower(p): xd*=p; yd=(yd-0.1F)*p + 0.1F; zd*=p.   (0.1F float WIDENS)
//   * setParticleSpeed(xd,yd,zd): direct assignment.
//
// Certified bit-for-bit by particle_physics_parity (ground truth:
// tools/ParticlePhysicsParity.java, which allocateInstance()s a REAL Particle and
// reflectively drives its setSize/setPos/move/tick/scale/setPower bodies).

#include <cmath>

namespace mc::client::particle {

// java.lang.Math.min/max(double,double) — 1:1 (handles NaN and signed zero exactly as
// the JDK intrinsics do; std::min/std::max do NOT). The integrator's inputs are finite
// and non-neg-zero so the NaN paths never fire, but the -0.0 vs +0.0 tie-break can occur
// (e.g. corner-sorting equal coordinates), so we reproduce it faithfully.
inline double jmin(double a, double b) {
    if (a != a) return a;            // NaN
    if (a == 0.0 && b == 0.0) {
        // both zero: Math.min prefers -0.0. Use raw sign bit (b's bit when negative).
        return (std::signbit(a) || std::signbit(b)) ? -0.0 : 0.0;
    }
    return a <= b ? a : b;
}
inline double jmax(double a, double b) {
    if (a != a) return a;            // NaN
    if (a == 0.0 && b == 0.0) {
        // both zero: Math.max prefers +0.0.
        return (!std::signbit(a) || !std::signbit(b)) ? 0.0 : -0.0;
    }
    return a >= b ? a : b;
}

// ── the two AABB operations the integrator uses (net.minecraft.world.phys.AABB) ──
struct PAABB {
    double minX, minY, minZ, maxX, maxY, maxZ;

    // AABB(double minX, double minY, double minZ, double maxX, double maxY, double maxZ)
    //   this.minX = Math.min(minX, maxX); ... this.maxX = Math.max(minX, maxX); ...
    static PAABB make(double x0, double y0, double z0, double x1, double y1, double z1) {
        return PAABB{
            jmin(x0, x1), jmin(y0, y1), jmin(z0, z1),
            jmax(x0, x1), jmax(y0, y1), jmax(z0, z1)};
    }

    // AABB move(double xa, double ya, double za)
    PAABB move(double xa, double ya, double za) const {
        return PAABB::make(minX + xa, minY + ya, minZ + za, maxX + xa, maxY + ya, maxZ + za);
    }
};

// 1:1 of the hermetic (hasPhysics == false) subset of net.minecraft.client.particle.Particle.
struct ParticlePhysics {
    // ── fields, in declaration order, with Particle.java's exact defaults ──
    double xo = 0.0, yo = 0.0, zo = 0.0;
    double x = 0.0, y = 0.0, z = 0.0;
    double xd = 0.0, yd = 0.0, zd = 0.0;
    PAABB bb = PAABB{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};  // INITIAL_AABB = new AABB(0,0,0,0,0,0)
    bool onGround = false;
    bool hasPhysics = true;
    bool stoppedByCollision = false;
    bool removed = false;
    float bbWidth = 0.6F;
    float bbHeight = 1.8F;
    int age = 0;
    int lifetime = 0;
    float gravity = 0.0F;
    float friction = 0.98F;
    bool speedUpWhenYMotionIsBlocked = false;

    // public void remove() { this.removed = true; }
    void remove() { removed = true; }

    // protected void setLocationFromBoundingbox()
    void setLocationFromBoundingbox() {
        x = (bb.minX + bb.maxX) / 2.0;
        y = bb.minY;
        z = (bb.minZ + bb.maxZ) / 2.0;
    }

    // protected void setSize(final float w, final float h)
    void setSize(float w, float h) {
        if (w != bbWidth || h != bbHeight) {
            bbWidth = w;
            bbHeight = h;
            const PAABB& aabb = bb;
            double newMinX = (aabb.minX + aabb.maxX - static_cast<double>(w)) / 2.0;
            double newMinZ = (aabb.minZ + aabb.maxZ - static_cast<double>(w)) / 2.0;
            bb = PAABB::make(newMinX, aabb.minY, newMinZ,
                             newMinX + static_cast<double>(bbWidth),
                             aabb.minY + static_cast<double>(bbHeight),
                             newMinZ + static_cast<double>(bbWidth));
        }
    }

    // public void setPos(final double x, final double y, final double z)
    void setPos(double nx, double ny, double nz) {
        x = nx;
        y = ny;
        z = nz;
        float w = bbWidth / 2.0F;
        float h = bbHeight;
        bb = PAABB::make(nx - static_cast<double>(w), ny, nz - static_cast<double>(w),
                         nx + static_cast<double>(w), ny + static_cast<double>(h),
                         nz + static_cast<double>(w));
    }

    // public void move(double xa, double ya, double za) — hermetic (hasPhysics==false) path.
    // The collision branch `if (this.hasPhysics && ...)` reads this.level; it is NOT
    // ported. Callers of this struct keep hasPhysics == false (the integrator's hermetic
    // regime); reaching the physics regime is a documented no-op, not a silent emulation.
    void move(double xa, double ya, double za) {
        if (!stoppedByCollision) {
            double originalXa = xa;
            double originalYa = ya;
            double originalZa = za;
            // collision branch (hasPhysics) intentionally not reproduced here.

            if (xa != 0.0 || ya != 0.0 || za != 0.0) {
                bb = bb.move(xa, ya, za);
                setLocationFromBoundingbox();
            }

            if (std::abs(originalYa) >= static_cast<double>(1.0E-5F) &&
                std::abs(ya) < static_cast<double>(1.0E-5F)) {
                stoppedByCollision = true;
            }

            onGround = originalYa != ya && originalYa < 0.0;
            if (originalXa != xa) {
                xd = 0.0;
            }

            if (originalZa != za) {
                zd = 0.0;
            }
        }
    }

    // public void tick()
    void tick() {
        xo = x;
        yo = y;
        zo = z;
        if (age++ >= lifetime) {
            remove();
        } else {
            yd = yd - 0.04 * static_cast<double>(gravity);
            move(xd, yd, zd);
            if (speedUpWhenYMotionIsBlocked && y == yo) {
                xd *= 1.1;
                zd *= 1.1;
            }

            xd = xd * static_cast<double>(friction);
            yd = yd * static_cast<double>(friction);
            zd = zd * static_cast<double>(friction);
            if (onGround) {
                xd *= static_cast<double>(0.7F);
                zd *= static_cast<double>(0.7F);
            }
        }
    }

    // public Particle scale(final float scale)
    void scale(float scaleAmount) { setSize(0.2F * scaleAmount, 0.2F * scaleAmount); }

    // public Particle setPower(final float power)
    void setPower(float power) {
        xd *= static_cast<double>(power);
        yd = (yd - static_cast<double>(0.1F)) * static_cast<double>(power) + static_cast<double>(0.1F);
        zd *= static_cast<double>(power);
    }

    // public void setParticleSpeed(final double xd, final double yd, final double zd)
    void setParticleSpeed(double nxd, double nyd, double nzd) {
        xd = nxd;
        yd = nyd;
        zd = nzd;
    }
};

}  // namespace mc::client::particle

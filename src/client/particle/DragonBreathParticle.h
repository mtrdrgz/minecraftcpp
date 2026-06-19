#pragma once

// 1:1 port of the PURE per-tick motion integrator of
// net.minecraft.client.particle.DragonBreathParticle (Minecraft Java Edition
// 26.1.2) — the lingering "dragon breath" cloud particle.
//
// DragonBreathParticle OVERRIDES Particle.tick() ENTIRELY (it does NOT call
// super.tick()) with a bespoke integrator that differs from the base class:
// it has a "hasHitGround" sticky state, a fixed +0.002 upward creep once
// grounded, an unconditional y==yo speed-up (the base class gates that on
// speedUpWhenYMotionIsBlocked), and applies friction to xd/zd always but to yd
// only after hitting the ground. Its tick() body is hermetic when the particle
// is non-colliding (hasPhysics == false, which the constructor sets): it touches
// neither the ClientLevel, Blaze3D, a registry, nor the RandomSource. The only
// non-arithmetic call, setSpriteFromAge(sprites), merely swaps the render sprite
// (it never reads or writes the position/velocity state we certify), so it is
// outside this gate.
//
//   public void tick() {                                  (DragonBreathParticle.java:46-72)
//      this.xo = this.x;
//      this.yo = this.y;
//      this.zo = this.z;
//      if (this.age++ >= this.lifetime) {
//         this.remove();
//      } else {
//         this.setSpriteFromAge(this.sprites);             // sprite only -> not ported here
//         if (this.onGround) {
//            this.yd = 0.0;
//            this.hasHitGround = true;
//         }
//         if (this.hasHitGround) {
//            this.yd += 0.002;
//         }
//         this.move(this.xd, this.yd, this.zd);
//         if (this.y == this.yo) {
//            this.xd *= 1.1;
//            this.zd *= 1.1;
//         }
//         this.xd = this.xd * this.friction;
//         this.zd = this.zd * this.friction;
//         if (this.hasHitGround) {
//            this.yd = this.yd * this.friction;
//         }
//      }
//   }
//
//   public float getQuadSize(final float a) {              (DragonBreathParticle.java:80-82)
//      return this.quadSize * Mth.clamp((this.age + a) / this.lifetime * 32.0F, 0.0F, 1.0F);
//   }
//
// move() is the inherited Particle.move() (Particle.java:145-174). With
// hasPhysics == false the collision branch (which reads this.level via
// Entity.collideBoundingBox) is skipped, so move() is pure double arithmetic over
// the particle's axis-aligned bounding box — IDENTICAL to the hermetic path
// already certified by particle_physics_parity (see ParticlePhysics.h). It is
// re-ported here, inline, so this gate is self-contained.
//
// 1:1 arithmetic notes (read straight from the source — float-widening rules):
//   * `this.age++ >= this.lifetime`: POST-increment; the comparison reads the
//     pre-increment age, the increment lands before the else-body. When the gate
//     fires, remove() runs and x/y/z keep their previous values (only xo/yo/zo
//     were refreshed this tick).
//   * `this.yd += 0.002`: 0.002 is a DOUBLE literal; yd is double. Plain add.
//   * `this.move(this.xd, this.yd, this.zd)`: see move() below. The y==yo test
//     that follows compares the post-move y against this tick's yo.
//   * `this.xd = this.xd * this.friction`: friction is a FLOAT field; it WIDENS
//     to double for the multiply (double = double * float). Same for zd and the
//     grounded yd multiply.
//   * `this.xd *= 1.1; this.zd *= 1.1;`: 1.1 is DOUBLE.
//   * onGround handling: when onGround is true at the top of a tick, yd:=0.0 and
//     hasHitGround latches true. In a purely non-colliding simulation move() only
//     ever leaves onGround == false (originalYa == ya always, since ya is never
//     reassigned when hasPhysics == false), so onGround is meaningful only as a
//     SEEDED precondition — the ground truth seeds it directly (that state is
//     produced by collision in-game, which is out of this gate's hermetic scope).
//
// move() (Particle.java:145-174), hermetic (hasPhysics == false) path:
//   if (!stoppedByCollision) {
//      double originalXa=xa, originalYa=ya, originalZa=za;
//      // collision branch (hasPhysics) skipped
//      if (xa!=0||ya!=0||za!=0) { bb = bb.move(xa,ya,za); setLocationFromBoundingbox(); }
//      if (abs(originalYa) >= 1.0E-5F && abs(ya) < 1.0E-5F) stoppedByCollision=true;
//      onGround = originalYa != ya && originalYa < 0.0;     // -> false in pure sim
//      if (originalXa != xa) xd = 0.0;                      // never (xa unchanged)
//      if (originalZa != za) zd = 0.0;                      // never (za unchanged)
//   }
//   1.0E-5F is a FLOAT literal; Math.abs(double) is compared against it, so the
//   float WIDENS to double. AABB corner-sorting uses Math.min/Math.max(double).
//
// Certified bit-for-bit by dragon_breath_particle_parity (ground truth:
// tools/DragonBreathParticleParity.java, which allocateInstance()s a REAL
// DragonBreathParticle — no constructor, so no RandomSource/ClientLevel — gives
// it a null-returning SpriteSet proxy so setSpriteFromAge is a harmless sprite
// swap, reflectively seeds its state, and drives the REAL tick()/getQuadSize()).

#include <algorithm>
#include <cmath>

namespace mc::client::particle {

// java.lang.Math.min/max(double,double) for AABB corner-sorting — 1:1 (signed-zero
// tie-break exactly as the JDK intrinsics, which std::min/std::max do not do).
inline double dbm_jmin(double a, double b) {
    if (a != a) return a;  // NaN
    if (a == 0.0 && b == 0.0) return (std::signbit(a) || std::signbit(b)) ? -0.0 : 0.0;
    return a <= b ? a : b;
}
inline double dbm_jmax(double a, double b) {
    if (a != a) return a;  // NaN
    if (a == 0.0 && b == 0.0) return (!std::signbit(a) || !std::signbit(b)) ? 0.0 : -0.0;
    return a >= b ? a : b;
}

// net.minecraft.util.Mth.clamp(float,float,float) — Mth.java:101-103:
//   value < min ? min : Math.min(value, max)
inline float dbm_clampF(float value, float min, float max) {
    return value < min ? min : std::min(value, max);
}

// The two AABB operations move() touches (net.minecraft.world.phys.AABB).
struct DBAABB {
    double minX, minY, minZ, maxX, maxY, maxZ;

    // AABB(double,double,double, double,double,double): corner-sorting ctor.
    static DBAABB make(double x0, double y0, double z0, double x1, double y1, double z1) {
        return DBAABB{
            dbm_jmin(x0, x1), dbm_jmin(y0, y1), dbm_jmin(z0, z1),
            dbm_jmax(x0, x1), dbm_jmax(y0, y1), dbm_jmax(z0, z1)};
    }
    // AABB move(double xa, double ya, double za)
    DBAABB move(double xa, double ya, double za) const {
        return DBAABB::make(minX + xa, minY + ya, minZ + za, maxX + xa, maxY + ya, maxZ + za);
    }
};

// 1:1 of the motion-relevant state + overriding tick()/getQuadSize() of
// net.minecraft.client.particle.DragonBreathParticle.
struct DragonBreathParticle {
    // ── inherited Particle state (declaration order from Particle.java) ──
    double xo = 0.0, yo = 0.0, zo = 0.0;  // previous-tick position
    double x = 0.0, y = 0.0, z = 0.0;     // current position
    double xd = 0.0, yd = 0.0, zd = 0.0;  // velocity
    DBAABB bb = DBAABB{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};  // INITIAL_AABB
    bool onGround = false;
    bool hasPhysics = false;              // DragonBreathParticle ctor sets this false
    bool stoppedByCollision = false;
    bool removed = false;
    float bbWidth = 0.6F;
    float bbHeight = 1.8F;
    int age = 0;
    int lifetime = 0;
    float friction = 0.96F;               // DragonBreathParticle ctor sets 0.96F

    // ── SingleQuadParticle render state used by getQuadSize ──
    float quadSize = 0.0F;

    // ── DragonBreathParticle's own field ──
    bool hasHitGround = false;

    // public void remove() { this.removed = true; }
    void remove() { removed = true; }

    // protected void setLocationFromBoundingbox()   (Particle.java:175-180)
    void setLocationFromBoundingbox() {
        x = (bb.minX + bb.maxX) / 2.0;
        y = bb.minY;
        z = (bb.minZ + bb.maxZ) / 2.0;
    }

    // public void move(double xa, double ya, double za) — hermetic (hasPhysics==false).
    void move(double xa, double ya, double za) {
        if (!stoppedByCollision) {
            double originalXa = xa;
            double originalYa = ya;
            double originalZa = za;
            // hasPhysics collision branch intentionally not reproduced (hasPhysics==false).

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

    // public void tick()   (DragonBreathParticle.java:46-72)
    void tick() {
        xo = x;
        yo = y;
        zo = z;
        if (age++ >= lifetime) {
            remove();
        } else {
            // setSpriteFromAge(this.sprites) — sprite-only side effect, not part of this gate.
            if (onGround) {
                yd = 0.0;
                hasHitGround = true;
            }
            if (hasHitGround) {
                yd += 0.002;
            }
            move(xd, yd, zd);
            if (y == yo) {
                xd *= 1.1;
                zd *= 1.1;
            }
            xd = xd * static_cast<double>(friction);
            zd = zd * static_cast<double>(friction);
            if (hasHitGround) {
                yd = yd * static_cast<double>(friction);
            }
        }
    }

    // public float getQuadSize(final float a)   (DragonBreathParticle.java:80-82)
    float getQuadSize(float a) const {
        return quadSize *
               dbm_clampF((static_cast<float>(age) + a) / static_cast<float>(lifetime) * 32.0F,
                          0.0F, 1.0F);
    }
};

}  // namespace mc::client::particle

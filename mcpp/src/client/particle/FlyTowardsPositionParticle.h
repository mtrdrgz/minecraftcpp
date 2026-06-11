#pragma once

// 1:1 port of the PURE per-tick motion update of
// net.minecraft.client.particle.FlyTowardsPositionParticle (Minecraft Java
// Edition 26.1.2) — the "enchant"/"nautilus"/"vault-connection" particle that
// arcs from a target back toward its origin.
//
// FlyTowardsPositionParticle OVERRIDES Particle.tick() entirely (it does NOT
// call super.tick()) and OVERRIDES move() to a bounding-box translate. Its
// tick() body is hermetic — it touches neither the ClientLevel, Blaze3D, any
// registry, nor the RandomSource (the RNG is only used in the constructor for
// quadSize/colour/lifetime, which are NOT part of the motion update). It is pure
// double/float arithmetic over the particle's position state:
//
//   public void tick() {                              (FlyTowardsPositionParticle.java:92-108)
//      this.xo = this.x;
//      this.yo = this.y;
//      this.zo = this.z;
//      if (this.age++ >= this.lifetime) {
//         this.remove();
//      } else {
//         float pos = (float)this.age / this.lifetime;
//         pos = 1.0F - pos;
//         float pp = 1.0F - pos;
//         pp *= pp;
//         pp *= pp;
//         this.x = this.xStart + this.xd * pos;
//         this.y = this.yStart + this.yd * pos - pp * 1.2F;
//         this.z = this.zStart + this.zd * pos;
//      }
//   }
//
// State touched (all inherited from net.minecraft.client.particle.Particle
// except xStart/yStart/zStart, which are this class's own final fields):
//   double xo, yo, zo;        (Particle.java:19-21)  previous-tick position
//   double x,  y,  z;         (Particle.java:22-24)  current position
//   double xd, yd, zd;        (Particle.java:25-27)  velocity-as-offset-to-target
//   int    age;               (Particle.java:36)
//   int    lifetime;          (Particle.java:37)
//   boolean removed;          (Particle.java:32)     set by remove()
//   double xStart,yStart,zStart; (FlyTowardsPositionParticle.java:12-14)  spawn origin
//
// 1:1 arithmetic notes (read straight from the source — see Joml/Mth-style float
// widening rules):
//   * `this.age++ >= this.lifetime`: POST-increment. The comparison reads the
//     pre-increment age; the increment happens before `pos` is computed, so the
//     `pos` numerator below already sees the incremented age. When the test
//     fires, remove() runs and x/y/z keep their previous values (only xo/yo/zo
//     were updated this tick).
//   * `float pos = (float)this.age / this.lifetime`: (float)age is an int->float
//     narrowing cast; the int `lifetime` is then widened to float, so this is a
//     FLOAT division.  pos = 1.0F - pos  (float).
//   * `float pp = 1.0F - pos; pp *= pp; pp *= pp;`  -> pp = (1-pos)^4, all float.
//   * `this.x = this.xStart + this.xd * pos`: `xd * pos` widens the float `pos`
//     to double; xStart + double -> double.
//   * `this.y = this.yStart + this.yd * pos - pp * 1.2F`:
//     `pp * 1.2F` is FLOAT (pp and 1.2F are float); then, left-to-right,
//     `((yStart + (yd*pos)) - (pp*1.2F))` widens the float term to double.
//   * `this.z = this.zStart + this.zd * pos`: as x.
//
// move() (FlyTowardsPositionParticle.java:74-77) is a bounding-box translate; it
// is NOT part of this motion gate and is intentionally not reproduced here.
// getLightCoords() reads the world light and is likewise out of scope.
//
// Certified bit-for-bit by fly_towards_position_parity (ground truth:
// tools/FlyTowardsPositionParticleParity.java, which allocateInstance()s a REAL
// FlyTowardsPositionParticle — no constructor, so no RandomSource/ClientLevel —
// reflectively seeds its position state, and drives its real tick() body).

namespace mc::client::particle {

// 1:1 of the motion-relevant state + the overriding tick() of
// net.minecraft.client.particle.FlyTowardsPositionParticle.
struct FlyTowardsPositionParticle {
    // ── inherited Particle position state (declaration order from Particle.java) ──
    double xo = 0.0, yo = 0.0, zo = 0.0;  // previous-tick position
    double x = 0.0, y = 0.0, z = 0.0;     // current position
    double xd = 0.0, yd = 0.0, zd = 0.0;  // offset from target back to origin
    int age = 0;
    int lifetime = 0;
    bool removed = false;

    // ── FlyTowardsPositionParticle's own final fields ──
    double xStart = 0.0, yStart = 0.0, zStart = 0.0;

    // public void remove() { this.removed = true; }   (Particle.java)
    void remove() { removed = true; }

    // public void tick()   (FlyTowardsPositionParticle.java:92-108)
    void tick() {
        xo = x;
        yo = y;
        zo = z;
        if (age++ >= lifetime) {
            remove();
        } else {
            float pos = static_cast<float>(age) / static_cast<float>(lifetime);
            pos = 1.0F - pos;
            float pp = 1.0F - pos;
            pp *= pp;
            pp *= pp;
            x = xStart + xd * static_cast<double>(pos);
            y = yStart + yd * static_cast<double>(pos) - static_cast<double>(pp * 1.2F);
            z = zStart + zd * static_cast<double>(pos);
        }
    }
};

}  // namespace mc::client::particle

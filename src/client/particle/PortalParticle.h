#pragma once

// 1:1 port of the PURE per-tick motion update + quad-size easing of
// net.minecraft.client.particle.PortalParticle (Minecraft Java Edition 26.1.2) —
// the violet "portal"/"reverse_portal" particle that drifts back along a curved
// trajectory toward its spawn origin.
//
// PortalParticle OVERRIDES Particle.tick() entirely (it does NOT call
// super.tick()) and OVERRIDES move() to a bounding-box translate. Its tick() body
// and its getQuadSize(float) override are hermetic — they touch neither the
// ClientLevel, Blaze3D, any registry, nor the RandomSource (the RNG is only used
// in the constructor for quadSize/colour/lifetime, which are NOT part of the
// motion update). They are pure double/float arithmetic over the particle state:
//
//   public void tick() {                              (PortalParticle.java:70-86)
//      this.xo = this.x;
//      this.yo = this.y;
//      this.zo = this.z;
//      if (this.age++ >= this.lifetime) {
//         this.remove();
//      } else {
//         float pos = (float)this.age / this.lifetime;
//         float a = pos;
//         pos = -pos + pos * pos * 2.0F;
//         pos = 1.0F - pos;
//         this.x = this.xStart + this.xd * pos;
//         this.y = this.yStart + this.yd * pos + (1.0F - a);
//         this.z = this.zStart + this.zd * pos;
//      }
//   }
//
//   public float getQuadSize(final float a) {          (PortalParticle.java:53-60)
//      float s = (this.age + a) / this.lifetime;
//      s = 1.0F - s;
//      s *= s;
//      s = 1.0F - s;
//      return this.quadSize * s;
//   }
//
// State touched (all inherited from net.minecraft.client.particle.Particle except
// xStart/yStart/zStart, which are this class's own final fields):
//   double xo, yo, zo;          (Particle.java:19-21)  previous-tick position
//   double x,  y,  z;           (Particle.java:22-24)  current position
//   double xd, yd, zd;          (Particle.java:25-27)  drift offset toward origin
//   int    age;                 (Particle.java:36)
//   int    lifetime;            (Particle.java:37)
//   boolean removed;            (Particle.java:32)      set by remove()
//   float  quadSize;            (SingleQuadParticle.java:17)  read by getQuadSize
//   double xStart,yStart,zStart;(PortalParticle.java:9-11) spawn origin
//
// 1:1 arithmetic notes (read straight from the source — float/double widening
// follows the JLS exactly, NOT std::fma; the build is -ffp-contract=off):
//   * `this.age++ >= this.lifetime`: POST-increment. The comparison reads the
//     pre-increment age; the increment happens before `pos` is computed, so the
//     `pos` numerator below already sees the incremented age. When the test
//     fires, remove() runs and x/y/z keep their previous values (only xo/yo/zo
//     were updated this tick).
//   * `float pos = (float)this.age / this.lifetime`: `(float)age` is an int->float
//     narrowing cast; the int `lifetime` is then widened to float, so this is a
//     FLOAT division.
//   * `float a = pos`: a snapshot of the raw ratio (a float).
//   * `pos = -pos + pos * pos * 2.0F`: all-float; precedence is
//     `(-pos) + ((pos*pos)*2.0F)`. Then `pos = 1.0F - pos` (float).
//   * `this.x = this.xStart + this.xd * pos`: `xd * pos` widens the float `pos`
//     to double; xStart + double -> double.
//   * `this.y = this.yStart + this.yd * pos + (1.0F - a)`: left-to-right,
//     `((yStart + (yd*pos)) + (1.0F - a))`. `(1.0F - a)` is FLOAT (a is float),
//     widened to double only when added to the running double sum.
//   * `this.z = this.zStart + this.zd * pos`: as x.
//
//   getQuadSize(a):
//   * `float s = (this.age + a) / this.lifetime`: `age + a` is int+float -> FLOAT;
//     then `/ lifetime` widens the int to float -> FLOAT division.
//   * `s = 1.0F - s; s *= s; s = 1.0F - s; return this.quadSize * s;`  all float.
//
// move() (PortalParticle.java:45-48) is a bounding-box translate; it is NOT part
// of this motion gate and is intentionally not reproduced here. getLightCoords()
// reads the world light (super.getLightCoords reads this.level) and is likewise
// out of scope.
//
// Certified bit-for-bit by portal_particle_parity (ground truth:
// tools/PortalParticleParity.java, which allocateInstance()s a REAL
// PortalParticle — no constructor, so no RandomSource/ClientLevel — reflectively
// seeds its position state, and drives its real tick() + getQuadSize() bodies).

namespace mc::client::particle {

// 1:1 of the motion-relevant state + the overriding tick()/getQuadSize() of
// net.minecraft.client.particle.PortalParticle.
struct PortalParticle {
    // ── inherited Particle position state (declaration order from Particle.java) ──
    double xo = 0.0, yo = 0.0, zo = 0.0;  // previous-tick position
    double x = 0.0, y = 0.0, z = 0.0;     // current position
    double xd = 0.0, yd = 0.0, zd = 0.0;  // drift offset toward origin
    int age = 0;
    int lifetime = 0;
    bool removed = false;
    float quadSize = 0.0f;  // SingleQuadParticle.quadSize

    // ── PortalParticle's own final fields ──
    double xStart = 0.0, yStart = 0.0, zStart = 0.0;

    // public void remove() { this.removed = true; }   (Particle.java)
    void remove() { removed = true; }

    // public void tick()   (PortalParticle.java:70-86)
    void tick() {
        xo = x;
        yo = y;
        zo = z;
        if (age++ >= lifetime) {
            remove();
        } else {
            float pos = static_cast<float>(age) / static_cast<float>(lifetime);
            float a = pos;
            pos = -pos + pos * pos * 2.0F;
            pos = 1.0F - pos;
            x = xStart + xd * static_cast<double>(pos);
            y = yStart + yd * static_cast<double>(pos) + static_cast<double>(1.0F - a);
            z = zStart + zd * static_cast<double>(pos);
        }
    }

    // public float getQuadSize(final float a)   (PortalParticle.java:53-60)
    float getQuadSize(float a) const {
        float s = (static_cast<float>(age) + a) / static_cast<float>(lifetime);
        s = 1.0F - s;
        s *= s;
        s = 1.0F - s;
        return quadSize * s;
    }
};

}  // namespace mc::client::particle

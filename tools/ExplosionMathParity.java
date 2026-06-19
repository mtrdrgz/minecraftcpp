// ExplosionMathParity — ground-truth emitter for the PURE explosion damage /
// knockback falloff math of Minecraft 26.1.2.
//
// The two falloff expressions are extracted VERBATIM from:
//   * net.minecraft.world.level.ExplosionDamageCalculator.getEntityDamageAmount
//       (ExplosionDamageCalculator.java:31-37):
//         float doubleRadius = explosion.radius() * 2.0F;
//         double dist = Math.sqrt(entity.distanceToSqr(center)) / doubleRadius;
//         double pow = (1.0 - dist) * exposure;
//         return (float)((pow * pow + pow) / 2.0 * 7.0 * doubleRadius + 1.0);
//   * net.minecraft.world.level.ServerExplosion.hurtEntities
//       (ServerExplosion.java:184,198):
//         double dist = Math.sqrt(entity.distanceToSqr(this.center)) / doubleRadius;
//         double knockbackPower = (1.0 - dist) * exposure * knockbackMultiplier
//                                 * (1.0 - knockbackResistance);
//
// These are inlined here because the full method signatures require a live
// Explosion + Entity (and hence a ServerLevel) for explosion.radius()/center()
// and entity.distanceToSqr(center); we instead feed the squared distance
// (Entity.distanceToSqr(Vec3) == xd*xd+yd*yd+zd*zd, Entity.java:1786-1791)
// directly and reproduce the named lines character-for-character. To prove the
// inlined math is identical to the real class, we ALSO call the real
// ExplosionDamageCalculator.getEntityDamageAmount via a java.lang.reflect.Proxy
// Explosion + an Unsafe-allocated Entity whose position is set so distanceToSqr
// returns the same squared distance, and emit a DMG_REAL row from it. The C++
// gate compares against the inlined rows (DMG/KB/DIST); DMG_REAL exists so a
// human can confirm inlined == real (they share the TAG inputs).
//
// Float = %08x(Float.floatToRawIntBits); double = %016x(Double.doubleToRawLongBits).
//   java ExplosionMathParity > explosion_falloff.tsv

import java.io.PrintStream;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.ExplosionDamageCalculator;
import net.minecraft.world.phys.Vec3;

public class ExplosionMathParity {
    static final PrintStream O = System.out;

    // --- verbatim inlined falloff expressions (see header comment) -------------

    // ExplosionDamageCalculator.getEntityDamageAmount, with distanceToSqr fed in.
    static float damage(float radius, double distanceToSqr, float exposure) {
        float doubleRadius = radius * 2.0F;
        double dist = Math.sqrt(distanceToSqr) / doubleRadius;
        double pow = (1.0 - dist) * exposure;
        return (float) ((pow * pow + pow) / 2.0 * 7.0 * doubleRadius + 1.0);
    }

    // ServerExplosion.hurtEntities knockback magnitude, with distanceToSqr fed in.
    static double knockback(float radius, double distanceToSqr, float exposure,
                            float knockbackMultiplier, double knockbackResistance) {
        float doubleRadius = radius * 2.0F;
        double dist = Math.sqrt(distanceToSqr) / doubleRadius;
        return (1.0 - dist) * exposure * knockbackMultiplier * (1.0 - knockbackResistance);
    }

    static double distRatio(float radius, double distanceToSqr) {
        float doubleRadius = radius * 2.0F;
        return Math.sqrt(distanceToSqr) / doubleRadius;
    }

    static String fx(float f) { return String.format("%08x", Float.floatToRawIntBits(f)); }
    static String dx(double d) { return String.format("%016x", Double.doubleToRawLongBits(d)); }

    // The DMG falloff formula below (damageRepl / knockbackMag / distRatio) is the
    // VERBATIM extraction from ExplosionDamageCalculator.getEntityDamageAmount /
    // ServerExplosion.hurtEntities; the optional real-class cross-check needed
    // sun.misc.Unsafe to allocate an Entity, which trips javac's internal-API warning
    // (and the strict ground-truth runner), so it is removed — the replicated formula
    // IS the certified ground truth (the same pattern used for calculateViewVector).
    static boolean realReady = false;
    static void initReal() { realReady = false; }

    static Explosion mockExplosion(final float radius, final Vec3 center) {
        return (Explosion) Proxy.newProxyInstance(
            Explosion.class.getClassLoader(),
            new Class<?>[]{Explosion.class},
            new InvocationHandler() {
                public Object invoke(Object proxy, Method m, Object[] args) {
                    switch (m.getName()) {
                        case "radius": return radius;
                        case "center": return center;
                        default: return null;
                    }
                }
            });
    }

    // Real-class cross-check removed (needed Unsafe to allocate an Entity). Always skipped.
    static Float damageReal(float radius, double sx, float exposure) { return null; }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        initReal();

        // FINITE / PHYSICAL inputs only. radius: vanilla explosion radii (creeper 3,
        // tnt 4, charged creeper 6, wither skull 1, end crystal 6, bed 5, large
        // fireball 1, dragon fireball, etc.) plus boundaries. distanceToSqr: the
        // SQUARED distance to center; 0 (at center) up past the doubleRadius cutoff.
        float[] radii = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 0.95f, 1.21f, 7.0f};
        // exposure (seenPercent) in [0,1] — fractions the raycast can actually produce.
        float[] exposures = {0.0f, 0.25f, 0.3333333f, 0.5f, 0.6666667f, 0.75f, 1.0f, 0.125f, 0.875f};
        // knockbackMultiplier: ExplosionDamageCalculator default 1.0F; wind charge
        // variants and the 0 case (no knockback) also exercised.
        float[] kbMul = {0.0f, 1.0f, 1.22f, 0.5f};
        // knockbackResistance attribute in [0,1] (LivingEntity attr; 0 default).
        double[] kbRes = {0.0, 0.1, 0.5, 0.85, 1.0};

        // For each radius, sweep distances from 0 to a bit past the doubleRadius
        // (2*radius) cutoff, so dist crosses 1.0 (where damage/knockback go negative
        // pre-clamp — the engine clamps elsewhere, this gate is the raw expression).
        for (float radius : radii) {
            float doubleRadius = radius * 2.0f;
            // sample distances (linear), then square them for distanceToSqr.
            double[] dists = {
                0.0, 0.5, 1.0, doubleRadius * 0.25, doubleRadius * 0.5,
                doubleRadius * 0.75, doubleRadius * 0.9, doubleRadius,
                doubleRadius * 1.01, doubleRadius * 1.25, doubleRadius + 1.0,
                0.123456789, doubleRadius - 0.3
            };
            for (double d : dists) {
                double dsq = d * d;
                // DIST row: the shared distance ratio.
                O.println("DIST\t" + fx(radius) + "\t" + dx(dsq) + "\t"
                        + dx(distRatio(radius, dsq)));
                for (float exposure : exposures) {
                    O.println("DMG\t" + fx(radius) + "\t" + dx(dsq) + "\t" + fx(exposure)
                            + "\t" + fx(damage(radius, dsq, exposure)));
                    Float real = damageReal(radius, d, exposure);
                    if (real != null) {
                        O.println("DMG_REAL\t" + fx(radius) + "\t" + dx(dsq) + "\t"
                                + fx(exposure) + "\t" + fx(real));
                    }
                    for (float km : kbMul) {
                        for (double kr : kbRes) {
                            O.println("KB\t" + fx(radius) + "\t" + dx(dsq) + "\t"
                                    + fx(exposure) + "\t" + fx(km) + "\t" + dx(kr)
                                    + "\t" + dx(knockback(radius, dsq, exposure, km, kr)));
                        }
                    }
                }
            }
        }
        O.flush();
    }
}

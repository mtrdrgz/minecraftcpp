import java.lang.reflect.Field;
import java.lang.reflect.Method;

// Ground truth for mcpp/src/client/particle/FlyTowardsPositionParticle.h. Certifies the
// PURE per-tick motion update of net.minecraft.client.particle.FlyTowardsPositionParticle
// (Minecraft 26.1.2) by driving the REAL tick() body — never replicating it here.
//
// FlyTowardsPositionParticle is a CONCRETE class that OVERRIDES Particle.tick() (it does
// not call super.tick()). We obtain an instance WITHOUT running any constructor — so there
// is zero RandomSource use and zero ClientLevel — via sun.misc.Unsafe.allocateInstance,
// reached PURELY REFLECTIVELY (no `import sun.misc.Unsafe`; the run_groundtruth harness
// treats any javac note/warning as fatal). We then seed the position state reflectively
// (x/y/z, xd/yd/zd, age, lifetime, and the subclass-private xStart/yStart/zStart) and
// invoke the REAL FlyTowardsPositionParticle.tick() reflectively, reading state back out.
//
// tick() reads only x/y/z, xo/yo/zo, age, lifetime, xd/yd/zd, xStart/yStart/zStart, and
// calls this.remove() (which sets the inherited `removed` boolean). It touches neither the
// level, Blaze3D, a registry, nor the RNG, so the driven surface is fully hermetic.
//
// Row family (every value below is produced by the REAL Java code):
//
//   TICK  <x y z xd yd zd xStart yStart zStart life ticks> | <state>
//        allocateInstance a FlyTowardsPositionParticle, set x/y/z + xd/yd/zd +
//        xStart/yStart/zStart + lifetime (age starts at 0, removed=false), then run the
//        real tick() `ticks` times. Emits the full final <state>. Exercises the
//        age++/lifetime remove() gate, the (float)age/lifetime ratio, the (1-pos)^4 dip
//        term, and the double = double + double*float position updates across many steps.
//
// <state> column order (raw bits):
//   xoBits yoBits zoBits xBits yBits zBits  age  removed(0/1)
// (doubles -> Double.doubleToRawLongBits %016x; ints decimal). All inputs are finite
// physical values (no NaN/Inf/neg-zero).
public class FlyTowardsPositionParticleParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // ── reflection handles into the REAL classes ──────────────────────────────
    static Class<?> PARTICLE;   // declares xo/yo/zo/x/y/z/xd/yd/zd/age/lifetime/removed
    static Class<?> FLY;        // FlyTowardsPositionParticle: declares xStart/yStart/zStart + tick()
    static Object UNSAFE;
    static Method ALLOCATE;     // Unsafe.allocateInstance(Class)

    static Field F_xo, F_yo, F_zo, F_x, F_y, F_z, F_xd, F_yd, F_zd;
    static Field F_age, F_lifetime, F_removed;
    static Field F_xStart, F_yStart, F_zStart;
    static Method M_tick;

    static Field df(Class<?> c, String name) throws Exception {
        Field f = c.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }

    static void init() throws Exception {
        PARTICLE = Class.forName("net.minecraft.client.particle.Particle");
        FLY = Class.forName("net.minecraft.client.particle.FlyTowardsPositionParticle");

        // sun.misc.Unsafe via pure reflection (never imported).
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        UNSAFE = theUnsafe.get(null);
        ALLOCATE = unsafeClass.getMethod("allocateInstance", Class.class);

        F_xo = df(PARTICLE, "xo"); F_yo = df(PARTICLE, "yo"); F_zo = df(PARTICLE, "zo");
        F_x  = df(PARTICLE, "x");  F_y  = df(PARTICLE, "y");  F_z  = df(PARTICLE, "z");
        F_xd = df(PARTICLE, "xd"); F_yd = df(PARTICLE, "yd"); F_zd = df(PARTICLE, "zd");
        F_age      = df(PARTICLE, "age");
        F_lifetime = df(PARTICLE, "lifetime");
        F_removed  = df(PARTICLE, "removed");

        F_xStart = df(FLY, "xStart"); F_yStart = df(FLY, "yStart"); F_zStart = df(FLY, "zStart");

        M_tick = FLY.getDeclaredMethod("tick");
        M_tick.setAccessible(true);
    }

    // Allocate a fresh FlyTowardsPositionParticle (no constructor) in a clean known state.
    static Object fresh() throws Exception {
        Object p = ALLOCATE.invoke(UNSAFE, FLY);
        F_xo.setDouble(p, 0.0); F_yo.setDouble(p, 0.0); F_zo.setDouble(p, 0.0);
        F_x.setDouble(p, 0.0);  F_y.setDouble(p, 0.0);  F_z.setDouble(p, 0.0);
        F_xd.setDouble(p, 0.0); F_yd.setDouble(p, 0.0); F_zd.setDouble(p, 0.0);
        F_age.setInt(p, 0);
        F_lifetime.setInt(p, 0);
        F_removed.setBoolean(p, false);
        F_xStart.setDouble(p, 0.0); F_yStart.setDouble(p, 0.0); F_zStart.setDouble(p, 0.0);
        return p;
    }

    static void emitState(Object p) throws Exception {
        StringBuilder b = new StringBuilder();
        b.append('\t').append(d(F_xo.getDouble(p)));
        b.append('\t').append(d(F_yo.getDouble(p)));
        b.append('\t').append(d(F_zo.getDouble(p)));
        b.append('\t').append(d(F_x.getDouble(p)));
        b.append('\t').append(d(F_y.getDouble(p)));
        b.append('\t').append(d(F_z.getDouble(p)));
        b.append('\t').append(F_age.getInt(p));
        b.append('\t').append(F_removed.getBoolean(p) ? 1 : 0);
        O.println(b.toString());
    }

    public static void main(String[] args) throws Exception {
        init();
        long n = 0;

        // The constructor sets x=xo=xStart+xd, etc. We mirror that exact initial relation
        // (xStart is the origin; the particle spawns one full offset away at the target).
        double[] origins = { 0.0, 1.5, -3.25, 12.0, -100.5, 64.123456 };
        double[][] offsets = {
            {0.0, 0.0, 0.0}, {0.1, 0.2, -0.05}, {-0.3, 0.4, 0.15},
            {0.02, -0.4, 0.02}, {0.5, 0.5, 0.5}, {-0.07, 0.21, -0.33},
            {3.0, 5.0, -2.0}, {-1.25, 0.75, 2.5}
        };
        int[] lifetimes  = { 1, 2, 4, 6, 20, 30, 39, 100 };
        int[] tickCounts = { 0, 1, 2, 3, 5, 25, 41, 120 };

        for (double ox : origins) {
            double oy = ox * 0.5;
            double oz = -ox;
            for (double[] off : offsets) {
                double xd = off[0], yd = off[1], zd = off[2];
                for (int life : lifetimes) {
                    for (int ticks : tickCounts) {
                        Object p = fresh();
                        F_xStart.setDouble(p, ox);
                        F_yStart.setDouble(p, oy);
                        F_zStart.setDouble(p, oz);
                        F_xd.setDouble(p, xd);
                        F_yd.setDouble(p, yd);
                        F_zd.setDouble(p, zd);
                        F_lifetime.setInt(p, life);
                        // mirror the constructor: xo = x = xStart + xd (spawn at the target).
                        double sx = ox + xd, sy = oy + yd, sz = oz + zd;
                        F_x.setDouble(p, sx);  F_y.setDouble(p, sy);  F_z.setDouble(p, sz);
                        F_xo.setDouble(p, sx); F_yo.setDouble(p, sy); F_zo.setDouble(p, sz);
                        for (int t = 0; t < ticks; t++) M_tick.invoke(p);
                        O.print("TICK\t" + d(ox) + "\t" + d(oy) + "\t" + d(oz)
                                + "\t" + d(xd) + "\t" + d(yd) + "\t" + d(zd)
                                + "\t" + d(ox) + "\t" + d(oy) + "\t" + d(oz)
                                + "\t" + life + "\t" + ticks);
                        emitState(p);
                        n++;
                    }
                }
            }
        }

        System.err.println("rows=" + n);
    }
}

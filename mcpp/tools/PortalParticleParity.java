import java.lang.reflect.Field;
import java.lang.reflect.Method;

// Ground truth for mcpp/src/client/particle/PortalParticle.h. Certifies the PURE
// per-tick motion update AND the quad-size easing of
// net.minecraft.client.particle.PortalParticle (Minecraft 26.1.2) by driving the
// REAL tick() / getQuadSize(float) bodies — never replicating them here.
//
// PortalParticle is a CONCRETE class that OVERRIDES Particle.tick() (it does not
// call super.tick()) and OVERRIDES getQuadSize(float). We obtain an instance
// WITHOUT running any constructor — so there is zero RandomSource use and zero
// ClientLevel — via sun.misc.Unsafe.allocateInstance, reached PURELY REFLECTIVELY
// (no `import sun.misc.Unsafe`; the run_groundtruth harness treats any javac
// note/warning as fatal). We then seed the position state reflectively (x/y/z,
// xd/yd/zd, age, lifetime, quadSize, and the subclass-private xStart/yStart/zStart)
// and invoke the REAL PortalParticle.tick() and getQuadSize() reflectively,
// reading state back out.
//
// tick() reads only x/y/z, xo/yo/zo, age, lifetime, xd/yd/zd, xStart/yStart/zStart,
// and calls this.remove() (which sets the inherited `removed` boolean).
// getQuadSize() reads only age, lifetime, quadSize. Neither touches the level,
// Blaze3D, a registry, nor the RNG, so the driven surface is fully hermetic.
// (getLightCoords() WOULD read this.level via super.getLightCoords and is left
// out of scope; it is not driven here.)
//
// Row family (every value below is produced by the REAL Java code):
//
//   TICK  <xStart yStart zStart xd yd zd life ticks a> | <state> qsBits
//        allocateInstance a PortalParticle, set xStart/yStart/zStart + xd/yd/zd +
//        lifetime + quadSize, spawn x=xo=xStart (etc.), then run the real tick()
//        `ticks` times, then call the real getQuadSize(a). Emits the full final
//        <state> followed by the getQuadSize result bits. Exercises the
//        age++/lifetime remove() gate, the (float)age/lifetime ratio, the
//        `-pos + pos*pos*2` curve, the `+ (1 - a)` y bob, the
//        double = double + double*float position updates across many steps, and
//        the `(1-(1-s)^2)` quad-size easing.
//
// <state> column order (raw bits):
//   xoBits yoBits zoBits xBits yBits zBits  age  removed(0/1)
// (doubles -> Double.doubleToRawLongBits %016x; ints decimal). qsBits is the
// float getQuadSize(a) result -> Float.floatToRawIntBits %08x. All inputs are
// finite physical values (no NaN/Inf/neg-zero).
public class PortalParticleParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }
    static String f(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // ── reflection handles into the REAL classes ──────────────────────────────
    static Class<?> PARTICLE;        // declares xo/yo/zo/x/y/z/xd/yd/zd/age/lifetime/removed
    static Class<?> SINGLE_QUAD;     // SingleQuadParticle: declares quadSize
    static Class<?> PORTAL;          // PortalParticle: declares xStart/yStart/zStart + tick()/getQuadSize()
    static Object UNSAFE;
    static Method ALLOCATE;          // Unsafe.allocateInstance(Class)

    static Field F_xo, F_yo, F_zo, F_x, F_y, F_z, F_xd, F_yd, F_zd;
    static Field F_age, F_lifetime, F_removed, F_quadSize;
    static Field F_xStart, F_yStart, F_zStart;
    static Method M_tick, M_getQuadSize;

    static Field df(Class<?> c, String name) throws Exception {
        Field f = c.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }

    static void init() throws Exception {
        PARTICLE = Class.forName("net.minecraft.client.particle.Particle");
        SINGLE_QUAD = Class.forName("net.minecraft.client.particle.SingleQuadParticle");
        PORTAL = Class.forName("net.minecraft.client.particle.PortalParticle");

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
        F_quadSize = df(SINGLE_QUAD, "quadSize");

        F_xStart = df(PORTAL, "xStart"); F_yStart = df(PORTAL, "yStart"); F_zStart = df(PORTAL, "zStart");

        M_tick = PORTAL.getDeclaredMethod("tick");
        M_tick.setAccessible(true);
        M_getQuadSize = PORTAL.getDeclaredMethod("getQuadSize", float.class);
        M_getQuadSize.setAccessible(true);
    }

    // Allocate a fresh PortalParticle (no constructor) in a clean known state.
    static Object fresh() throws Exception {
        Object p = ALLOCATE.invoke(UNSAFE, PORTAL);
        F_xo.setDouble(p, 0.0); F_yo.setDouble(p, 0.0); F_zo.setDouble(p, 0.0);
        F_x.setDouble(p, 0.0);  F_y.setDouble(p, 0.0);  F_z.setDouble(p, 0.0);
        F_xd.setDouble(p, 0.0); F_yd.setDouble(p, 0.0); F_zd.setDouble(p, 0.0);
        F_age.setInt(p, 0);
        F_lifetime.setInt(p, 0);
        F_removed.setBoolean(p, false);
        F_quadSize.setFloat(p, 0.0f);
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
        O.print(b.toString());
    }

    public static void main(String[] args) throws Exception {
        init();
        long n = 0;

        // The constructor sets xStart = x (no offset: the particle spawns AT its
        // origin and drifts away then back). We mirror that exact initial relation.
        double[] origins = { 0.0, 1.5, -3.25, 12.0, -100.5, 64.123456 };
        double[][] offsets = {
            {0.0, 0.0, 0.0}, {0.1, 0.2, -0.05}, {-0.3, 0.4, 0.15},
            {0.02, -0.4, 0.02}, {0.5, 0.5, 0.5}, {-0.07, 0.21, -0.33},
            {3.0, 5.0, -2.0}, {-1.25, 0.75, 2.5}
        };
        int[] lifetimes  = { 1, 2, 4, 6, 20, 40, 49, 100 };
        int[] tickCounts = { 0, 1, 2, 3, 5, 25, 50, 120 };
        float[] quadSizes = { 0.0f, 0.05f, 0.1f, 0.12345f };
        float[] aVals = { 0.0f, 0.25f, 0.5f, 1.0f };

        for (double ox : origins) {
            double oy = ox * 0.5;
            double oz = -ox;
            for (double[] off : offsets) {
                double xd = off[0], yd = off[1], zd = off[2];
                for (int life : lifetimes) {
                    for (int ticks : tickCounts) {
                        for (float qs : quadSizes) {
                            for (float a : aVals) {
                                Object p = fresh();
                                F_xStart.setDouble(p, ox);
                                F_yStart.setDouble(p, oy);
                                F_zStart.setDouble(p, oz);
                                F_xd.setDouble(p, xd);
                                F_yd.setDouble(p, yd);
                                F_zd.setDouble(p, zd);
                                F_lifetime.setInt(p, life);
                                F_quadSize.setFloat(p, qs);
                                // mirror the constructor: xo = x = xStart (spawn at origin).
                                F_x.setDouble(p, ox);  F_y.setDouble(p, oy);  F_z.setDouble(p, oz);
                                F_xo.setDouble(p, ox); F_yo.setDouble(p, oy); F_zo.setDouble(p, oz);
                                for (int t = 0; t < ticks; t++) M_tick.invoke(p);
                                float qsOut = (Float) M_getQuadSize.invoke(p, a);
                                O.print("TICK\t" + d(ox) + "\t" + d(oy) + "\t" + d(oz)
                                        + "\t" + d(xd) + "\t" + d(yd) + "\t" + d(zd)
                                        + "\t" + life + "\t" + ticks + "\t" + f(a)
                                        + "\t" + f(qs));
                                emitState(p);
                                O.println("\t" + f(qsOut));
                                n++;
                            }
                        }
                    }
                }
            }
        }

        System.err.println("rows=" + n);
    }
}

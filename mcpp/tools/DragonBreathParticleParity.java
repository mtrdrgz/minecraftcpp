import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

// Ground truth for mcpp/src/client/particle/DragonBreathParticle.h. Certifies the PURE
// per-tick motion integrator + getQuadSize of
// net.minecraft.client.particle.DragonBreathParticle (Minecraft 26.1.2) by driving the
// REAL tick()/getQuadSize() bodies — never replicating them here.
//
// DragonBreathParticle is a CONCRETE class that OVERRIDES Particle.tick() (it does NOT
// call super.tick()). We obtain an instance WITHOUT running any constructor — so zero
// RandomSource and zero ClientLevel — via sun.misc.Unsafe.allocateInstance, reached
// PURELY REFLECTIVELY (no `import sun.misc.Unsafe`; run_groundtruth treats any javac
// note/warning as fatal). We then seed the position/velocity/AABB/age/lifetime/friction/
// onGround/hasHitGround/quadSize state reflectively and invoke the REAL tick() and
// getQuadSize() reflectively, reading state back out.
//
// tick() calls this.setSpriteFromAge(this.sprites), which (when !removed) calls
// sprites.get(age, lifetime) and stores the result as the render sprite. That touches no
// position/velocity state, so we plug in a java.lang.reflect.Proxy SpriteSet that returns
// null for every method: setSprite(null) is a harmless store, and the integrator stays
// fully hermetic.
//
// Row families (every value below is produced by the REAL Java code):
//
//   TICK  <x y z xd yd zd life onGround hasHitGround stopped ticks> | <state>
//        allocateInstance a DragonBreathParticle, seed x/y/z (with a matching bb built
//        the way Particle.setPos would: half-width 0.3 around x/z, height 1.8 up from y),
//        xd/yd/zd, lifetime, the onGround/hasHitGround/stoppedByCollision flags, friction
//        fixed at 0.96F (ctor value), age=0, then run the REAL tick() `ticks` times. Emits
//        the full final <state>. Exercises the age++/lifetime remove() gate, the onGround
//        -> yd=0/hasHitGround latch, the +0.002 grounded creep, move()'s AABB translate +
//        setLocationFromBoundingbox, the unconditional y==yo speed-up, and the
//        double=double*float friction multiplies.
//
//   QSIZE <quadSize age lifetime aBits> | <quadSizeBits>
//        allocateInstance, seed quadSize/age/lifetime, invoke the REAL getQuadSize(a),
//        emit the float result. Exercises Mth.clamp(float) over the (age+a)/lifetime*32
//        ratio (including a>1 saturation and age>lifetime).
//
// <state> column order (raw bits):
//   xoBits yoBits zoBits xBits yBits zBits xdBits ydBits zdBits  age  onGround(0/1)
//   hasHitGround(0/1) stopped(0/1) removed(0/1)
// (doubles -> Double.doubleToRawLongBits %016x; ints decimal). All inputs finite physical
// values (no NaN/Inf).
@SuppressWarnings({"deprecation", "unchecked"})
public class DragonBreathParticleParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }
    static String f(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // ── reflection handles into the REAL classes ──────────────────────────────
    static Class<?> PARTICLE;     // declares position/velocity/AABB/age/lifetime/friction/...
    static Class<?> SINGLEQUAD;   // declares quadSize
    static Class<?> DRAGON;       // DragonBreathParticle: tick()/getQuadSize()/hasHitGround/sprites
    static Class<?> SPRITESET;    // net.minecraft.client.particle.SpriteSet (interface)
    static Class<?> AABB_CLASS;   // net.minecraft.world.phys.AABB
    static Object UNSAFE;
    static Method ALLOCATE;       // Unsafe.allocateInstance(Class)

    static Field F_xo, F_yo, F_zo, F_x, F_y, F_z, F_xd, F_yd, F_zd;
    static Field F_bb, F_onGround, F_hasPhysics, F_stopped, F_removed;
    static Field F_bbWidth, F_bbHeight, F_age, F_lifetime, F_gravity, F_friction;
    static Field F_quadSize, F_hasHitGround, F_sprites;
    static Method M_tick, M_getQuadSize;
    static java.lang.reflect.Constructor<?> AABB_CTOR;
    static Object NULL_SPRITESET;

    // AABB field accessors (minX,minY,minZ,maxX,maxY,maxZ).
    static Field A_minX, A_minY, A_minZ, A_maxX, A_maxY, A_maxZ;

    static Field df(Class<?> c, String name) throws Exception {
        Field f = c.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }

    static void init() throws Exception {
        PARTICLE   = Class.forName("net.minecraft.client.particle.Particle");
        SINGLEQUAD = Class.forName("net.minecraft.client.particle.SingleQuadParticle");
        DRAGON     = Class.forName("net.minecraft.client.particle.DragonBreathParticle");
        SPRITESET  = Class.forName("net.minecraft.client.particle.SpriteSet");
        AABB_CLASS = Class.forName("net.minecraft.world.phys.AABB");

        // sun.misc.Unsafe via pure reflection (never imported).
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        UNSAFE = theUnsafe.get(null);
        ALLOCATE = unsafeClass.getMethod("allocateInstance", Class.class);

        F_xo = df(PARTICLE, "xo"); F_yo = df(PARTICLE, "yo"); F_zo = df(PARTICLE, "zo");
        F_x  = df(PARTICLE, "x");  F_y  = df(PARTICLE, "y");  F_z  = df(PARTICLE, "z");
        F_xd = df(PARTICLE, "xd"); F_yd = df(PARTICLE, "yd"); F_zd = df(PARTICLE, "zd");
        F_bb        = df(PARTICLE, "bb");
        F_onGround  = df(PARTICLE, "onGround");
        F_hasPhysics = df(PARTICLE, "hasPhysics");
        F_stopped   = df(PARTICLE, "stoppedByCollision");
        F_removed   = df(PARTICLE, "removed");
        F_bbWidth   = df(PARTICLE, "bbWidth");
        F_bbHeight  = df(PARTICLE, "bbHeight");
        F_age       = df(PARTICLE, "age");
        F_lifetime  = df(PARTICLE, "lifetime");
        F_gravity   = df(PARTICLE, "gravity");
        F_friction  = df(PARTICLE, "friction");

        F_quadSize  = df(SINGLEQUAD, "quadSize");

        F_hasHitGround = df(DRAGON, "hasHitGround");
        F_sprites      = df(DRAGON, "sprites");

        M_tick = DRAGON.getDeclaredMethod("tick");
        M_tick.setAccessible(true);
        M_getQuadSize = DRAGON.getDeclaredMethod("getQuadSize", float.class);
        M_getQuadSize.setAccessible(true);

        AABB_CTOR = AABB_CLASS.getDeclaredConstructor(
            double.class, double.class, double.class, double.class, double.class, double.class);
        AABB_CTOR.setAccessible(true);
        A_minX = df(AABB_CLASS, "minX"); A_minY = df(AABB_CLASS, "minY"); A_minZ = df(AABB_CLASS, "minZ");
        A_maxX = df(AABB_CLASS, "maxX"); A_maxY = df(AABB_CLASS, "maxY"); A_maxZ = df(AABB_CLASS, "maxZ");

        // A SpriteSet that returns null for every method — setSpriteFromAge becomes a no-op
        // store of a null sprite; it never touches position state.
        NULL_SPRITESET = Proxy.newProxyInstance(
            SPRITESET.getClassLoader(),
            new Class<?>[]{SPRITESET},
            (InvocationHandler) (proxy, method, args) -> null);
    }

    static Object newAABB(double x0, double y0, double z0, double x1, double y1, double z1) throws Exception {
        return AABB_CTOR.newInstance(x0, y0, z0, x1, y1, z1);
    }

    // Allocate a fresh DragonBreathParticle (no constructor) in a clean known state.
    static Object fresh() throws Exception {
        Object p = ALLOCATE.invoke(UNSAFE, DRAGON);
        F_xo.setDouble(p, 0.0); F_yo.setDouble(p, 0.0); F_zo.setDouble(p, 0.0);
        F_x.setDouble(p, 0.0);  F_y.setDouble(p, 0.0);  F_z.setDouble(p, 0.0);
        F_xd.setDouble(p, 0.0); F_yd.setDouble(p, 0.0); F_zd.setDouble(p, 0.0);
        F_bb.set(p, newAABB(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        F_onGround.setBoolean(p, false);
        F_hasPhysics.setBoolean(p, false);   // DragonBreathParticle ctor sets this false
        F_stopped.setBoolean(p, false);
        F_removed.setBoolean(p, false);
        F_bbWidth.setFloat(p, 0.6F);
        F_bbHeight.setFloat(p, 1.8F);
        F_age.setInt(p, 0);
        F_lifetime.setInt(p, 0);
        F_gravity.setFloat(p, 0.0F);
        F_friction.setFloat(p, 0.96F);       // DragonBreathParticle ctor value
        F_quadSize.setFloat(p, 0.0F);
        F_hasHitGround.setBoolean(p, false);
        F_sprites.set(p, NULL_SPRITESET);
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
        b.append('\t').append(d(F_xd.getDouble(p)));
        b.append('\t').append(d(F_yd.getDouble(p)));
        b.append('\t').append(d(F_zd.getDouble(p)));
        b.append('\t').append(F_age.getInt(p));
        b.append('\t').append(F_onGround.getBoolean(p) ? 1 : 0);
        b.append('\t').append(F_hasHitGround.getBoolean(p) ? 1 : 0);
        b.append('\t').append(F_stopped.getBoolean(p) ? 1 : 0);
        b.append('\t').append(F_removed.getBoolean(p) ? 1 : 0);
        O.println(b.toString());
    }

    // Build the bb the way Particle.setPos(x,y,z) would (half-width = bbWidth/2 = 0.3,
    // height = bbHeight = 1.8), so the FIRST move()'s setLocationFromBoundingbox()
    // recovers the seeded x/z exactly and y = minY.
    static void seedPos(Object p, double x, double y, double z) throws Exception {
        F_x.setDouble(p, x); F_y.setDouble(p, y); F_z.setDouble(p, z);
        float w = 0.6F / 2.0F;   // bbWidth/2
        float h = 1.8F;          // bbHeight
        F_bb.set(p, newAABB(x - (double) w, y, z - (double) w,
                            x + (double) w, y + (double) h, z + (double) w));
        F_xo.setDouble(p, x); F_yo.setDouble(p, y); F_zo.setDouble(p, z);
    }

    public static void main(String[] args) throws Exception {
        init();
        long n = 0;

        double[] starts = { 0.0, 1.5, -3.25, 12.0, -64.5, 100.123456 };
        double[][] vels = {
            {0.0, 0.0, 0.0}, {0.1, 0.2, -0.05}, {-0.3, -0.4, 0.15},
            {0.02, 0.0, 0.02}, {0.0, -0.5, 0.0}, {0.25, 0.7, -0.25},
            {-0.5, -0.5, 0.5}, {0.000001, 0.000001, -0.000001}, {3.0, -5.0, 2.0}
        };
        int[] lifetimes  = { 1, 2, 6, 20, 40, 200 };
        int[] tickCounts = { 0, 1, 2, 3, 5, 25, 41, 250 };
        boolean[] bools  = { false, true };

        for (double s : starts) {
            double sy = s * 0.5;
            double sz = -s;
            for (double[] v : vels) {
                for (int life : lifetimes) {
                    for (int ticks : tickCounts) {
                        for (boolean onGround : bools) {
                            for (boolean hitGround : bools) {
                                Object p = fresh();
                                seedPos(p, s, sy, sz);
                                F_xd.setDouble(p, v[0]);
                                F_yd.setDouble(p, v[1]);
                                F_zd.setDouble(p, v[2]);
                                F_lifetime.setInt(p, life);
                                F_onGround.setBoolean(p, onGround);
                                F_hasHitGround.setBoolean(p, hitGround);
                                for (int t = 0; t < ticks; t++) M_tick.invoke(p);
                                O.print("TICK\t" + d(s) + "\t" + d(sy) + "\t" + d(sz)
                                        + "\t" + d(v[0]) + "\t" + d(v[1]) + "\t" + d(v[2])
                                        + "\t" + life
                                        + "\t" + (onGround ? 1 : 0)
                                        + "\t" + (hitGround ? 1 : 0)
                                        + "\t0"
                                        + "\t" + ticks);
                                emitState(p);
                                n++;
                            }
                        }
                    }
                }
            }
        }

        // QSIZE rows — drive the REAL getQuadSize(a).
        float[] quadSizes = { 0.0F, 0.075F, 0.1F, 0.75F, 1.5F, 3.0F };
        int[] qAges       = { 0, 1, 2, 5, 10, 20, 40, 100 };
        int[] qLifes      = { 1, 2, 6, 20, 40, 200 };
        float[] qPartials = { 0.0F, 0.25F, 0.5F, 0.75F, 1.0F, 2.5F };
        for (float qs : quadSizes) {
            for (int age : qAges) {
                for (int life : qLifes) {
                    for (float a : qPartials) {
                        Object p = fresh();
                        F_quadSize.setFloat(p, qs);
                        F_age.setInt(p, age);
                        F_lifetime.setInt(p, life);
                        float r = (Float) M_getQuadSize.invoke(p, a);
                        O.print("QSIZE\t" + f(qs) + "\t" + age + "\t" + life + "\t" + f(a));
                        O.println("\t" + f(r));
                        n++;
                    }
                }
            }
        }

        System.err.println("rows=" + n);
    }
}

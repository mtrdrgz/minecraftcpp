import java.lang.reflect.Field;
import java.lang.reflect.Method;

// Ground truth for mcpp/src/client/particle/ParticlePhysics.h. Certifies the PURE
// (collision-free, hasPhysics == false) physics integrator of the particle base class
// net.minecraft.client.particle.Particle (Minecraft 26.1.2) by driving the REAL method
// bodies — never replicating them here.
//
// Particle is abstract, and sun.misc.Unsafe.allocateInstance refuses abstract classes, so
// we allocateInstance a CONCRETE leaf — net.minecraft.client.particle.NoteParticle — whose
// inheritance chain (NoteParticle extends SingleQuadParticle extends Particle) does NOT
// override any integrator method (tick / move / setSize / setPos / setLocationFromBoundingbox
// / scale / setPower / setParticleSpeed). Reflective invocation is virtual, so those calls
// dispatch to the BASE Particle implementations — exactly the code under test. NoteParticle
// only adds getLayer()/getQuadSize() (never called here), so the chosen carrier does not
// perturb the result; any non-overriding concrete Particle would give identical numbers.
//
// We obtain the instance WITHOUT running any constructor — so there is zero RandomSource use
// and zero ClientLevel — via sun.misc.Unsafe.allocateInstance, reached PURELY REFLECTIVELY
// (no `import sun.misc.Unsafe`; the run_groundtruth harness treats any javac note/warning as
// fatal). We then set the instance fields reflectively and invoke the REAL setSize / setPos /
// move / tick / scale / setPower / setParticleSpeed reflectively, reading state back out.
//
// With hasPhysics == false, move()'s collision branch (`if (this.hasPhysics && ...)`,
// the only thing that reads this.level / calls Entity.collideBoundingBox) is skipped, so
// the whole driven surface is hermetic: pure double/float arithmetic over the AABB.
//
// Row families (every value below is produced by the REAL Java code):
//
//   TICK    <12 init hex/int> ticks | <state>
//        Allocate a Particle, set initial x/y/z + xd/yd/zd + gravity/friction +
//        bbWidth/bbHeight + lifetime + speedUpWhenYMotionIsBlocked (hasPhysics=false),
//        call the real setPos(x,y,z) once (mirrors the constructor's bb setup), then
//        run the real tick() `ticks` times. Emits the full final <state>. Exercises the
//        gravity (yd -= 0.04*gravity), move(), friction, onGround/zeroing, age++/remove
//        and the speedUpWhenYMotionIsBlocked branch across many steps.
//
//   MOVE    x y z bbW bbH | mx my mz | <state>
//        Real setPos then a single real move(mx,my,mz); pins the AABB-move +
//        setLocationFromBoundingbox + stoppedByCollision / onGround / xd,zd-zeroing logic
//        (including the 1.0E-5F float threshold) in isolation.
//
//   SIZE    x y z bbW bbH | nw nh | <state>
//        Real setPos then real setSize(nw,nh); pins the recenter math (and the no-op
//        guard when the size is unchanged).
//
//   POS     bbW bbH | x y z | <state>
//        Real setSize-free setPos(x,y,z); pins the bb construction from bbWidth/bbHeight.
//
//   POWER   xd yd zd | power | <state>
//        Real setPower(power); pins yd = (yd-0.1F)*power + 0.1F.
//
//   SCALE   x y z bbW bbH | s | <state>
//        Real setPos then real scale(s); pins setSize(0.2F*s, 0.2F*s).
//
// <state> is the full integrator state, in this fixed column order (raw bits):
//   xoBits yoBits zoBits xBits yBits zBits xdBits ydBits zdBits
//   minXBits minYBits minZBits maxXBits maxYBits maxZBits
//   bbWidthBits(f) bbHeightBits(f) age onGround(0/1) stopped(0/1) removed(0/1)
// (doubles -> Double.doubleToRawLongBits %016x; floats -> Float.floatToRawIntBits %08x;
//  ints decimal). All inputs are finite physical values (no NaN/Inf/neg-zero).
public class ParticlePhysicsParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }
    static String f(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // ── reflection handles into the REAL Particle ──────────────────────────────
    static Class<?> PARTICLE;     // declares the integrator (fields + methods under test)
    static Class<?> CARRIER;      // concrete leaf we can allocateInstance (no overrides)
    static Class<?> AABB;
    static Object UNSAFE;
    static Method ALLOCATE;       // Unsafe.allocateInstance(Class)
    static java.lang.reflect.Constructor<?> AABB_CTOR;  // AABB(6 doubles)

    static Field F_xo, F_yo, F_zo, F_x, F_y, F_z, F_xd, F_yd, F_zd, F_bb;
    static Field F_onGround, F_hasPhysics, F_stopped, F_removed;
    static Field F_bbWidth, F_bbHeight, F_age, F_lifetime, F_gravity, F_friction, F_speedUp;
    static Field A_minX, A_minY, A_minZ, A_maxX, A_maxY, A_maxZ;
    static Method M_setSize, M_setPos, M_move, M_tick, M_scale, M_setPower, M_setParticleSpeed;

    static Field df(Class<?> c, String name) throws Exception {
        Field f = c.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }

    static void init() throws Exception {
        PARTICLE = Class.forName("net.minecraft.client.particle.Particle");
        CARRIER  = Class.forName("net.minecraft.client.particle.NoteParticle");
        AABB = Class.forName("net.minecraft.world.phys.AABB");

        // sun.misc.Unsafe via pure reflection (never imported).
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        UNSAFE = theUnsafe.get(null);
        ALLOCATE = unsafeClass.getMethod("allocateInstance", Class.class);

        AABB_CTOR = AABB.getConstructor(double.class, double.class, double.class,
                                        double.class, double.class, double.class);

        F_xo = df(PARTICLE, "xo"); F_yo = df(PARTICLE, "yo"); F_zo = df(PARTICLE, "zo");
        F_x  = df(PARTICLE, "x");  F_y  = df(PARTICLE, "y");  F_z  = df(PARTICLE, "z");
        F_xd = df(PARTICLE, "xd"); F_yd = df(PARTICLE, "yd"); F_zd = df(PARTICLE, "zd");
        F_bb = df(PARTICLE, "bb");
        F_onGround   = df(PARTICLE, "onGround");
        F_hasPhysics = df(PARTICLE, "hasPhysics");
        F_stopped    = df(PARTICLE, "stoppedByCollision");
        F_removed    = df(PARTICLE, "removed");
        F_bbWidth  = df(PARTICLE, "bbWidth");
        F_bbHeight = df(PARTICLE, "bbHeight");
        F_age      = df(PARTICLE, "age");
        F_lifetime = df(PARTICLE, "lifetime");
        F_gravity  = df(PARTICLE, "gravity");
        F_friction = df(PARTICLE, "friction");
        F_speedUp  = df(PARTICLE, "speedUpWhenYMotionIsBlocked");

        A_minX = df(AABB, "minX"); A_minY = df(AABB, "minY"); A_minZ = df(AABB, "minZ");
        A_maxX = df(AABB, "maxX"); A_maxY = df(AABB, "maxY"); A_maxZ = df(AABB, "maxZ");

        M_setSize = PARTICLE.getDeclaredMethod("setSize", float.class, float.class);
        M_setPos  = PARTICLE.getDeclaredMethod("setPos", double.class, double.class, double.class);
        M_move    = PARTICLE.getDeclaredMethod("move", double.class, double.class, double.class);
        M_tick    = PARTICLE.getDeclaredMethod("tick");
        M_scale   = PARTICLE.getDeclaredMethod("scale", float.class);
        M_setPower = PARTICLE.getDeclaredMethod("setPower", float.class);
        M_setParticleSpeed = PARTICLE.getDeclaredMethod("setParticleSpeed",
                                double.class, double.class, double.class);
        for (Method m : new Method[]{M_setSize, M_setPos, M_move, M_tick, M_scale,
                                     M_setPower, M_setParticleSpeed}) m.setAccessible(true);
    }

    // Allocate a fresh Particle (no constructor) and put it in a clean, known base state
    // that mirrors Particle.java's field defaults, with collision physics OFF.
    static Object fresh() throws Exception {
        Object p = ALLOCATE.invoke(UNSAFE, CARRIER);  // concrete; methods dispatch to Particle
        F_xo.setDouble(p, 0.0); F_yo.setDouble(p, 0.0); F_zo.setDouble(p, 0.0);
        F_x.setDouble(p, 0.0);  F_y.setDouble(p, 0.0);  F_z.setDouble(p, 0.0);
        F_xd.setDouble(p, 0.0); F_yd.setDouble(p, 0.0); F_zd.setDouble(p, 0.0);
        // INITIAL_AABB = new AABB(0,0,0,0,0,0)
        F_bb.set(p, AABB_CTOR.newInstance(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        F_onGround.setBoolean(p, false);
        F_hasPhysics.setBoolean(p, false);      // hermetic regime
        F_stopped.setBoolean(p, false);
        F_removed.setBoolean(p, false);
        F_bbWidth.setFloat(p, 0.6F);            // Particle.java field defaults
        F_bbHeight.setFloat(p, 1.8F);
        F_age.setInt(p, 0);
        F_lifetime.setInt(p, 0);
        F_gravity.setFloat(p, 0.0F);
        F_friction.setFloat(p, 0.98F);
        F_speedUp.setBoolean(p, false);
        return p;
    }

    static void emitState(Object p) throws Exception {
        Object bb = F_bb.get(p);
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
        b.append('\t').append(d(A_minX.getDouble(bb)));
        b.append('\t').append(d(A_minY.getDouble(bb)));
        b.append('\t').append(d(A_minZ.getDouble(bb)));
        b.append('\t').append(d(A_maxX.getDouble(bb)));
        b.append('\t').append(d(A_maxY.getDouble(bb)));
        b.append('\t').append(d(A_maxZ.getDouble(bb)));
        b.append('\t').append(f(F_bbWidth.getFloat(p)));
        b.append('\t').append(f(F_bbHeight.getFloat(p)));
        b.append('\t').append(F_age.getInt(p));
        b.append('\t').append(F_onGround.getBoolean(p) ? 1 : 0);
        b.append('\t').append(F_stopped.getBoolean(p) ? 1 : 0);
        b.append('\t').append(F_removed.getBoolean(p) ? 1 : 0);
        O.println(b.toString());
    }

    public static void main(String[] args) throws Exception {
        init();
        long n = 0;

        // sweep grids ------------------------------------------------------------
        double[] starts   = { 0.0, 1.5, -3.25, 12.0, -100.5, 64.123456 };
        double[][] vels   = {
            {0.0, 0.0, 0.0}, {0.1, 0.2, -0.05}, {-0.3, 0.0, 0.15},
            {0.02, -0.4, 0.02}, {0.5, 0.5, 0.5}, {-0.07, 0.21, -0.33}
        };
        float[] gravs     = { 0.0f, 0.04f, 0.06f, 0.75f, 1.0f };
        float[] fricts    = { 0.98f, 0.96f, 0.999f, 0.66f, 0.999f };
        float[][] sizes   = { {0.6f, 1.8f}, {0.2f, 0.2f}, {0.01f, 0.01f}, {0.02f, 0.02f}, {1.0f, 1.0f} };
        int[] lifetimes   = { 1, 4, 6, 20, 100 };
        int[] tickCounts  = { 0, 1, 2, 5, 25 };

        // TICK: full integrator over many steps.
        for (double sx : starts) {
            for (double[] v : vels) {
                for (int gi = 0; gi < gravs.length; gi++) {
                    float grav = gravs[gi];
                    float frict = fricts[gi];
                    float[] sz = sizes[gi % sizes.length];
                    for (int life : lifetimes) {
                        for (int ticks : tickCounts) {
                            for (int su = 0; su <= 1; su++) {
                                Object p = fresh();
                                F_bbWidth.setFloat(p, sz[0]);
                                F_bbHeight.setFloat(p, sz[1]);
                                F_gravity.setFloat(p, grav);
                                F_friction.setFloat(p, frict);
                                F_lifetime.setInt(p, life);
                                F_speedUp.setBoolean(p, su == 1);
                                // mirror the ctor: setPos(x,y,z) builds the bb, then xo/yo/zo=x/y/z.
                                M_setPos.invoke(p, sx, sx * 0.5, -sx);
                                F_xo.setDouble(p, sx); F_yo.setDouble(p, sx * 0.5); F_zo.setDouble(p, -sx);
                                F_xd.setDouble(p, v[0]); F_yd.setDouble(p, v[1]); F_zd.setDouble(p, v[2]);
                                for (int t = 0; t < ticks; t++) M_tick.invoke(p);
                                O.print("TICK\t" + d(sx) + "\t" + d(sx * 0.5) + "\t" + d(-sx)
                                        + "\t" + d(v[0]) + "\t" + d(v[1]) + "\t" + d(v[2])
                                        + "\t" + f(grav) + "\t" + f(frict)
                                        + "\t" + f(sz[0]) + "\t" + f(sz[1])
                                        + "\t" + life + "\t" + ticks + "\t" + su);
                                emitState(p);
                                n++;
                            }
                        }
                    }
                }
            }
        }

        // MOVE: isolate move() incl. the 1.0E-5F stop threshold and zero/non-zero deltas.
        double[][] moves = {
            {0.0, 0.0, 0.0}, {0.1, -0.2, 0.05}, {0.0, -1e-6, 0.0}, {0.0, 1e-6, 0.0},
            {1e-4, 0.0, -1e-4}, {-0.5, 0.5, -0.5}, {0.0, -0.04, 0.0}, {2.0, -3.0, 4.0}
        };
        for (double sx : starts) {
            for (float[] sz : sizes) {
                for (double[] m : moves) {
                    Object p = fresh();
                    F_bbWidth.setFloat(p, sz[0]);
                    F_bbHeight.setFloat(p, sz[1]);
                    M_setPos.invoke(p, sx, sx + 1.0, sx - 1.0);
                    M_move.invoke(p, m[0], m[1], m[2]);
                    O.print("MOVE\t" + d(sx) + "\t" + d(sx + 1.0) + "\t" + d(sx - 1.0)
                            + "\t" + f(sz[0]) + "\t" + f(sz[1])
                            + "\t" + d(m[0]) + "\t" + d(m[1]) + "\t" + d(m[2]));
                    emitState(p);
                    n++;
                }
            }
        }

        // SIZE: isolate setSize() incl. the unchanged-size no-op guard.
        float[][] newSizes = {
            {0.6f, 1.8f}, {0.2f, 0.2f}, {0.01f, 0.01f}, {1.0f, 0.5f}, {0.5f, 1.0f}, {2.0f, 2.0f}
        };
        for (double sx : starts) {
            for (float[] sz : sizes) {
                for (float[] ns : newSizes) {
                    Object p = fresh();
                    F_bbWidth.setFloat(p, sz[0]);
                    F_bbHeight.setFloat(p, sz[1]);
                    M_setPos.invoke(p, sx, sx + 2.0, sx - 2.0);
                    M_setSize.invoke(p, ns[0], ns[1]);
                    O.print("SIZE\t" + d(sx) + "\t" + d(sx + 2.0) + "\t" + d(sx - 2.0)
                            + "\t" + f(sz[0]) + "\t" + f(sz[1])
                            + "\t" + f(ns[0]) + "\t" + f(ns[1]));
                    emitState(p);
                    n++;
                }
            }
        }

        // POS: isolate setPos() bb construction.
        for (double sx : starts) {
            for (float[] sz : sizes) {
                Object p = fresh();
                F_bbWidth.setFloat(p, sz[0]);
                F_bbHeight.setFloat(p, sz[1]);
                M_setPos.invoke(p, sx, sx * 0.25, sx * -0.5);
                O.print("POS\t" + f(sz[0]) + "\t" + f(sz[1])
                        + "\t" + d(sx) + "\t" + d(sx * 0.25) + "\t" + d(sx * -0.5));
                emitState(p);
                n++;
            }
        }

        // POWER: isolate setPower().
        float[] powers = { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 0.333333f };
        for (double[] v : vels) {
            for (float pw : powers) {
                Object p = fresh();
                F_xd.setDouble(p, v[0]); F_yd.setDouble(p, v[1]); F_zd.setDouble(p, v[2]);
                M_setPower.invoke(p, pw);
                O.print("POWER\t" + d(v[0]) + "\t" + d(v[1]) + "\t" + d(v[2]) + "\t" + f(pw));
                emitState(p);
                n++;
            }
        }

        // SCALE: isolate scale() -> setSize(0.2F*s, 0.2F*s).
        float[] scales = { 0.5f, 1.0f, 1.5f, 0.25f, 3.0f, 0.123456f };
        for (double sx : starts) {
            for (float[] sz : sizes) {
                for (float s : scales) {
                    Object p = fresh();
                    F_bbWidth.setFloat(p, sz[0]);
                    F_bbHeight.setFloat(p, sz[1]);
                    M_setPos.invoke(p, sx, sx + 3.0, sx - 3.0);
                    M_scale.invoke(p, s);
                    O.print("SCALE\t" + d(sx) + "\t" + d(sx + 3.0) + "\t" + d(sx - 3.0)
                            + "\t" + f(sz[0]) + "\t" + f(sz[1]) + "\t" + f(s));
                    emitState(p);
                    n++;
                }
            }
        }

        System.err.println("rows=" + n);
    }
}

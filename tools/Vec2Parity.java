// Ground-truth generator for net.minecraft.world.phys.Vec2 (the float-precision
// 2D rotation/math vector). Pure; no Bootstrap needed. The C++ port (world/phys/
// Vec2.h) must match bit-for-bit (floats as raw IEEE-754 bits).
//
//   tools/run_groundtruth.ps1 -Tool Vec2Parity -Out mcpp/build/vec2.tsv

import net.minecraft.world.phys.Vec2;

public class Vec2Parity {
    static final java.io.PrintStream O = System.out;
    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String v2(Vec2 v) { return f(v.x) + "\t" + f(v.y); }

    // Exhaustive battery: zeros, ones, units, negatives, fractions, large/small
    // magnitudes, asymmetric components, NaN/Inf edges, and the public constants.
    static final float[][] VS = {
        {0f, 0f}, {1f, 1f}, {1f, 0f}, {0f, 1f}, {-1f, 0f}, {0f, -1f},
        {1f, 2f}, {-1.5f, 0.5f}, {3.25f, -7.75f}, {10f, -20f},
        {0.1f, 0.2f}, {-3.7f, 8.2f}, {100f, -100f}, {0.5f, 0.5f},
        {-0.0001f, 0f}, {1e6f, -1e6f}, {2.5f, -2.5f}, {0.00005f, 0.00005f},
        {0.0001f, 0f}, {0.00009999f, 0f}, {7e-5f, 7e-5f},
        {Float.MAX_VALUE, Float.MAX_VALUE}, {Float.MIN_VALUE, Float.MIN_VALUE},
        {-0f, 0f}, {123.456f, 789.012f}, {-0.0f, -0.0f},
        {Float.NaN, 1f}, {Float.POSITIVE_INFINITY, 0f}, {Float.NEGATIVE_INFINITY, 2f},
        {1f, Float.NaN}, {3.4e38f, -3.4e38f}
    };
    static final float[] SCALARS = { 0f, 1f, -1f, 2.5f, 0.5f, -3.0f, 100f, 1e-4f, -0.25f };
    static final float[] ADDENDS = { 0f, 1f, -1f, 0.5f, -2.25f, 10f };

    public static void main(String[] args) throws Exception {
        for (float[] a : VS) {
            Vec2 va = new Vec2(a[0], a[1]);
            String in = f(a[0]) + "\t" + f(a[1]);
            O.println("LENGTH\t"     + in + "\t" + f(va.length()));
            O.println("LENGTHSQ\t"   + in + "\t" + f(va.lengthSquared()));
            O.println("NORMALIZED\t" + in + "\t" + v2(va.normalized()));
            O.println("NEGATED\t"    + in + "\t" + v2(va.negated()));
            for (float s : SCALARS)
                O.println("SCALE\t" + in + "\t" + f(s) + "\t" + v2(va.scale(s)));
            for (float v : ADDENDS)
                O.println("ADDF\t" + in + "\t" + f(v) + "\t" + v2(va.add(v)));
        }
        // Binary ops over all ordered pairs.
        for (float[] a : VS) for (float[] b : VS) {
            Vec2 va = new Vec2(a[0], a[1]), vb = new Vec2(b[0], b[1]);
            String in = f(a[0]) + "\t" + f(a[1]) + "\t" + f(b[0]) + "\t" + f(b[1]);
            O.println("ADD\t"     + in + "\t" + v2(va.add(vb)));
            O.println("DOT\t"     + in + "\t" + f(va.dot(vb)));
            O.println("DISTSQR\t" + in + "\t" + f(va.distanceToSqr(vb)));
            O.println("EQUALS\t"  + in + "\t" + (va.equals(vb) ? 1 : 0));
        }
        // Public constants — verify the C++ literals match the real fields.
        O.println("CONST\tZERO\t"       + v2(Vec2.ZERO));
        O.println("CONST\tONE\t"        + v2(Vec2.ONE));
        O.println("CONST\tUNIT_X\t"     + v2(Vec2.UNIT_X));
        O.println("CONST\tNEG_UNIT_X\t" + v2(Vec2.NEG_UNIT_X));
        O.println("CONST\tUNIT_Y\t"     + v2(Vec2.UNIT_Y));
        O.println("CONST\tNEG_UNIT_Y\t" + v2(Vec2.NEG_UNIT_Y));
        O.println("CONST\tMAX\t"        + v2(Vec2.MAX));
        O.println("CONST\tMIN\t"        + v2(Vec2.MIN));
    }
}

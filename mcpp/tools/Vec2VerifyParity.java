// Independent verification ground-truth for net.minecraft.world.phys.Vec2 — the
// float-precision 2D rotation/math vector (x, y). This is a SECOND, finite/physical
// battery (no NaN/+-Inf/-0.0) that re-certifies the existing engine header
// world/phys/Vec2.h via the vec2_verify_parity gate. Calls the REAL Vec2 methods;
// pure, no Bootstrap needed. Floats are emitted as raw IEEE-754 bits so every
// comparison downstream is bit-for-bit.
//
//   tools/run_groundtruth.ps1 -Tool Vec2VerifyParity -Out mcpp/build/vec2_verify.tsv
//
// Methods verified (Vec2.java 26.1.2): scale, dot, add(Vec2), add(float), equals,
// normalized, length, lengthSquared, distanceToSqr, negated, and all 8 public
// constants. Mth.sqrt(float) = (float)Math.sqrt(x) (correctly rounded) underlies
// length()/normalized().

import net.minecraft.world.phys.Vec2;

public class Vec2VerifyParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String v2(Vec2 v) { return f(v.x) + "\t" + f(v.y); }

    // FINITE / PHYSICAL inputs only: zeros, ones, units, negatives, fractions,
    // sub-1e-4 magnitudes that straddle the normalized() cutoff, asymmetric
    // components, and modest large/small magnitudes. No NaN/Inf/-0.0 — those would
    // diverge only in sign/NaN bits and add nothing to a math gate.
    static final float[][] VS = {
        {0f, 0f}, {1f, 1f}, {1f, 0f}, {0f, 1f}, {-1f, 0f}, {0f, -1f},
        {1f, 2f}, {-1.5f, 0.5f}, {3.25f, -7.75f}, {10f, -20f},
        {0.1f, 0.2f}, {-3.7f, 8.2f}, {100f, -100f}, {0.5f, 0.5f},
        {2.5f, -2.5f}, {123.456f, 789.012f}, {-42.0f, 17.0f}, {0.75f, -0.125f},
        // normalized() cutoff dist < 1.0E-4F probes (length just below / above):
        {0.00005f, 0.00005f}, {0.0001f, 0f}, {0.00009999f, 0f}, {7e-5f, 7e-5f},
        {-0.0001f, 0f}, {5e-5f, 0f}, {0.00015f, 0f}, {6e-5f, 8e-5f},
        // larger but still finite magnitudes:
        {1e6f, -1e6f}, {2.0e7f, 3.0e7f}, {1234.5f, -6789.0f}, {0.001f, 0.002f}
    };
    static final float[] SCALARS = { 0f, 1f, -1f, 2.5f, 0.5f, -3.0f, 100f, 1e-4f, -0.25f, 0.333f };
    static final float[] ADDENDS = { 0f, 1f, -1f, 0.5f, -2.25f, 10f, 0.001f, -100f };

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
        // Public constants — re-verify the C++ literals match the real fields.
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

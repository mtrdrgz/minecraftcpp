// Ground-truth generator for the remaining net.minecraft.util.Mth helpers that the
// engine-wide mth_parity gate does NOT yet cover directly, focused on the assigned
// subset: wrapDegrees(float/double), degreesDifference, positiveModulo(int/float/double),
// clamp(int/float/double), length(x,y) (both overloads), lengthSquared(x,y) /
// lengthSquared(x,y,z), getSeed(x,y,z), floorDiv, floorMod (java.lang.Math).
//
// Pure functions only (no RandomSource / Bootstrap). The C++ test (MthExtraParityTest)
// recomputes each row from mc::levelgen::mth and must match BIT-FOR-BIT; floats/doubles
// are exchanged as raw IEEE-754 bit patterns. FINITE/PHYSICAL inputs only.
//
//   tools/run_groundtruth.ps1 -Tool MthExtraParity -Out mcpp/build/mth_extra.tsv
//
// Note: Mth.length(float,float) and Mth.floorMod are exercised via REAL calls:
//   Mth.length(float,float)  -> public static in Mth (Mth.java:684).
//   Mth.floorMod is not a Mth method; the assigned "floorMod" is java.lang.Math.floorMod,
//   which Mth.positiveModulo(int) delegates to (Mth.java:166). We call Math.floorMod
//   directly as ground truth for the C++ mth::floorMod helper.

import net.minecraft.util.Mth;

@SuppressWarnings("deprecation") // Mth.getSeed is @Deprecated but still live and tested
public class MthExtraParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // Finite/physical sweeps. No NaN / Infinity / -0.0 (those diverge only in sign/NaN
    // bits and waste the gate). Degrees cover well past +/-360 to exercise the wrap.
    static final float[] DEG_F = {
        0.0f, 1.0f, -1.0f, 45.0f, -45.0f, 90.0f, 135.0f, 179.0f, 180.0f, 181.0f,
        270.0f, 359.0f, 360.0f, 361.0f, 540.0f, 720.0f, 1080.5f, -179.0f, -180.0f,
        -181.0f, -360.0f, -540.3f, -720.0f, 359.9f, 0.5f, -0.5f, 123.456f, -987.654f,
        89.999f, 90.001f
    };
    static final double[] DEG_D = {
        0.0, 1.0, -1.0, 45.0, -45.0, 90.0, 135.0, 179.0, 180.0, 181.0,
        270.0, 359.0, 360.0, 361.0, 540.0, 720.0, 1080.5, -179.0, -180.0,
        -181.0, -360.0, -540.3, -720.0, 359.9, 0.5, -0.5, 123.456789, -987.654321,
        89.999, 90.001, 1234.5678, -2718.281828
    };
    // For positiveModulo / clamp / length numeric sweeps (finite, includes negatives
    // where the operation is defined).
    static final double[] NUM_D = {
        0.0, 1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 3.25, -3.25, 7.0, -7.0, 7.5, -7.5,
        10.0, -10.0, 13.37, -13.37, 100.25, -100.75, 0.001, -0.001, 50.0, -50.0,
        3.141592653589793, -3.141592653589793, 6.5, -6.5, 99.9, -99.9
    };
    static final int[] NUM_I = {
        0, 1, -1, 2, -2, 3, -3, 4, 5, -5, 6, 7, -7, 8, -8, 12, -12, 13, 16, -16,
        17, 100, -100, 255, 256, -257, 1000, -1000, 1023, -1024
    };
    static final int[] MODS_I = { 1, 2, 3, 4, 5, 7, 8, 16, 360, -3, -5, -7, -360 };

    public static void main(String[] args) throws Exception {
        // ── wrapDegrees(float) / wrapDegrees(double) ─────────────────────────────
        for (float a : DEG_F) O.println("WRAPDEG_F\t" + f(a) + "\t" + f(Mth.wrapDegrees(a)));
        for (double a : DEG_D) O.println("WRAPDEG_D\t" + d(a) + "\t" + d(Mth.wrapDegrees(a)));

        // ── degreesDifference(float, float) = wrapDegrees(to - from) ─────────────
        for (float from : DEG_F)
            for (float to : new float[]{0.0f, 10.0f, 90.0f, 180.0f, 270.0f, 359.0f, -45.0f, -170.0f, 720.0f})
                O.println("DEGDIFF\t" + f(from) + "\t" + f(to) + "\t" + f(Mth.degreesDifference(from, to)));

        // ── positiveModulo(int, int) = Math.floorMod ─────────────────────────────
        for (int in : NUM_I)
            for (int mod : MODS_I)
                O.println("POSMOD_I\t" + in + "\t" + mod + "\t" + Mth.positiveModulo(in, mod));

        // ── positiveModulo(float, float) / positiveModulo(double, double) ────────
        float[] FMODS = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f, 8.0f, 360.0f, 0.5f, 1.5f };
        for (double v : NUM_D)
            for (float mod : FMODS) {
                float vf = (float) v;
                O.println("POSMOD_F\t" + f(vf) + "\t" + f(mod) + "\t" + f(Mth.positiveModulo(vf, mod)));
            }
        double[] DMODS = { 1.0, 2.0, 3.0, 4.0, 5.0, 7.0, 8.0, 360.0, 0.5, 1.5 };
        for (double v : NUM_D)
            for (double mod : DMODS)
                O.println("POSMOD_D\t" + d(v) + "\t" + d(mod) + "\t" + d(Mth.positiveModulo(v, mod)));

        // ── clamp(int, int, int) / clamp(float,..) / clamp(double,..) ────────────
        for (int v : NUM_I)
            for (int[] mm : new int[][]{ {0, 10}, {-5, 5}, {-100, 100}, {3, 3}, {-1, 1} })
                O.println("CLAMP_I\t" + v + "\t" + mm[0] + "\t" + mm[1] + "\t" + Mth.clamp(v, mm[0], mm[1]));
        for (double v : NUM_D)
            for (double[] mm : new double[][]{ {0.0, 1.0}, {-5.0, 5.0}, {-50.5, 50.5}, {2.0, 2.0} }) {
                float vf = (float) v;
                O.println("CLAMP_F\t" + f(vf) + "\t" + f((float) mm[0]) + "\t" + f((float) mm[1]) + "\t" + f(Mth.clamp(vf, (float) mm[0], (float) mm[1])));
                O.println("CLAMP_D\t" + d(v) + "\t" + d(mm[0]) + "\t" + d(mm[1]) + "\t" + d(Mth.clamp(v, mm[0], mm[1])));
            }

        // ── length(x,y) double + float overloads / lengthSquared(x,y) ────────────
        // Mth.length(float,float) = (float)Math.sqrt(lengthSquared(x,y)) where
        // lengthSquared(x,y) is the DOUBLE overload (x,y promoted) — Mth.java:684-686.
        double[] LX = { 0.0, 1.0, -1.0, 3.0, -3.0, 4.0, -4.0, 5.0, 7.5, -7.5, 12.34, -56.78, 100.0, 0.001 };
        double[] LY = { 0.0, 1.0, -1.0, 2.0, -2.0, 4.0, 6.0, -8.0, 9.99, -10.1, 0.5 };
        for (double x : LX)
            for (double y : LY) {
                O.println("LEN2_D\t" + d(x) + "\t" + d(y) + "\t" + d(Mth.length(x, y)));
                O.println("LENSQ2_D\t" + d(x) + "\t" + d(y) + "\t" + d(Mth.lengthSquared(x, y)));
                float xf = (float) x, yf = (float) y;
                O.println("LEN2_F\t" + f(xf) + "\t" + f(yf) + "\t" + f(Mth.length(xf, yf)));
            }
        // lengthSquared(x,y,z) double + float
        double[] LZ = { 0.0, 1.0, -1.0, 2.0, -3.5, 6.0 };
        for (double x : LX)
            for (double y : LY)
                for (double z : LZ) {
                    O.println("LENSQ3_D\t" + d(x) + "\t" + d(y) + "\t" + d(z) + "\t" + d(Mth.lengthSquared(x, y, z)));
                    float xf = (float) x, yf = (float) y, zf = (float) z;
                    O.println("LENSQ3_F\t" + f(xf) + "\t" + f(yf) + "\t" + f(zf) + "\t" + f(Mth.lengthSquared(xf, yf, zf)));
                }

        // ── getSeed(x,y,z) — Mth.java:313-318 ────────────────────────────────────
        int[] SX = { 0, 1, -1, 16, -16, 100, -100, 255, -256, 30000, -30000, 1000000, -1000000 };
        int[] SY = { 0, 1, -1, 63, 64, 127, -64, 255, -1 };
        int[] SZ = { 0, 1, -1, 16, -16, 100, -100, 30000, -30000, 5000 };
        for (int x : SX)
            for (int y : SY)
                for (int z : SZ)
                    O.println("GETSEED\t" + x + "\t" + y + "\t" + z + "\t" + Mth.getSeed(x, y, z));

        // ── floorDiv(int,int) — Mth.floorDiv delegates to Math.floorDiv (Mth.java:141) ──
        // ── floorMod(int,int) — java.lang.Math.floorMod (what positiveModulo(int) uses) ──
        int[] DIVISORS = { 1, 2, 3, 4, 5, 7, 8, 16, 360, -1, -2, -3, -4, -7, -360 };
        for (int a : NUM_I)
            for (int b : DIVISORS) {
                O.println("FLOORDIV\t" + a + "\t" + b + "\t" + Mth.floorDiv(a, b));
                O.println("FLOORMOD\t" + a + "\t" + b + "\t" + Math.floorMod(a, b));
            }
    }
}

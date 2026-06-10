// Ground-truth generator for net.minecraft.util.Mth (the engine-wide math utility)
// using the REAL decompiled 26.1.2 class. Pure functions only (no RandomSource),
// so no Bootstrap is needed. The C++ test (MthParityTest) recomputes each row and
// must match bit-for-bit; all floats/doubles are exchanged as raw IEEE-754 bits.
//
//   tools/run_groundtruth.ps1 -Tool MthParity -Out mcpp/build/mth.tsv

import net.minecraft.util.Mth;

@SuppressWarnings("deprecation") // Mth.getSeed / Mth.fastInvSqrt are @Deprecated but still live and tested
public class MthParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    static final double[] DS = {
        0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 3.141592653589793, -2.718281828, 100.25, -100.75,
        1.0E-7, 1.0E7, 0.16666666, 359.9, 360.0, 720.5, -540.3, 45.0, 1234.5678, -0.999999,
        2.0, -2.0, 0.25, 0.75, 6.28318, -3.5, 89.999, 90.0, 180.0, 270.0, 0.0001, 12345.6789
    };
    static final int[] IS = {
        0, 1, -1, 2, 3, 7, 8, 15, 16, 17, 31, 32, 100, -100, 255, 256, 1023, 1024,
        65535, 65536, -7, 1000000, -1000000, 5, 6, 9, 13, 63, 64, 127, 128, 511, 512
    };
    static final long[] LS = { 0, 1, -1, 360, 359, 180, 720, -540, 1000000000000L, -1000000000000L, 12345678901L };

    public static void main(String[] args) throws Exception {
        // Dump the REAL Mth.ASIN_TAB / COS_TAB (private static, built by Mth's
        // static initializer with Math.asin/Math.cos) so the C++ atan2 can embed
        // bit-identical tables instead of recomputing with a possibly-different libm.
        java.lang.reflect.Field fa = Mth.class.getDeclaredField("ASIN_TAB"); fa.setAccessible(true);
        java.lang.reflect.Field fc = Mth.class.getDeclaredField("COS_TAB");  fc.setAccessible(true);
        double[] asinTab = (double[]) fa.get(null);
        double[] cosTab  = (double[]) fc.get(null);
        for (int k = 0; k < asinTab.length; k++) O.println("ASINTAB\t" + k + "\t" + d(asinTab[k]));
        for (int k = 0; k < cosTab.length; k++)  O.println("COSTAB\t"  + k + "\t" + d(cosTab[k]));

        // SIN/COS over DS plus a fine sweep
        for (double v : DS) { O.println("SIN\t" + d(v) + "\t" + f(Mth.sin(v))); O.println("COS\t" + d(v) + "\t" + f(Mth.cos(v))); }
        for (int i = -3200; i <= 3200; i += 7) {
            double v = i / 1000.0;
            O.println("SIN\t" + d(v) + "\t" + f(Mth.sin(v)));
            O.println("COS\t" + d(v) + "\t" + f(Mth.cos(v)));
        }

        for (double v : DS) {
            float vf = (float) v;
            O.println("SQRTF\t" + f(Math.abs(vf)) + "\t" + f(Mth.sqrt(Math.abs(vf))));
            O.println("FLOOR_D\t" + d(v) + "\t" + Mth.floor(v));
            O.println("FLOOR_F\t" + f(vf) + "\t" + Mth.floor(vf));
            O.println("LFLOOR\t" + d(v) + "\t" + Mth.lfloor(v));
            O.println("CEIL_D\t" + d(v) + "\t" + Mth.ceil(v));
            O.println("CEIL_F\t" + f(vf) + "\t" + Mth.ceil(vf));
            O.println("CEILLONG\t" + d(v) + "\t" + Mth.ceilLong(v));
            O.println("ABS_F\t" + f(vf) + "\t" + f(Mth.abs(vf)));
            O.println("FRAC_F\t" + f(vf) + "\t" + f(Mth.frac(vf)));
            O.println("FRAC_D\t" + d(v) + "\t" + d(Mth.frac(v)));
            O.println("WRAPDEG_F\t" + f(vf) + "\t" + f(Mth.wrapDegrees(vf)));
            O.println("WRAPDEG_D\t" + d(v) + "\t" + d(Mth.wrapDegrees(v)));
            O.println("SMOOTH\t" + d(v) + "\t" + d(Mth.smoothstep(v)));
            O.println("SMOOTHDERIV\t" + d(v) + "\t" + d(Mth.smoothstepDerivative(v)));
            O.println("SIGN\t" + d(v) + "\t" + Mth.sign(v));
            O.println("SQUARE_D\t" + d(v) + "\t" + d(Mth.square(v)));
            O.println("SQUARE_F\t" + f(vf) + "\t" + f(Mth.square(vf)));
            O.println("CUBE_F\t" + f(vf) + "\t" + f(Mth.cube(vf)));
            O.println("FASTINVSQRT\t" + d(Math.abs(v) + 0.001) + "\t" + d(Mth.fastInvSqrt(Math.abs(v) + 0.001)));
            O.println("FASTINVCBRT_F\t" + f(Math.abs(vf) + 0.001f) + "\t" + f(Mth.fastInvCubeRoot(Math.abs(vf) + 0.001f)));
            O.println("INVSQRT_F\t" + f(Math.abs(vf) + 0.001f) + "\t" + f(Mth.invSqrt(Math.abs(vf) + 0.001f)));
            O.println("INVSQRT_D\t" + d(Math.abs(v) + 0.001) + "\t" + d(Mth.invSqrt(Math.abs(v) + 0.001)));
            O.println("PACKDEG\t" + f(vf) + "\t" + Mth.packDegrees(vf));
        }

        for (int x : IS) {
            O.println("ABS_I\t" + x + "\t" + Mth.abs(x));
            O.println("SEPOT\t" + x + "\t" + Mth.smallestEncompassingPowerOfTwo(x));
            O.println("ISPOW2\t" + x + "\t" + (Mth.isPowerOfTwo(x) ? 1 : 0));
            O.println("MURMUR\t" + x + "\t" + Mth.murmurHash3Mixer(x));
            O.println("SQUARE_I\t" + x + "\t" + Mth.square(x));
            if (x > 0) {
                O.println("CEILLOG2\t" + x + "\t" + Mth.ceillog2(x));
                O.println("LOG2\t" + x + "\t" + Mth.log2(x));
                O.println("SQSIDE\t" + x + "\t" + Mth.smallestSquareSide(x));
            }
        }
        for (long x : LS) {
            O.println("WRAPDEG_L\t" + x + "\t" + f(Mth.wrapDegrees(x)));
            O.println("SQUARE_L\t" + x + "\t" + Mth.square(x));
        }
        for (int x : IS) O.println("WRAPDEG_I\t" + x + "\t" + Mth.wrapDegrees(x));

        // clamp / lerp / map families
        for (double v : DS) {
            float vf = (float) v;
            O.println("CLAMP_D\t" + d(v) + "\t" + d(-50.0) + "\t" + d(50.0) + "\t" + d(Mth.clamp(v, -50.0, 50.0)));
            O.println("CLAMP_F\t" + f(vf) + "\t" + f(-50.0f) + "\t" + f(50.0f) + "\t" + f(Mth.clamp(vf, -50.0f, 50.0f)));
            O.println("CLAMPEDLERP_D\t" + d(v) + "\t" + d(2.0) + "\t" + d(8.0) + "\t" + d(Mth.clampedLerp(v, 2.0, 8.0)));
            O.println("CLAMPEDLERP_F\t" + f(vf) + "\t" + f(2.0f) + "\t" + f(8.0f) + "\t" + f(Mth.clampedLerp(vf, 2.0f, 8.0f)));
            O.println("INVLERP_D\t" + d(v) + "\t" + d(-10.0) + "\t" + d(10.0) + "\t" + d(Mth.inverseLerp(v, -10.0, 10.0)));
            O.println("INVLERP_F\t" + f(vf) + "\t" + f(-10.0f) + "\t" + f(10.0f) + "\t" + f(Mth.inverseLerp(vf, -10.0f, 10.0f)));
            O.println("MAP_D\t" + d(v) + "\t" + d(0.0) + "\t" + d(100.0) + "\t" + d(-1.0) + "\t" + d(1.0) + "\t" + d(Mth.map(v, 0.0, 100.0, -1.0, 1.0)));
            O.println("MAP_F\t" + f(vf) + "\t" + f(0.0f) + "\t" + f(100.0f) + "\t" + f(-1.0f) + "\t" + f(1.0f) + "\t" + f(Mth.map(vf, 0.0f, 100.0f, -1.0f, 1.0f)));
            O.println("CLAMPEDMAP_D\t" + d(v) + "\t" + d(0.0) + "\t" + d(100.0) + "\t" + d(-1.0) + "\t" + d(1.0) + "\t" + d(Mth.clampedMap(v, 0.0, 100.0, -1.0, 1.0)));
            O.println("QUANTIZE\t" + d(v) + "\t16\t" + Mth.quantize(v, 16));
        }
        for (int a : new int[]{0,1,2,3,4,5,7,8,9,15,16,17,100,-7,-16}) {
            O.println("FLOORDIV\t" + a + "\t3\t" + Mth.floorDiv(a, 3));
            O.println("FLOORDIV\t" + a + "\t-4\t" + Mth.floorDiv(a, -4));
            O.println("POSMOD_I\t" + a + "\t5\t" + Mth.positiveModulo(a, 5));
            O.println("POSCEILDIV\t" + a + "\t4\t" + Mth.positiveCeilDiv(a, 4));
            O.println("ROUNDTOWARD\t" + a + "\t8\t" + Mth.roundToward(a, 8));
        }
        for (double v : DS) {
            float vf = (float) v;
            O.println("POSMOD_F\t" + f(vf) + "\t" + f(7.0f) + "\t" + f(Mth.positiveModulo(vf, 7.0f)));
            O.println("POSMOD_D\t" + d(v) + "\t" + d(7.0) + "\t" + d(Mth.positiveModulo(v, 7.0)));
        }

        // atan2 over a grid
        double[] AT = { 0.0, 1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 3.3, -4.4, 0.001, -0.001, 100.0, -100.0 };
        for (double y : AT) for (double x : AT) O.println("ATAN2\t" + d(y) + "\t" + d(x) + "\t" + d(Mth.atan2(y, x)));

        // hsv
        for (int hi = 0; hi <= 12; hi++) {
            float hue = hi / 12.0F;
            for (float sat : new float[]{0.0f, 0.3f, 0.7f, 1.0f})
                for (float val : new float[]{0.0f, 0.5f, 1.0f})
                    for (int a : new int[]{0, 128, 255})
                        O.println("HSV\t" + f(hue) + "\t" + f(sat) + "\t" + f(val) + "\t" + a + "\t" + Mth.hsvToArgb(hue, sat, val, a));
        }

        // lerp2/lerp3/catmullrom/rotLerp/triangleWave/length
        double[] AL = { 0.0, 0.25, 0.5, 0.75, 1.0, 1.5, -0.5 };
        for (double a1 : AL) for (double a2 : AL)
            O.println("LERP2\t" + d(a1) + "\t" + d(a2) + "\t" + d(1.0) + "\t" + d(2.0) + "\t" + d(3.0) + "\t" + d(4.0) + "\t" + d(Mth.lerp2(a1, a2, 1.0, 2.0, 3.0, 4.0)));
        for (double a1 : AL)
            O.println("LERP3\t" + d(a1) + "\t" + d(0.5) + "\t" + d(0.25) + "\t" + d(1) + "\t" + d(2) + "\t" + d(3) + "\t" + d(4) + "\t" + d(5) + "\t" + d(6) + "\t" + d(7) + "\t" + d(8) + "\t" + d(Mth.lerp3(a1, 0.5, 0.25, 1, 2, 3, 4, 5, 6, 7, 8)));
        for (float a : new float[]{0.0f, 0.5f, 1.0f, 1.5f, -0.5f})
            O.println("CATMULL\t" + f(a) + "\t" + f(1.0f) + "\t" + f(2.0f) + "\t" + f(4.0f) + "\t" + f(8.0f) + "\t" + f(Mth.catmullrom(a, 1.0f, 2.0f, 4.0f, 8.0f)));
        for (float a : new float[]{0.0f, 0.3f, 0.5f, 0.7f, 1.0f})
            for (float from : new float[]{10.0f, 170.0f, 350.0f, -20.0f})
                for (float to : new float[]{20.0f, 200.0f, -170.0f}) {
                    O.println("ROTLERP_F\t" + f(a) + "\t" + f(from) + "\t" + f(to) + "\t" + f(Mth.rotLerp(a, from, to)));
                    O.println("ROTLERPRAD\t" + f(a) + "\t" + f(from) + "\t" + f(to) + "\t" + f(Mth.rotLerpRad(a, from, to)));
                }
        for (float idx : new float[]{0.0f, 1.0f, 2.5f, 5.0f, 7.3f, -3.0f})
            O.println("TRIWAVE\t" + f(idx) + "\t" + f(4.0f) + "\t" + f(Mth.triangleWave(idx, 4.0f)));
        for (double x : DS) for (double y : new double[]{0.0, 3.0, -4.0}) {
            O.println("LEN2_D\t" + d(x) + "\t" + d(y) + "\t" + d(Mth.length(x, y)));
            O.println("LEN3_D\t" + d(x) + "\t" + d(y) + "\t" + d(2.0) + "\t" + d(Mth.length(x, y, 2.0)));
            O.println("LENSQ3_F\t" + f((float)x) + "\t" + f((float)y) + "\t" + f(2.0f) + "\t" + f(Mth.lengthSquared((float)x, (float)y, 2.0f)));
        }
        // lerpInt / lerpDiscrete
        for (float a : new float[]{0.0f, 0.25f, 0.5f, 0.99f, 1.0f, -0.3f})
            for (int p0 : new int[]{0, 5, -3}) for (int p1 : new int[]{10, 2, -8}) {
                O.println("LERPINT\t" + f(a) + "\t" + p0 + "\t" + p1 + "\t" + Mth.lerpInt(a, p0, p1));
                O.println("LERPDISCRETE\t" + f(a) + "\t" + p0 + "\t" + p1 + "\t" + Mth.lerpDiscrete(a, p0, p1));
            }
        // getSeed
        for (int x : new int[]{0, 1, -1, 100, -100, 2000000}) for (int y : new int[]{0, 64, 255, -50}) for (int z : new int[]{0, 1, -1, 5000})
            O.println("GETSEED\t" + x + "\t" + y + "\t" + z + "\t" + Mth.getSeed(x, y, z));
        // degreesDifference / approach / rotateIfNecessary
        for (float a : new float[]{0.0f, 90.0f, 180.0f, 270.0f, 350.0f, -45.0f})
            for (float b : new float[]{10.0f, 200.0f, -90.0f, 359.0f}) {
                O.println("DEGDIFF\t" + f(a) + "\t" + f(b) + "\t" + f(Mth.degreesDifference(a, b)));
                O.println("DEGDIFFABS\t" + f(a) + "\t" + f(b) + "\t" + f(Mth.degreesDifferenceAbs(a, b)));
                O.println("ROTIFNEC\t" + f(a) + "\t" + f(b) + "\t" + f(30.0f) + "\t" + f(Mth.rotateIfNecessary(a, b, 30.0f)));
                O.println("APPROACH\t" + f(a) + "\t" + f(b) + "\t" + f(15.0f) + "\t" + f(Mth.approach(a, b, 15.0f)));
                O.println("APPROACHDEG\t" + f(a) + "\t" + f(b) + "\t" + f(15.0f) + "\t" + f(Mth.approachDegrees(a, b, 15.0f)));
            }
        // equal / chessboard / isMultipleOf / unpackDegrees
        O.println("EQUAL_F\t" + f(1.0f) + "\t" + f(1.000001f) + "\t" + (Mth.equal(1.0f, 1.000001f) ? 1 : 0));
        O.println("EQUAL_F\t" + f(1.0f) + "\t" + f(1.1f) + "\t" + (Mth.equal(1.0f, 1.1f) ? 1 : 0));
        O.println("EQUAL_D\t" + d(5.0) + "\t" + d(5.0000001) + "\t" + (Mth.equal(5.0, 5.0000001) ? 1 : 0));
        for (int x0 : new int[]{0,5,-3}) for (int z0 : new int[]{0,7,-2}) for (int x1 : new int[]{10,-8}) for (int z1 : new int[]{3,-15})
            O.println("CHESS\t" + x0 + "\t" + z0 + "\t" + x1 + "\t" + z1 + "\t" + Mth.chessboardDistance(x0, z0, x1, z1));
        for (int a : new int[]{0,1,2,3,6,7,12,15}) O.println("ISMULT\t" + a + "\t3\t" + (Mth.isMultipleOf(a, 3) ? 1 : 0));
        for (int rb = -128; rb < 128; rb += 7) O.println("UNPACKDEG\t" + rb + "\t" + f(Mth.unpackDegrees((byte) rb)));
        // absMax
        for (int a : new int[]{-5,3,-10}) for (int b : new int[]{2,-8,6}) O.println("ABSMAX_I\t" + a + "\t" + b + "\t" + Mth.absMax(a, b));
        // binarySearch with predicate middle >= threshold
        for (int th : new int[]{-5, 0, 3, 50, 100, 1000}) O.println("BINSEARCH\t0\t100\t" + th + "\t" + Mth.binarySearch(0, 100, m -> m >= th));
    }
}

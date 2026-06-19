// Ground-truth generator for the pure vertical-velocity math in
// net.minecraft.world.level.block.HoneyBlock (Minecraft 26.1.2).
//
// Drives the REAL class: the two private statics getOldDeltaY(double) and
// getNewDeltaY(double) are invoked PURELY REFLECTIVELY (they are package-private/
// private and take/return a primitive double, so no world, entity, registry or
// bootstrap is required). We never replicate their bodies here — we call the real
// methods and dump their exact outputs.
//
// These two functions carry a 1:1 fidelity trap: the literal 0.98F is a *float* used
// inside a *double* expression, so Java widens it to 0.9800000190734863 before the
// divide/multiply. A naive port using a plain double 0.98 diverges in the low bits.
//
//   HoneyBlock.java:76-78  getOldDeltaY(double deltaY): return deltaY / 0.98F + 0.08;
//   HoneyBlock.java:80-82  getNewDeltaY(double deltaY): return (deltaY - 0.08) * 0.98F;
//
// Output: tab-separated <TAG>\t<input bits>\t<output bits>. Doubles are emitted as the
// raw IEEE-754 bits (Double.doubleToRawLongBits, decimal long) so the comparison is
// exact and NaN/sign-of-zero faithful.
//
//   tools/run_groundtruth.ps1 -Tool HoneyBlockParity -Out mcpp/build/honey_block.tsv

import java.lang.reflect.Method;

@SuppressWarnings({"deprecation", "unchecked"})
public class HoneyBlockParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return Long.toString(Double.doubleToRawLongBits(v)); }

    public static void main(String[] args) throws Exception {
        // Pure value math; bootstrap is unnecessary but harmless if the class touches
        // any static init. Guarded so a failure here never aborts the dump.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — these statics need nothing bootstrapped
        }

        Class<?> cls = Class.forName("net.minecraft.world.level.block.HoneyBlock");
        Method mOld = cls.getDeclaredMethod("getOldDeltaY", double.class);
        Method mNew = cls.getDeclaredMethod("getNewDeltaY", double.class);
        mOld.setAccessible(true);
        mNew.setAccessible(true);

        for (double dy : inputs()) {
            double oldDy = (Double) mOld.invoke(null, dy);
            double newDy = (Double) mNew.invoke(null, dy);
            // OLD  <deltaY bits>  <getOldDeltaY bits>
            O.println("OLD\t" + d(dy) + "\t" + d(oldDy));
            // NEW  <deltaY bits>  <getNewDeltaY bits>
            O.println("NEW\t" + d(dy) + "\t" + d(newDy));
        }
    }

    // A broad finite grid of physical & edge-case deltaY values. No NaN/Inf (the slide
    // logic only ever feeds finite velocities), but we include the exact thresholds the
    // caller compares against (-0.08, -0.13), the constant -0.05 that getNewDeltaY is
    // always called with, zero (both signs), subnormals, and large magnitudes so the
    // float->double-widened 0.98F arithmetic is exercised across the whole exponent range.
    static double[] inputs() {
        java.util.ArrayList<Double> v = new java.util.ArrayList<>();

        // Exact constants from the slide logic.
        double[] anchors = {
            0.0, -0.0, 0.08, -0.08, 0.13, -0.13, 0.05, -0.05,
            // getOldDeltaY threshold solved for deltaY (oldDeltaY == -0.08 / -0.13):
            (-0.08 - 0.08) * 0.98, (-0.13 - 0.08) * 0.98,
            // SLIDE_STARTS_WHEN_VERTICAL_SPEED_IS_AT_LEAST etc.
            0.5, -0.5, 1.0, -1.0, 0.98, -0.98,
            Double.MIN_VALUE, -Double.MIN_VALUE,           // smallest subnormal
            Double.MIN_NORMAL, -Double.MIN_NORMAL,
            Math.nextUp(0.0), Math.nextDown(0.0),
        };
        for (double a : anchors) v.add(a);

        // Fine sweep over the physically-relevant velocity band [-4, 4] at 0.0009765625
        // (= 1/1024) steps — dense enough to surface any rounding divergence.
        for (int i = -4096; i <= 4096; i++) v.add(i / 1024.0);

        // Coarse sweep over larger magnitudes (clamped/extreme fall & launch speeds).
        for (int i = -100; i <= 100; i++) v.add(i * 0.37);
        double[] big = {
            5.0, -5.0, 10.0, -10.0, 64.0, -64.0, 123.456, -123.456,
            1000.0, -1000.0, 1.0E6, -1.0E6, 1.0E9, -1.0E9, 1.0E12, -1.0E12,
            3.9999999, -3.9999999, 0.0799999, -0.0799999, 0.1300001, -0.1300001,
        };
        for (double b : big) v.add(b);

        double[] out = new double[v.size()];
        for (int i = 0; i < out.length; i++) out[i] = v.get(i);
        return out;
    }
}

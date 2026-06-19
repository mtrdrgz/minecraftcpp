// Ground-truth generator for net.minecraft.world.level.levelgen.synth.NoiseUtils
// (26.1.2) using the REAL decompiled class. The only ported method is
//
//   public static double biasTowardsExtreme(double noise, double factor)
//       return noise + Math.sin(Math.PI * noise) * factor / Math.PI;
//
// It is public + static + pure (no RandomSource / registries / world), so no
// Bootstrap is required — we call the real method directly. The two
// parityNoiseOctaveConfigString overloads are diagnostic String.format helpers
// and are intentionally not part of the gate (they format text, no math).
//
// The C++ test (NoiseUtilsParityTest) recomputes biasTowardsExtreme and must match
// BIT-FOR-BIT; the double output is exchanged as a raw IEEE-754 bit pattern.
//
//   mcpp/tools/run_groundtruth.ps1 -Tool NoiseUtilsParity -Out mcpp/build/noise_utils.tsv
//
// Row format:  BIAS_TOWARDS_EXTREME\t<noise:double-bits>\t<factor:double-bits>\t<result:double-bits>

import net.minecraft.world.level.levelgen.synth.NoiseUtils;

public class NoiseUtilsParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    static void rowBias(double noise, double factor) {
        double r = NoiseUtils.biasTowardsExtreme(noise, factor);
        O.println("BIAS_TOWARDS_EXTREME\t" + d(noise) + "\t" + d(factor) + "\t" + d(r));
    }

    public static void main(String[] args) throws Exception {
        // biasTowardsExtreme's natural domain: noise is a perlin/simplex sample,
        // roughly [-1, 1] but the callers feed un-clamped values too, and factor is
        // a fixed bias amount (e.g. 0.5 in DensityFunctions / weirdness offsets).
        // FINITE/PHYSICAL inputs only — no NaN/Inf/-0.0.
        java.util.ArrayList<Double> noises = new java.util.ArrayList<>();
        java.util.ArrayList<Double> factors = new java.util.ArrayList<>();

        // Dense sweep of noise across and a bit beyond [-1, 1] in 1/64 steps. This
        // walks the full Math.sin(PI*noise) period (and a couple beyond) so every
        // sign/curvature region of the bias is exercised.
        for (int i = -160; i <= 160; i++) noises.add(i / 64.0);

        // Exact knots where Math.sin(PI*noise) hits 0 / +-1 (n integer / half-int):
        double[] knots = {
            0.0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5, 2.0, -2.0,
            0.25, -0.25, 0.75, -0.75, 0.125, 0.375,
            0.3333333333333333, 0.6666666666666666, -0.3333333333333333,
            0.1, -0.1, 0.9, -0.9, 0.99, -0.99, 0.01, -0.01,
            0.123456789, -0.987654321, 0.7071067811865476, -0.7071067811865476
        };
        for (double k : knots) noises.add(k);

        // Bias factors actually used / plausible: 0 (no-op), the common 0.5/1.0,
        // and a fractional spread to exercise the multiply/divide chain.
        double[] facs = {
            0.0, 0.5, 1.0, -0.5, -1.0, 0.25, 0.75, 1.5, 2.0,
            0.1, 0.3, 0.6, 0.9, 0.333333, 0.111111, 0.987654321, 3.14159
        };
        for (double f : facs) factors.add(f);

        for (double noise : noises)
            for (double factor : factors)
                rowBias(noise, factor);
    }
}

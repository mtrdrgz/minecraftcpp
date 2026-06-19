// Ground truth for net.minecraft.util.valueproviders.ClampedNormalFloat.
//
// Emits, for a battery of (mean, deviation, min, max) configs x seeds, the raw
// IEEE-754 bits of sample() for the first N draws of a freshly-seeded
// RandomSource.create(seed) (== LegacyRandomSource(seed)). The C++ side seeds an
// identical mc::levelgen::LegacyRandomSource and recomputes
//   Mth.clamp(Mth.normal(random, mean, deviation), min, max)
//     = clamp(mean + (float)random.nextGaussian() * deviation, min, max)
// bit-for-bit.
//
// Rows (tab-separated):
//   CNF <meanBits> <devBits> <minBits> <maxBits> <seed> <count> <s0Bits> <s1Bits> ...
// floats are %08x of Float.floatToRawIntBits; seed/count are decimal.
//
// We call the REAL static method
//   ClampedNormalFloat.sample(RandomSource, float, float, float, float)
// via reflection (it is public, but reflection keeps us robust and uses exactly
// the shipped bytecode). Same RandomSource INSTANCE is threaded across the N
// draws so the nextNextGaussian cache state evolves exactly as in the engine.
//
// Build/run: tools/run_groundtruth.ps1 -Tool ClampedNormalFloatParity -Out mcpp/build/clamped_normal_float.tsv

import java.lang.reflect.Method;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.ClampedNormalFloat;

public class ClampedNormalFloatParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) {
        return Integer.toHexString(Float.floatToRawIntBits(v));
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Real static: float sample(RandomSource, float mean, float dev, float min, float max)
        Method sample = ClampedNormalFloat.class.getDeclaredMethod(
            "sample", RandomSource.class, float.class, float.class, float.class, float.class);
        sample.setAccessible(true);

        // Finite/physical configs: (mean, deviation, min, max) with max >= min
        // (the codec rejects max < min, so we never test that). Spread of means,
        // deviations (incl. 0 -> degenerate to mean, still clamped), and clamp
        // windows that variously: contain the mean, lie entirely above/below it
        // (saturating clamp), and are degenerate (min == max).
        float[][] configs = {
            { 0.0f,  1.0f,  -2.0f,  2.0f },
            { 0.0f,  1.0f,  -10.0f, 10.0f },
            { 5.0f,  2.0f,   0.0f,  10.0f },
            { 5.0f,  2.0f,   6.0f,  10.0f },   // window entirely above mean
            { 5.0f,  2.0f,   0.0f,  4.0f },    // window entirely below mean
            { -3.5f, 0.75f, -5.0f,  0.0f },
            { 12.25f,4.5f,   8.0f,  16.0f },
            { 0.5f,  0.0f,   0.0f,  1.0f },    // deviation 0 -> always mean, clamped
            { 100.0f,25.0f,  50.0f, 150.0f },
            { 0.0f,  3.0f,   1.0f,  1.0f },    // degenerate window min==max
            { 1.0f,  0.1f,   0.9f,  1.1f },    // tight window around mean
            { -1.0f, 1.0f,  -1.0f,  1.0f },
            { 7.0f,  10.0f, -50.0f, 50.0f },   // wide window, large deviation
            { 0.0f,  0.25f, -1.0f,  1.0f },
            { 64.0f, 8.0f,   64.0f, 64.0f },   // degenerate at a larger value
        };

        long[] seeds = { 0L, 1L, 2L, 42L, 123L, 1000L, -1L, -42L,
                         123456789L, 987654321L, 0xdeadbeefL, 1234567890123L };

        final int N = 16; // draws per (config, seed)

        for (float[] c : configs) {
            float mean = c[0], dev = c[1], min = c[2], max = c[3];
            for (long seed : seeds) {
                RandomSource r = RandomSource.create(seed);
                StringBuilder sb = new StringBuilder();
                sb.append("CNF\t")
                  .append(f(mean)).append('\t')
                  .append(f(dev)).append('\t')
                  .append(f(min)).append('\t')
                  .append(f(max)).append('\t')
                  .append(seed).append('\t')
                  .append(N);
                for (int i = 0; i < N; i++) {
                    float s = (Float) sample.invoke(null, r, mean, dev, min, max);
                    sb.append('\t').append(f(s));
                }
                O.println(sb.toString());
            }
        }
    }
}

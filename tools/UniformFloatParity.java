// Ground truth for net.minecraft.util.valueproviders.UniformFloat.
//
// UniformFloat.sample(random) == Mth.randomBetween(random, min, max)
//                             == random.nextFloat() * (max - min) + min
//
// We drive the REAL UniformFloat.sample over a battery of (min,max) pairs, each
// against a freshly-seeded REAL RandomSource, drawing N consecutive samples so
// the whole nextFloat() stream is exercised. Two RNG flavours are covered, both
// certified on the C++ side and both seeded IDENTICALLY there:
//   LEG  -> RandomSource.create(seed)        == LegacyRandomSource(seed)
//   XOR  -> new XoroshiroRandomSource(seed)
//
// Output rows (tab-separated):
//   UF  <rng> <seedDec> <minBits> <maxBits> <s0Bits> ... <s7Bits>
// where every float is %08x of Float.floatToRawIntBits (bit-exact).
//
// Run via tools/run_groundtruth.ps1 -Tool UniformFloatParity -Out mcpp/build/uniform_float.tsv
import java.lang.reflect.Constructor;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.UniformFloat;

public class UniformFloatParity {
    static final java.io.PrintStream O = System.out;
    static final int N = 8; // samples per (rng,seed,provider) row

    // Build a real RandomSource of the requested flavour, seeded identically to
    // the C++ side. "LEG" = LegacyRandomSource via the public factory;
    // "XOR" = XoroshiroRandomSource via its (long) constructor (reflection,
    // setAccessible because the ctor is not part of the RandomSource interface).
    static RandomSource makeRng(String rng, long seed) throws Exception {
        if (rng.equals("LEG")) {
            return RandomSource.create(seed);
        } else if (rng.equals("XOR")) {
            Class<?> c = Class.forName("net.minecraft.world.level.levelgen.XoroshiroRandomSource");
            Constructor<?> ctor = c.getDeclaredConstructor(long.class);
            ctor.setAccessible(true);
            return (RandomSource) ctor.newInstance(seed);
        }
        throw new IllegalArgumentException("rng " + rng);
    }

    static void emit(String rng, long seed, float min, float max) throws Exception {
        // of() enforces max > min; mirror that here (finite/physical inputs only).
        UniformFloat u = UniformFloat.of(min, max);
        RandomSource r = makeRng(rng, seed);
        StringBuilder sb = new StringBuilder();
        sb.append("UF\t").append(rng).append('\t').append(seed)
          .append('\t').append(String.format("%08x", Float.floatToRawIntBits(min)))
          .append('\t').append(String.format("%08x", Float.floatToRawIntBits(max)));
        for (int i = 0; i < N; i++) {
            float v = u.sample(r);
            sb.append('\t').append(String.format("%08x", Float.floatToRawIntBits(v)));
        }
        O.println(sb.toString());
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Finite, physical (min,max) pairs with max > min (the only legal domain).
        float[][] ranges = {
            {0.0f, 1.0f},
            {-1.0f, 1.0f},
            {-2.0f, 3.0f},
            {0.0f, 0.0078125f},      // tiny positive range (power of two)
            {0.5f, 0.5000001f},      // near-degenerate range -> rounding stress
            {-100.0f, 100.0f},
            {1.0f, 16.0f},
            {3.0f, 3.140625f},       // (used by some features)
            {0.10000000149011612f, 0.6000000238418579f}, // non-representable decimals
            {-0.5f, 0.5f},
            {10.0f, 10000.0f},       // wide range -> exercises float rounding
            {0.0f, 6.2831855f},      // ~2*pi (cherry/rotation style angles)
        };

        long[] seeds = { 0L, 1L, 2L, 42L, -1L, 1234567890123456789L,
                         -8888888888888888888L, 1000000L, 987654321L };

        String[] rngs = { "LEG", "XOR" };

        for (String rng : rngs) {
            for (long seed : seeds) {
                for (float[] mm : ranges) {
                    emit(rng, seed, mm[0], mm[1]);
                }
            }
        }
    }
}

// Ground truth for net.minecraft.util.valueproviders.ConstantFloat.
//
//   public record ConstantFloat(float value) implements FloatProvider {
//      public static final ConstantFloat ZERO = new ConstantFloat(0.0F);
//      public static ConstantFloat of(final float value) {
//         return value == 0.0F ? ZERO : new ConstantFloat(value);
//      }
//      public float sample(RandomSource random) { return this.value; }
//      public float min() { return this.value; }
//      public float max() { return this.value; }
//   }
//
// We drive the REAL ConstantFloat over a battery of finite/physical floats. For
// each input we build the provider via the public factory of(v) (so the
// value==0.0F singleton folding is exercised), then dump the raw float bits of:
//   value()  min()  max()  sample(rng)
// sample() ignores the RandomSource and must not advance it, so we additionally
// dump the RNG's nextInt() AFTER sampling 4 times: it must equal the nextInt()
// of a fresh RNG of the same seed (proving sample() consumed zero RNG draws).
//
// Output rows (tab-separated), every float = %08x of Float.floatToRawIntBits:
//   CF  <inBits>  <valueBits>  <minBits>  <maxBits>  <s0Bits> <s1Bits> <s2Bits> <s3Bits>  <postNextIntDec>  <freshNextIntDec>
//
// Run via tools/run_groundtruth.ps1 -Tool ConstantFloatParity -Out mcpp/build/int_provider_constant_float.tsv
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.ConstantFloat;

public class ConstantFloatParity {
    static final java.io.PrintStream O = System.out;
    static final int N = 4; // sample() calls per row (must not advance the RNG)

    static void emit(float in, long seed) throws Exception {
        // of() folds value==0.0F (incl. -0.0F) to the ZERO singleton (value=+0.0F).
        ConstantFloat cf = ConstantFloat.of(in);

        float value = cf.value();
        float min = cf.min();
        float max = cf.max();

        RandomSource r = RandomSource.create(seed);
        float[] s = new float[N];
        for (int i = 0; i < N; i++) {
            s[i] = cf.sample(r); // must return value and consume NO RNG draws
        }
        int postNextInt = r.nextInt();            // first untouched draw after N samples
        int freshNextInt = RandomSource.create(seed).nextInt(); // baseline first draw

        StringBuilder sb = new StringBuilder();
        sb.append("CF")
          .append('\t').append(String.format("%08x", Float.floatToRawIntBits(in)))
          .append('\t').append(String.format("%08x", Float.floatToRawIntBits(value)))
          .append('\t').append(String.format("%08x", Float.floatToRawIntBits(min)))
          .append('\t').append(String.format("%08x", Float.floatToRawIntBits(max)));
        for (int i = 0; i < N; i++) {
            sb.append('\t').append(String.format("%08x", Float.floatToRawIntBits(s[i])));
        }
        sb.append('\t').append(postNextInt)
          .append('\t').append(freshNextInt);
        O.println(sb.toString());
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Finite, physical float inputs. Includes both signed zeros (of() singleton
        // folding: -0.0F -> +0.0F), tiny/large magnitudes, negatives, and a few
        // non-power-of-two decimals that stress raw-bit fidelity.
        float[] inputs = {
            0.0f,
            -0.0f,
            1.0f,
            -1.0f,
            0.5f,
            -0.5f,
            2.0f,
            3.0f,
            16.0f,
            -100.0f,
            100.0f,
            0.0078125f,
            0.10000000149011612f,   // 0.1f (non-representable decimal)
            0.6000000238418579f,    // 0.6f
            3.140625f,
            6.2831855f,             // ~2*pi
            255.0f,
            -255.0f,
            1.0E-7f,
            1.0E7f,
            0.30000001192092896f,   // 0.3f
            123.456f,
            -123.456f,
            1.0E-30f,
            1.0E30f,
        };

        long[] seeds = { 0L, 1L, 42L, -1L, 1234567890123456789L, 987654321L };

        for (long seed : seeds) {
            for (float in : inputs) {
                emit(in, seed);
            }
        }
    }
}

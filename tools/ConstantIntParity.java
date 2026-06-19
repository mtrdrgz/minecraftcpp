// Reference generator for the C++ mc::valueproviders::ConstantInt port (verified,
// lives in mcpp/src/world/level/levelgen/IntProvider.h). Runs the REAL decompiled
// net.minecraft.util.valueproviders.ConstantInt, so the sampled values + bounds are
// exact ground truth.
//
//   javac -cp 26.1.2/client.jar:26.1.2/libs/* -d <out> mcpp/tools/ConstantIntParity.java
//   java  -cp <out>:26.1.2/client.jar:26.1.2/libs/* ConstantIntParity > int_provider_constant.tsv
//
// ConstantInt(value): sample(random) = value (IGNORES the rng), minInclusive() =
// maxInclusive() = value, and ConstantInt.of(0) returns the ZERO singleton while
// of(v!=0) builds a fresh record. To prove sample() ignores the RandomSource we draw
// 8 samples per case after stepping BOTH a LegacyRandomSource and an
// XoroshiroRandomSource (both already verified 1:1) with several seeds; every drawn
// sample must equal `value` regardless of rng state.
//
// Row format (tab separated, emitted to STDOUT — the runner captures stdout):
//   BOUND\t<value>\t<minInclusive>\t<maxInclusive>\t<isZeroSingleton>
//   SAMP\t<value>\t<rng>\t<seed>\t<s0>..<s7>     (8 samples, decimal ints)
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.ConstantInt;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;

public class ConstantIntParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Finite/physical int values: zero (singleton), small +/- counts, typical
        // worldgen heights/spreads, and the 32-bit extremes.
        int[] values = {
            0, 1, -1, 2, 3, 5, 7, 8, 16, 17, 31, 32, 63, 64, 100, 127, 128, 255, 256,
            -2, -3, -5, -8, -16, -64, -100, -128, 1000, -1000, 65535, -65536, 1234567,
            Integer.MAX_VALUE, Integer.MIN_VALUE
        };
        long[] seeds = { 0L, 1L, 42L, 123456789L, -987654321L, 2147483647L, 1234567890123L };

        StringBuilder out = new StringBuilder();
        for (int v : values) {
            ConstantInt c = ConstantInt.of(v);

            // BOUND: minInclusive == maxInclusive == value; isZeroSingleton proves the
            // of(0) -> ZERO identity branch in ConstantInt.of.
            int isZeroSingleton = (c == ConstantInt.ZERO) ? 1 : 0;
            out.append("BOUND\t").append(v)
               .append('\t').append(c.minInclusive())
               .append('\t').append(c.maxInclusive())
               .append('\t').append(isZeroSingleton)
               .append('\n');

            for (long seed : seeds) {
                // LegacyRandomSource path
                RandomSource leg = new LegacyRandomSource(seed);
                StringBuilder sbL = new StringBuilder("SAMP\t").append(v)
                        .append("\tLEG\t").append(seed);
                for (int i = 0; i < 8; i++) {
                    sbL.append('\t').append(c.sample(leg));
                    leg.nextInt(); // perturb rng state; sample must still ignore it
                }
                out.append(sbL).append('\n');

                // XoroshiroRandomSource path
                RandomSource xor = new XoroshiroRandomSource(seed);
                StringBuilder sbX = new StringBuilder("SAMP\t").append(v)
                        .append("\tXOR\t").append(seed);
                for (int i = 0; i < 8; i++) {
                    sbX.append('\t').append(c.sample(xor));
                    xor.nextInt();
                }
                out.append(sbX).append('\n');
            }
        }
        O.print(out);
    }
}

// Reference value generator for the C++ net.minecraft.util.valueproviders.BiasedToBottomInt
// port (verified against mcpp/src/world/level/levelgen/IntProvider.h ::
// mc::valueproviders::BiasedToBottomInt).
//
// Runs the REAL decompiled net.minecraft.util.valueproviders.BiasedToBottomInt from
// client.jar so the emitted sample sequences are exact ground truth. The sole
// behaviour under test is:
//
//     sample = minInclusive + random.nextInt(random.nextInt(maxInclusive - minInclusive + 1) + 1)
//
// We drive it with a REAL net.minecraft.world.level.levelgen.LegacyRandomSource seeded
// per row, and the C++ test seeds an mc::levelgen::LegacyRandomSource IDENTICALLY. The
// inner-then-outer nextInt ordering is fixed by the JVM's left-to-right argument
// evaluation, which the C++ port reproduces (inner drawn first, then outer).
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/BiasedToBottomIntParity.java
//   java  -cp <out>:26.1.2/client.jar BiasedToBottomIntParity > biased_to_bottom_int.tsv
//
// Rows are tab-separated:
//   B2B  <min>  <max>  <seed>  <s0..s7>
//        BiasedToBottomInt.of(min,max).sample(LegacyRandomSource(seed)) drawn 8 times
//        in sequence from one continuously-advancing RNG (no reseed between draws).
//
// O is captured at class load so any bootstrap chatter on stdout stays out of the TSV.
public class BiasedToBottomIntParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> b2bClass =
            Class.forName("net.minecraft.util.valueproviders.BiasedToBottomInt");

        // BiasedToBottomInt.of(int, int) is public static; resolve & open it anyway.
        java.lang.reflect.Method of = b2bClass.getDeclaredMethod("of", int.class, int.class);
        of.setAccessible(true);

        // sample(RandomSource) is declared on the record; the param is the interface
        // net.minecraft.util.RandomSource. Resolve by name+arity so we don't depend on
        // exact signature spelling, then open it (record method may be non-public to us).
        java.lang.reflect.Method sample = null;
        for (java.lang.reflect.Method m : b2bClass.getDeclaredMethods()) {
            if (m.getName().equals("sample") && m.getParameterCount() == 1) {
                sample = m;
                break;
            }
        }
        sample.setAccessible(true);

        // FINITE / PHYSICAL battery: min<=max always (BiasedToBottomInt requires
        // max>=min; nextInt(bound) throws for bound<=0). Includes min==max (degenerate,
        // bound==1 twice), tiny ranges, larger ranges, and negative/positive offsets as
        // appear in real feature configs (e.g. counts, y-offsets).
        int[][] ranges = new int[][] {
            { 0, 0 },      // degenerate: range 1, always min
            { 0, 1 },
            { 0, 2 },
            { 0, 3 },
            { 0, 7 },
            { 0, 15 },
            { 0, 16 },     // straddle power-of-two bound (max-min+1 == 17)
            { 1, 1 },
            { 1, 3 },
            { 2, 5 },
            { 3, 3 },      // degenerate non-zero
            { -4, 4 },     // symmetric around zero
            { -8, -1 },    // wholly negative
            { -2, 10 },
            { 5, 20 },
            { 1, 31 },     // bound 31 (non power of two) -> reject loop exercised
            { 0, 63 },
            { 0, 64 },     // bound 65
            { 7, 7 },
            { 10, 100 },
        };

        long[] seeds = { 0L, 1L, 2L, 42L, 123456789L, -1L, -987654321L,
                         2147483647L, -1234567890123456789L, 8675309L };

        for (int[] r : ranges) {
            int min = r[0];
            int max = r[1];
            Object provider = of.invoke(null, min, max);
            for (long seed : seeds) {
                net.minecraft.world.level.levelgen.LegacyRandomSource rng =
                    new net.minecraft.world.level.levelgen.LegacyRandomSource(seed);
                StringBuilder sb = new StringBuilder();
                sb.append("B2B\t").append(min).append('\t').append(max)
                  .append('\t').append(seed);
                for (int i = 0; i < 8; i++) {
                    int s = (Integer) sample.invoke(provider, rng);
                    sb.append('\t').append(s);
                }
                O.println(sb.toString());
            }
        }
    }
}

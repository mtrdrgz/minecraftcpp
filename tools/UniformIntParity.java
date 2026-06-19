// Reference generator for the C++ mc::valueproviders::UniformInt port
// (mcpp/src/world/level/levelgen/IntProvider.h). Runs the REAL decompiled
// net.minecraft.util.valueproviders.UniformInt so sampled values and
// min/maxInclusive are exact ground truth.
//
//   javac -cp 26.1.2/client.jar:26.1.2/libs/* -d <out> mcpp/tools/UniformIntParity.java
//   java  -cp <out>:26.1.2/client.jar:26.1.2/libs/* UniformIntParity uniform_int.tsv
//
// UniformInt.sample(RandomSource) == Mth.randomBetweenInclusive(random, min, max)
//   == random.nextInt(max - min + 1) + min.
// We exercise it against both verified RandomSource flavours (LegacyRandomSource
// and XoroshiroRandomSource), each reset per (provider, seed) case and seeded
// identically on the C++ side. minInclusive()/maxInclusive() (the getMinValue /
// getMaxValue accessors) are echoed once per provider.
//
// Rows (tab-separated):
//   META   <name>  <min>  <max>                    -> minInclusive/maxInclusive
//   SAMPLE <name>  <rng>  <seed>  s0..s7            -> 8 sequential samples
// rng: 0 = LegacyRandomSource(seed), 1 = XoroshiroRandomSource(seed).
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;

import java.util.LinkedHashMap;
import java.util.Map;

public class UniformIntParity {
    static final java.io.PrintStream O = System.out;

    static RandomSource rng(int kind, long seed) {
        return kind == 0 ? new LegacyRandomSource(seed) : new XoroshiroRandomSource(seed);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Built after Bootstrap: UniformInt.MAP_CODEC static init touches registries.
        Map<String, UniformInt> providers = new LinkedHashMap<>();
        providers.put("uni1_3", UniformInt.of(1, 3));
        providers.put("uni0_7", UniformInt.of(0, 7));
        providers.put("uni0_0", UniformInt.of(0, 0));       // degenerate single value
        providers.put("uni5_5", UniformInt.of(5, 5));       // degenerate single value
        providers.put("uni_neg", UniformInt.of(-5, 10));    // crosses zero
        providers.put("uni_allneg", UniformInt.of(-12, -3));// fully negative range
        providers.put("uni0_255", UniformInt.of(0, 255));   // power-of-two-ish bound (256)
        providers.put("uni0_99", UniformInt.of(0, 99));     // non-power-of-two bound (reject loop)
        providers.put("uni1_1000", UniformInt.of(1, 1000));
        providers.put("uni_big", UniformInt.of(-32768, 32767));

        long[] seeds = { 0L, 1L, 42L, 100L, 123456789L, -987654321L, 7640891576956012809L };

        String outPath = args.length > 0 ? args[0] : "uniform_int.tsv";
        // Bootstrap reroutes System.out through Log4j; write the TSV directly.
        try (java.io.PrintWriter out = new java.io.PrintWriter(outPath)) {
            for (Map.Entry<String, UniformInt> e : providers.entrySet()) {
                String name = e.getKey();
                UniformInt p = e.getValue();
                out.println("META\t" + name + "\t" + p.minInclusive() + "\t" + p.maxInclusive());
            }
            for (Map.Entry<String, UniformInt> e : providers.entrySet()) {
                String name = e.getKey();
                UniformInt p = e.getValue();
                for (int kind = 0; kind <= 1; kind++) {
                    for (long seed : seeds) {
                        RandomSource r = rng(kind, seed);
                        StringBuilder sb = new StringBuilder("SAMPLE\t")
                            .append(name).append('\t').append(kind).append('\t').append(seed);
                        for (int i = 0; i < 8; i++) {
                            sb.append('\t').append(p.sample(r));
                        }
                        out.println(sb);
                    }
                }
            }
        }
    }
}

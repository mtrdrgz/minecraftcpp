// Ground-truth generator for the C++ mc::valueproviders::WeightedListInt port
// (lives in mcpp/src/world/level/levelgen/IntProvider.h).
//
// Runs the REAL decompiled net.minecraft.util.valueproviders.WeightedListInt,
// which delegates to net.minecraft.util.random.WeightedList.getRandomOrThrow:
//   selection = random.nextInt(totalWeight); value = selector.get(selection);
// then value.sample(random). WeightedList picks Flat (totalWeight < 64) or
// Compact (totalWeight >= 64); both map the same `selection` index to the same
// entry, so the C++ cumulative-walk must agree for BOTH regimes. We deliberately
// include distributions on either side of the FLAT_THRESHOLD (64).
//
// build/run (paths per memory/env-build-toolchain):
//   javac -cp "26.1.2/client.jar;26.1.2/libs/*" -d <out> mcpp/tools/WeightedListIntParity.java
//   java  -cp "<out>;26.1.2/client.jar;26.1.2/libs/*" WeightedListIntParity weighted_list_int.tsv
//
// Emits tab-separated rows: WLI <name> <seed> <s0..s7>  (8 samples, ints decimal)
// and a META row per distribution: META <name> <totalWeight> <minInclusive> <maxInclusive>.
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.util.valueproviders.BiasedToBottomInt;
import net.minecraft.util.valueproviders.ClampedInt;
import net.minecraft.util.valueproviders.ConstantInt;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.util.valueproviders.TrapezoidInt;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.util.valueproviders.WeightedListInt;
import net.minecraft.world.level.levelgen.LegacyRandomSource;

import java.util.LinkedHashMap;
import java.util.Map;

public class WeightedListIntParity {
    static final java.io.PrintStream O = System.out;

    static IntProvider weighted(Object... pairs) {
        WeightedList.Builder<IntProvider> b = WeightedList.builder();
        for (int i = 0; i < pairs.length; i += 2) {
            b.add((IntProvider) pairs[i], (Integer) pairs[i + 1]);
        }
        return new WeightedListInt(b.build());
    }

    // Built lazily (after Bootstrap) — the *Int CODEC static fields touch registries.
    static Map<String, IntProvider> distributions() {
        Map<String, IntProvider> m = new LinkedHashMap<>();

        // --- Flat regime (totalWeight < 64) ---
        // two constants, total 20
        m.put("wl_19_1", weighted(ConstantInt.of(0), 19, ConstantInt.of(1), 1));
        // three constants, total 6, single-step weights
        m.put("wl_c3", weighted(ConstantInt.of(7), 1, ConstantInt.of(8), 2, ConstantInt.of(9), 3));
        // mixed sub-providers: second random draw exercised
        m.put("wl_mixed", weighted(UniformInt.of(1, 2), 3, ConstantInt.of(5), 1));
        // four heterogeneous providers, total 10
        m.put("wl_het", weighted(
            ConstantInt.of(-3), 4,
            UniformInt.of(0, 7), 3,
            BiasedToBottomInt.of(0, 4), 2,
            ClampedInt.of(UniformInt.of(-5, 10), 0, 8), 1));
        // total exactly 63 (just under threshold -> Flat)
        m.put("wl_flat63", weighted(
            ConstantInt.of(100), 60,
            UniformInt.of(200, 205), 3));

        // --- Compact regime (totalWeight >= 64) ---
        // total exactly 64 (boundary -> Compact)
        m.put("wl_compact64", weighted(
            ConstantInt.of(100), 60,
            UniformInt.of(200, 205), 4));
        // total 130, mixed providers incl. trapezoid
        m.put("wl_compact_big", weighted(
            ConstantInt.of(-10), 50,
            UniformInt.of(1, 6), 40,
            TrapezoidInt.of(-8, 8, 0), 25,
            BiasedToBottomInt.of(2, 9), 15));
        // total 200, single dominant + tail
        m.put("wl_compact200", weighted(
            ConstantInt.of(0), 199,
            ConstantInt.of(1), 1));

        return m;
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Map<String, IntProvider> DISTS = distributions();
        long[] seeds = { 0L, 1L, 2L, 42L, 123456789L, 987654321L, 555L, 8675309L };

        String outPath = args.length > 0 ? args[0] : "weighted_list_int.tsv";
        // Write to a file: Bootstrap reroutes System.out through Log4j (prefixes lines).
        try (java.io.PrintWriter out = new java.io.PrintWriter(outPath)) {
            for (Map.Entry<String, IntProvider> e : DISTS.entrySet()) {
                String name = e.getKey();
                IntProvider p = e.getValue();
                out.println("META\t" + name + "\t" + 0 + "\t" + p.minInclusive() + "\t" + p.maxInclusive());
                for (long seed : seeds) {
                    RandomSource r = new LegacyRandomSource(seed);
                    StringBuilder sb = new StringBuilder("WLI\t").append(name).append('\t').append(seed);
                    for (int i = 0; i < 8; i++) {
                        sb.append('\t').append(p.sample(r));
                    }
                    out.println(sb);
                }
            }
        }
    }
}

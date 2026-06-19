// Reference generator for the C++ mc::valueproviders::ClampedInt port (verified,
// lives in mcpp/src/world/level/levelgen/IntProvider.h). Runs the REAL decompiled
// net.minecraft.util.valueproviders.ClampedInt wrapping real ConstantInt /
// UniformInt / BiasedToBottomInt sources, so the clamped sampled values are exact
// ground truth.
//
//   javac -cp 26.1.2/client.jar:26.1.2/libs/* -d <out> mcpp/tools/ClampedIntParity.java
//   java  -cp <out>:26.1.2/client.jar:26.1.2/libs/* ClampedIntParity <out.tsv>
//
// ClampedInt.sample(random) = Mth.clamp(source.sample(random), min, max), and
// Mth.clamp(int) = Math.min(Math.max(value, min), max). RandomSource is a
// LegacyRandomSource (already verified 1:1) re-seeded identically per case, so the
// underlying draw sequence matches the C++ mc::levelgen::LegacyRandomSource bit-for-bit.
//
// NOTE: ClampedInt is a record but its component accessors minInclusive() /
// maxInclusive() are OVERRIDDEN (they combine the window with the wrapped source's
// range), so the raw clamp window cannot be read back via accessors. We therefore
// carry the raw [min,max] window alongside each case.
//
// Row format (tab separated):
//   SAMP\t<name>\t<seed>\t<min>\t<max>\t<s0>..<s7>     (8 clamped samples, decimal ints)
//   BOUND\t<name>\t<min>\t<max>\t<minInclusive>\t<maxInclusive>   (combined effective bounds)
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.BiasedToBottomInt;
import net.minecraft.util.valueproviders.ClampedInt;
import net.minecraft.util.valueproviders.ConstantInt;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.level.levelgen.LegacyRandomSource;

import java.util.ArrayList;
import java.util.List;

public class ClampedIntParity {
    static final java.io.PrintStream O = System.out;

    static final class Case {
        final String name;
        final int min, max;
        final ClampedInt provider;
        Case(String name, IntProvider source, int min, int max) {
            this.name = name;
            this.min = min;
            this.max = max;
            this.provider = ClampedInt.of(source, min, max);
        }
    }

    // The clamp window is deliberately chosen to bite from below, above, both, or
    // neither, and to widen past the source range (no-op clamp) too.
    static List<Case> cases() {
        List<Case> c = new ArrayList<>();
        // wrapped UniformInt(-5,10), clamp variations
        c.add(new Case("uni_-5_10__0_8",    UniformInt.of(-5, 10),    0, 8));    // both sides bite
        c.add(new Case("uni_-5_10__-3_12",  UniformInt.of(-5, 10),   -3, 12));   // only low bites
        c.add(new Case("uni_-5_10__-8_5",   UniformInt.of(-5, 10),   -8, 5));    // only high bites
        c.add(new Case("uni_-5_10__-20_20", UniformInt.of(-5, 10),  -20, 20));   // no-op (wider)
        c.add(new Case("uni_0_15__7_7",     UniformInt.of(0, 15),     7, 7));    // degenerate single value
        c.add(new Case("uni_-100_100__-9_9",UniformInt.of(-100, 100),-9, 9));    // tight window, big source
        // wrapped ConstantInt (source.sample is constant; clamp may pass or pin)
        c.add(new Case("const5__0_8",       ConstantInt.of(5),        0, 8));    // pass-through
        c.add(new Case("const5__6_10",      ConstantInt.of(5),        6, 10));   // pinned to min
        c.add(new Case("const5__0_3",       ConstantInt.of(5),        0, 3));    // pinned to max
        // wrapped BiasedToBottomInt
        c.add(new Case("bias0_20__0_5",     BiasedToBottomInt.of(0, 20),   0, 5));    // clamps the long tail
        c.add(new Case("bias0_20__3_18",    BiasedToBottomInt.of(0, 20),   3, 18));   // both sides
        c.add(new Case("bias-10_10__-2_2",  BiasedToBottomInt.of(-10, 10), -2, 2));
        return c;
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        List<Case> CASES = cases();
        long[] seeds = { 0L, 1L, 42L, 123456789L, -987654321L, 2147483647L, 1234567890123L };

        // Write straight to a file: Bootstrap.bootStrap() reroutes System.out through
        // Log4j, which would prefix every line.
        String outPath = args.length > 0 ? args[0] : "clamped_int.tsv";
        try (java.io.PrintWriter out = new java.io.PrintWriter(outPath)) {
            for (Case cs : CASES) {
                // BOUND row: ClampedInt.minInclusive()/maxInclusive() combine the
                // window with the wrapped source's own range.
                out.println("BOUND\t" + cs.name + "\t" + cs.min + "\t" + cs.max + "\t"
                        + cs.provider.minInclusive() + "\t" + cs.provider.maxInclusive());

                for (long seed : seeds) {
                    RandomSource r = new LegacyRandomSource(seed);
                    StringBuilder sb = new StringBuilder("SAMP\t").append(cs.name)
                            .append('\t').append(seed)
                            .append('\t').append(cs.min)
                            .append('\t').append(cs.max);
                    for (int i = 0; i < 8; i++) {
                        sb.append('\t').append(cs.provider.sample(r));
                    }
                    out.println(sb);
                }
            }
        }
    }
}

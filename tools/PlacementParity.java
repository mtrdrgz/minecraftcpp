// Reference generator for the C++ IntProvider + pure PlacementModifier ports.
// Runs the REAL decompiled classes from client.jar so the sampled counts and
// scatter positions are exact ground truth.
//
//   javac -cp 26.1.2/client.jar:26.1.2/libs/* -d <out> mcpp/tools/PlacementParity.java
//   java  -cp <out>:26.1.2/client.jar:26.1.2/libs/* PlacementParity > placement_cases.tsv
//
// The pure placement modifiers ignore the PlacementContext, so it is passed as
// null. RandomSource is a LegacyRandomSource (already verified 1:1) reset per case.
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.util.valueproviders.BiasedToBottomInt;
import net.minecraft.util.valueproviders.ClampedInt;
import net.minecraft.util.valueproviders.ConstantInt;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.util.valueproviders.WeightedListInt;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.placement.CountPlacement;
import net.minecraft.world.level.levelgen.placement.InSquarePlacement;
import net.minecraft.world.level.levelgen.placement.PlacementModifier;
import net.minecraft.world.level.levelgen.placement.RandomOffsetPlacement;
import net.minecraft.world.level.levelgen.placement.RarityFilter;

import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

public class PlacementParity {
    static IntProvider weighted(Object... pairs) {
        WeightedList.Builder<IntProvider> b = WeightedList.builder();
        for (int i = 0; i < pairs.length; i += 2) {
            b.add((IntProvider) pairs[i], (Integer) pairs[i + 1]);
        }
        return new WeightedListInt(b.build());
    }

    // Built lazily (not in static initialisers) so Bootstrap.bootStrap() runs
    // first — the *Int CODEC static fields touch BuiltInRegistries.
    static Map<String, IntProvider> intProviders() {
        return Map.ofEntries(
            Map.entry("const5", ConstantInt.of(5)),
            Map.entry("uni1_3", UniformInt.of(1, 3)),
            Map.entry("uni0_7", UniformInt.of(0, 7)),
            Map.entry("bias0_4", BiasedToBottomInt.of(0, 4)),
            Map.entry("clamp_uni", ClampedInt.of(UniformInt.of(-5, 10), 0, 8)),
            Map.entry("wl_19_1", weighted(ConstantInt.of(0), 19, ConstantInt.of(1), 1)),
            Map.entry("wl_mixed", weighted(UniformInt.of(1, 2), 3, ConstantInt.of(5), 1))
        );
    }

    static Map<String, PlacementModifier> modifiers() {
        return Map.ofEntries(
            Map.entry("insquare", InSquarePlacement.spread()),
            Map.entry("count64", CountPlacement.of(64)),
            Map.entry("count_uni", CountPlacement.of(UniformInt.of(2, 5))),
            Map.entry("count_wl", CountPlacement.of(weighted(ConstantInt.of(0), 19, ConstantInt.of(1), 1))),
            Map.entry("rarity7", RarityFilter.onAverageOnceEvery(7)),
            Map.entry("rarity32", RarityFilter.onAverageOnceEvery(32)),
            Map.entry("roff_v", RandomOffsetPlacement.of(ConstantInt.of(0), UniformInt.of(-2, 2))),
            Map.entry("roff_h", RandomOffsetPlacement.of(UniformInt.of(-3, 3), ConstantInt.of(0)))
        );
    }

    public static void main(String[] args) {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Map<String, IntProvider> INT_PROVIDERS = intProviders();
        Map<String, PlacementModifier> MODIFIERS = modifiers();
        long[] seeds = { 0L, 1L, 42L, 123456789L, -987654321L };
        int ox = 100, oy = 64, oz = -50;

        // Write straight to a file: Bootstrap.bootStrap() reroutes System.out
        // through Log4j, which would prefix every line.
        String outPath = args.length > 0 ? args[0] : "placement_cases.tsv";
        try (java.io.PrintWriter out = new java.io.PrintWriter(outPath)) {
            for (long seed : seeds) {
                for (String name : INT_PROVIDERS.keySet().stream().sorted().collect(Collectors.toList())) {
                    RandomSource r = new LegacyRandomSource(seed);
                    StringBuilder sb = new StringBuilder("INT\t").append(name).append('\t').append(seed);
                    for (int i = 0; i < 8; i++) {
                        sb.append('\t').append(INT_PROVIDERS.get(name).sample(r));
                    }
                    out.println(sb);
                }
                for (String name : MODIFIERS.keySet().stream().sorted().collect(Collectors.toList())) {
                    RandomSource r = new LegacyRandomSource(seed);
                    List<BlockPos> positions = MODIFIERS.get(name).getPositions(null, r, new BlockPos(ox, oy, oz)).collect(Collectors.toList());
                    String posStr = positions.stream().map(p -> p.getX() + ":" + p.getY() + ":" + p.getZ()).collect(Collectors.joining(","));
                    out.println("POS\t" + name + "\t" + seed + "\t" + ox + "\t" + oy + "\t" + oz + "\t" + positions.size() + "\t" + (posStr.isEmpty() ? "-" : posStr));
                }
            }
        } catch (java.io.IOException e) {
            throw new RuntimeException(e);
        }
    }
}

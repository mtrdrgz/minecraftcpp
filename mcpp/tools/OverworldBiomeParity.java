// Reference value generator for the C++ OverworldBiomeBuilder / Climate.RTree port.
//
// Unlike BiomeManagerExpected.java (which re-implements the algorithm), this
// generator invokes the REAL decompiled Minecraft code from client.jar so the
// emitted values are exact ground truth for the 1:1 port.
//
// Compile / run (needs the local Mojang client.jar; never committed):
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/OverworldBiomeParity.java
//   java  -cp <out>:26.1.2/client.jar OverworldBiomeParity <outDir>
//
// It writes two TSV files into <outDir>:
//   climate_params.tsv  - the canonical overworld parameter list, in builder order
//                         (12 interval longs + offset + biome id per row)
//   climate_cases.tsv   - deterministic target points and the biome returned by
//                         Climate.ParameterList.findValue (the production RTree path)
import com.mojang.datafixers.util.Pair;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.Climate;
import net.minecraft.world.level.biome.OverworldBiomeBuilder;

import java.io.PrintWriter;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import java.util.function.Consumer;

public class OverworldBiomeParity {
    // Builds the overworld parameter list exactly like the non-debug branch of
    // OverworldBiomeBuilder.addBiomes(), but invoking the three private builder
    // methods directly so we never touch SharedConstants (whose static init
    // would otherwise need a full game bootstrap).
    static List<Pair<Climate.ParameterPoint, ResourceKey<Biome>>> buildList() throws Exception {
        OverworldBiomeBuilder builder = new OverworldBiomeBuilder();
        List<Pair<Climate.ParameterPoint, ResourceKey<Biome>>> out = new ArrayList<>();
        Consumer<Pair<Climate.ParameterPoint, ResourceKey<Biome>>> consumer = out::add;
        for (String name : new String[]{"addOffCoastBiomes", "addInlandBiomes", "addUndergroundBiomes"}) {
            Method m = OverworldBiomeBuilder.class.getDeclaredMethod(name, Consumer.class);
            m.setAccessible(true);
            m.invoke(builder, consumer);
        }
        return out;
    }

    static void writeParams(List<Pair<Climate.ParameterPoint, ResourceKey<Biome>>> list, String path) throws Exception {
        try (PrintWriter w = new PrintWriter(path)) {
            for (Pair<Climate.ParameterPoint, ResourceKey<Biome>> pair : list) {
                Climate.ParameterPoint p = pair.getFirst();
                w.printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s%n",
                        p.temperature().min(), p.temperature().max(),
                        p.humidity().min(), p.humidity().max(),
                        p.continentalness().min(), p.continentalness().max(),
                        p.erosion().min(), p.erosion().max(),
                        p.depth().min(), p.depth().max(),
                        p.weirdness().min(), p.weirdness().max(),
                        p.offset(),
                        pair.getSecond().identifier().toString());
            }
        }
    }

    public static void main(String[] args) throws Exception {
        String outDir = args.length > 0 ? args[0] : ".";
        List<Pair<Climate.ParameterPoint, ResourceKey<Biome>>> list = buildList();
        System.out.println("param count: " + list.size());
        writeParams(list, outDir + "/climate_params.tsv");

        Climate.ParameterList<ResourceKey<Biome>> pl = new Climate.ParameterList<>(list);

        // Deterministic targets spanning a little beyond the [-1,1] climate box
        // extents so that box boundaries (where distance ties occur) are hit.
        Random rng = new Random(123456789L);
        int n = 300000;
        int treeVsBrute = 0;
        try (PrintWriter w = new PrintWriter(outDir + "/climate_cases.tsv");
             PrintWriter tie = new PrintWriter(outDir + "/climate_tie_cases.tsv")) {
            for (int i = 0; i < n; i++) {
                long t = rng.nextInt(26001) - 13000;
                long h = rng.nextInt(26001) - 13000;
                long c = rng.nextInt(26001) - 13000;
                long e = rng.nextInt(26001) - 13000;
                long d = rng.nextInt(26001) - 13000;
                long wd = rng.nextInt(26001) - 13000;
                Climate.TargetPoint target = new Climate.TargetPoint(t, h, c, e, d, wd);
                ResourceKey<Biome> tree = pl.findValue(target);
                ResourceKey<Biome> brute = pl.findValueBruteForce(target);
                if (tree != brute) {
                    treeVsBrute++;
                    // Cases where the production RTree path disagrees with brute force
                    // (distance ties) - the hardest parity cases for the C++ port.
                    tie.printf("%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s%n", t, h, c, e, d, wd,
                            tree.identifier().toString(), brute.identifier().toString());
                }
                w.printf("%d\t%d\t%d\t%d\t%d\t%d\t%s%n", t, h, c, e, d, wd, tree.identifier().toString());
            }
        }
        System.out.println("wrote " + n + " cases; tree-vs-bruteforce divergences: " + treeVsBrute);
    }
}

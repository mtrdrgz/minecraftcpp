// Ground-truth generator for the C++ NoiseRouter / RandomState wiring.
// Runs the REAL decompiled net.minecraft RandomState so the emitted density
// samples are exact (validates RandomState.mapAll/NoiseWiringHelper, audit #6).
//
//   tools/run_groundtruth.ps1 -Tool DensityParity -Out mcpp/build/density_cases.tsv
//
// Each row: seed  x  y  z  function  rawDoubleBits   (raw IEEE bits = exact).
import net.minecraft.core.HolderLookup;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.world.level.levelgen.DensityFunction;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.NoiseRouter;
import net.minecraft.world.level.levelgen.Noises;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.synth.NormalNoise;

public class DensityParity {
    static String bits(double v) { return Long.toString(Double.doubleToRawLongBits(v)); }

    public static void main(String[] args) {
        // Bootstrap installs a Log4j redirect on System.out that prefixes every line
        // ("[..] [main/INFO]: [STDOUT]: ..."), which would corrupt the TSV. Capture
        // the real stdout BEFORE bootStrap and emit through it.
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion(); // DataFixerUpper needs the game version
        net.minecraft.server.Bootstrap.bootStrap();
        HolderLookup.Provider holders = VanillaRegistries.createLookup();

        long[] seeds = { 0L, 1L, 123456789L, -987654321L };
        // Fixed probe coordinates spanning surface, caves and far-out columns.
        int[][] coords = {
            {0, 0, 0}, {0, 64, 0}, {16, 64, 16}, {100, 32, -200}, {-50, 128, 300},
            {1000, 0, 1000}, {-1234, 80, 5678}, {37, -40, 37}, {8, 96, -8}, {-300, 16, -300}
        };
        String[] names = { "temperature", "vegetation", "continents", "erosion", "depth", "ridges", "final_density" };

        for (long seed : seeds) {
            RandomState rs = RandomState.create(holders, NoiseGeneratorSettings.OVERWORLD, seed);
            NoiseRouter r = rs.router();
            DensityFunction[] fns = {
                r.temperature(), r.vegetation(), r.continents(), r.erosion(), r.depth(), r.ridges(), r.finalDensity()
            };
            for (int[] c : coords) {
                DensityFunction.FunctionContext ctx = new DensityFunction.SinglePointContext(c[0], c[1], c[2]);
                for (int i = 0; i < fns.length; i++) {
                    out.println(seed + "\t" + c[0] + "\t" + c[1] + "\t" + c[2] + "\t" + names[i]
                        + "\t" + bits(fns[i].compute(ctx)));
                }
            }

            // Raw seeded-noise probes: isolate noise seeding/sampling from the
            // density-function wiring. Each is rs.getOrCreateNoise(key).getValue(x,y,z).
            @SuppressWarnings("unchecked")
            ResourceKey<NormalNoise.NoiseParameters>[] rawKeys =
                new ResourceKey[]{ Noises.SHIFT, Noises.TEMPERATURE, Noises.CONTINENTALNESS, Noises.EROSION, Noises.RIDGE };
            String[] rawNames = { "raw:offset", "raw:temperature", "raw:continentalness", "raw:erosion", "raw:ridge" };
            for (int[] c : coords) {
                for (int i = 0; i < rawKeys.length; i++) {
                    double v = rs.getOrCreateNoise(rawKeys[i]).getValue(c[0], c[1], c[2]);
                    out.println(seed + "\t" + c[0] + "\t" + c[1] + "\t" + c[2] + "\t" + rawNames[i] + "\t" + bits(v));
                }
            }

            // Positional-chain probes (forkPositional + fromHashOf): the seeding
            // primitives worldgen_random_parity does NOT cover. Emitted as raw longs.
            var f = new net.minecraft.world.level.levelgen.XoroshiroRandomSource(seed).forkPositional();
            out.println(seed + "\t0\t0\t0\tfork:minecraft:offset\t" + f.fromHashOf("minecraft:offset").nextLong());
            out.println(seed + "\t0\t0\t0\tfork:octave_-3\t" + f.fromHashOf("octave_-3").nextLong());
        }
    }
}

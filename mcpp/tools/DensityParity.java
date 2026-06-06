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
import net.minecraft.world.level.levelgen.RandomState;

public class DensityParity {
    static String bits(double v) { return Long.toString(Double.doubleToRawLongBits(v)); }

    public static void main(String[] args) {
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
                    System.out.println(seed + "\t" + c[0] + "\t" + c[1] + "\t" + c[2] + "\t" + names[i]
                        + "\t" + bits(fns[i].compute(ctx)));
                }
            }
        }
    }
}

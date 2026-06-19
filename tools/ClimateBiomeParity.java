// Ground-truth generator for the overworld climate sampler + biome source.
//
// Runs the REAL decompiled RandomState / Climate.Sampler /
// MultiNoiseBiomeSourceParameterList.Preset.OVERWORLD path and emits exact
// quantized climate target values plus the selected biome id.
//
//   tools/run_groundtruth.ps1 -Tool ClimateBiomeParity -Out mcpp/build/climate_biome_cases.tsv
//
// Each row:
// seed  quartX  quartY  quartZ  temperature  humidity  continentalness
// erosion  depth  weirdness  biome
import java.util.List;
import net.minecraft.core.HolderLookup;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.Climate;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.NoiseRouter;
import net.minecraft.world.level.levelgen.RandomState;

public class ClimateBiomeParity {
    public static void main(String[] args) {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        HolderLookup.Provider holders = VanillaRegistries.createLookup();

        long[] seeds = { 0L, 1L, 123456789L, -987654321L };
        int[][] quartCoords = {
            {0, 0, 0}, {0, 16, 0}, {4, 16, 4}, {25, 8, -50}, {-13, 32, 75},
            {250, 0, 250}, {-309, 20, 1420}, {9, -10, 9}, {2, 24, -2}, {-75, 4, -75},
            {1024, 16, -1024}, {-2048, 24, 2048}, {5000, 20, 5000}, {-5000, 20, -5000}
        };

        Climate.ParameterList<ResourceKey<Biome>> overworld =
            MultiNoiseBiomeSourceParameterList.knownPresets().get(MultiNoiseBiomeSourceParameterList.Preset.OVERWORLD);

        for (long seed : seeds) {
            RandomState rs = RandomState.create(holders, NoiseGeneratorSettings.OVERWORLD, seed);
            NoiseRouter r = rs.router();
            Climate.Sampler sampler = new Climate.Sampler(
                r.temperature(), r.vegetation(), r.continents(), r.erosion(), r.depth(), r.ridges(), List.of()
            );
            for (int[] q : quartCoords) {
                Climate.TargetPoint target = sampler.sample(q[0], q[1], q[2]);
                String biome = overworld.findValue(target).identifier().toString();
                out.println(seed + "\t" + q[0] + "\t" + q[1] + "\t" + q[2]
                    + "\t" + target.temperature()
                    + "\t" + target.humidity()
                    + "\t" + target.continentalness()
                    + "\t" + target.erosion()
                    + "\t" + target.depth()
                    + "\t" + target.weirdness()
                    + "\t" + biome);
            }
        }
    }
}

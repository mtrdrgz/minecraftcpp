// Ground-truth generator for vanilla overworld base terrain columns.
//
// This samples NoiseBasedChunkGenerator.getBaseColumn(), which uses the real
// Java NoiseChunk / aquifer / ore-vein material rule path before surface rules,
// carvers, structures, features or decoration.
//
//   tools/run_groundtruth.ps1 -Tool BaseTerrainColumnParity -Out mcpp/build/base_terrain_columns.tsv
//
// Each row: seed  blockX  blockZ  y  block_id
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;

public class BaseTerrainColumnParity {
    public static void main(String[] args) {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        HolderLookup.Provider holders = VanillaRegistries.createLookup();

        Holder<NoiseGeneratorSettings> settings =
            holders.lookupOrThrow(Registries.NOISE_SETTINGS).getOrThrow(NoiseGeneratorSettings.OVERWORLD);
        Holder<MultiNoiseBiomeSourceParameterList> overworldPreset =
            holders.lookupOrThrow(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST)
                .getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD);
        NoiseBasedChunkGenerator generator =
            new NoiseBasedChunkGenerator(MultiNoiseBiomeSource.createFromPreset(overworldPreset), settings);
        LevelHeightAccessor height = LevelHeightAccessor.create(
            settings.value().noiseSettings().minY(),
            settings.value().noiseSettings().height()
        );

        long[] seeds = { 0L, 1L, 123456789L, -987654321L };
        int[][] columns = {
            {0, 0}, {1, 1}, {15, 15}, {16, 16}, {-1, -1}, {-16, 31},
            {100, -200}, {-50, 300}, {1000, 1000}, {-1234, 5678},
            {37, 37}, {8, -8}, {-300, -300}, {2048, -2048}
        };

        for (long seed : seeds) {
            RandomState randomState = RandomState.create(holders, NoiseGeneratorSettings.OVERWORLD, seed);
            for (int[] c : columns) {
                NoiseColumn column = generator.getBaseColumn(c[0], c[1], height, randomState);
                for (int y = height.getMinY(); y < height.getMinY() + height.getHeight(); y++) {
                    String block = column.getBlock(y).getBlock().builtInRegistryHolder().key().identifier().toString();
                    out.println(seed + "\t" + c[0] + "\t" + c[1] + "\t" + y + "\t" + block);
                }
            }
        }
    }
}

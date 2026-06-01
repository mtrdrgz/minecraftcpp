// Reference generator for the C++ BlockStateProvider port. Calls the real
// decompiled providers' getState() directly (no level needed) so the chosen
// states are exact ground truth. SimpleBlockFeature's own logic is a faithful
// direct port; its only world-dependent step (BlockState.canSurvive) is the
// block-behaviour boundary, verified once that subsystem is ported.
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.level.levelgen.feature.stateproviders.NoiseThresholdProvider;
import net.minecraft.world.level.levelgen.feature.stateproviders.WeightedStateProvider;
import net.minecraft.world.level.levelgen.synth.NormalNoise;

import java.io.PrintWriter;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public class BlockStateProviderParity {
    static String key(BlockState state) {
        return net.minecraft.core.registries.BuiltInRegistries.BLOCK.getKey(state.getBlock()).toString();
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Map<String, BlockStateProvider> providers = new LinkedHashMap<>();
        providers.put("simple_grass", BlockStateProvider.simple(Blocks.SHORT_GRASS));
        WeightedList<BlockState> wl = WeightedList.<BlockState>builder()
            .add(Blocks.SHORT_GRASS.defaultBlockState(), 3)
            .add(Blocks.FERN.defaultBlockState(), 1)
            .add(Blocks.DANDELION.defaultBlockState(), 1)
            .build();
        providers.put("weighted", new WeightedStateProvider(wl));
        WeightedList<BlockState> wl2 = WeightedList.<BlockState>builder()
            .add(Blocks.POPPY.defaultBlockState(), 2)
            .add(Blocks.DANDELION.defaultBlockState(), 2)
            .add(Blocks.BLUE_ORCHID.defaultBlockState(), 1)
            .add(Blocks.ALLIUM.defaultBlockState(), 1)
            .add(Blocks.OXEYE_DAISY.defaultBlockState(), 1)
            .build();
        providers.put("weighted2", new WeightedStateProvider(wl2));

        long[] seeds = { 0L, 1L, 42L, 123456789L, -987654321L, 7L };
        BlockPos pos = new BlockPos(10, 64, -7);

        try (PrintWriter out = new PrintWriter(args.length > 0 ? args[0] : "block_state_provider_cases.tsv")) {
            for (Map.Entry<String, BlockStateProvider> e : providers.entrySet()) {
                for (long seed : seeds) {
                    RandomSource r = new LegacyRandomSource(seed);
                    StringBuilder sb = new StringBuilder("BSP\t").append(e.getKey()).append('\t').append(seed);
                    for (int i = 0; i < 8; i++) sb.append('\t').append(key(e.getValue().getState(null, r, pos)));
                    out.println(sb);
                }
            }

            // Raw NormalNoise values (directly verifies the C++ NormalNoise port).
            NormalNoise noise = NormalNoise.create(new WorldgenRandom(new LegacyRandomSource(2345L)),
                    new NormalNoise.NoiseParameters(0, 1.0));
            double scale = 0.005F; // float widened to double, as NoiseBasedStateProvider does
            int[][] npos = { {0, 64, 0}, {16, 70, 32}, {100, 64, -50}, {-37, 64, 89}, {1000, 80, 2000}, {5, 64, 5}, {-13, 64, -29} };
            for (int[] p : npos) {
                out.println("NOISE\t" + p[0] + "\t" + p[1] + "\t" + p[2] + "\t"
                        + Double.doubleToRawLongBits(noise.getValue(p[0] * scale, p[1] * scale, p[2] * scale)));
            }

            // NoiseThresholdProvider (the flower_plain config) getState.
            NoiseThresholdProvider flowers = new NoiseThresholdProvider(2345L, new NormalNoise.NoiseParameters(0, 1.0),
                    0.005F, -0.8F, 0.33333334F, Blocks.DANDELION.defaultBlockState(),
                    List.of(Blocks.ORANGE_TULIP.defaultBlockState(), Blocks.RED_TULIP.defaultBlockState(),
                            Blocks.PINK_TULIP.defaultBlockState(), Blocks.WHITE_TULIP.defaultBlockState()),
                    List.of(Blocks.POPPY.defaultBlockState(), Blocks.AZURE_BLUET.defaultBlockState(),
                            Blocks.OXEYE_DAISY.defaultBlockState(), Blocks.CORNFLOWER.defaultBlockState()));
            for (int[] p : npos) {
                for (long seed : seeds) {
                    RandomSource r = new LegacyRandomSource(seed);
                    out.println("BSPN\t" + p[0] + "\t" + p[1] + "\t" + p[2] + "\t" + seed + "\t"
                            + key(flowers.getState(null, r, new BlockPos(p[0], p[1], p[2]))));
                }
            }
        }
    }
}

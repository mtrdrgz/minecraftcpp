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
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.level.levelgen.feature.stateproviders.WeightedStateProvider;

import java.io.PrintWriter;
import java.util.LinkedHashMap;
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
        }
    }
}

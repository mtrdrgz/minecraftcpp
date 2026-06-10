// Ground truth for net.minecraft.client.renderer.LevelRenderer.getLightCoords (LevelRenderer.java:
// 1441-1452) — the per-block packed light-coords. Drives the REAL static method with a brightness-
// getter lambda (controls packedBrightness) + real BlockStates (real emissiveRendering / light
// emission) + a minimal BlockAndLightGetter stub (only getLightEngine throws; never reached for the
// chosen blocks). Emits the resolved (emissive, lightEmission, packedBrightness) inputs + output.
//
//   tools/run_groundtruth.ps1 -Tool LevelRenderLightParity -Out mcpp/build/level_render_light.tsv
//
// Row: LC \t <emissive 0/1> \t <packedBrightness> \t <lightEmission> \t <getLightCoords output>

import net.minecraft.client.renderer.LevelRenderer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.LightCoordsUtil;
import net.minecraft.world.level.BlockAndLightGetter;
import net.minecraft.world.level.EmptyBlockGetter;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.lighting.LevelLightEngine;

public class LevelRenderLightParity {
    static final java.io.PrintStream O = System.out;
    static final BlockPos POS = BlockPos.ZERO;

    static final BlockAndLightGetter LEVEL = new BlockAndLightGetter() {
        public net.minecraft.world.level.block.entity.BlockEntity getBlockEntity(BlockPos p) { return EmptyBlockGetter.INSTANCE.getBlockEntity(p); }
        public BlockState getBlockState(BlockPos p) { return EmptyBlockGetter.INSTANCE.getBlockState(p); }
        public net.minecraft.world.level.material.FluidState getFluidState(BlockPos p) { return EmptyBlockGetter.INSTANCE.getFluidState(p); }
        public int getHeight() { return EmptyBlockGetter.INSTANCE.getHeight(); }
        public int getMinY() { return EmptyBlockGetter.INSTANCE.getMinY(); }
        public LevelLightEngine getLightEngine() { throw new UnsupportedOperationException(); }
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        BlockState[] states = {
            Blocks.AIR.defaultBlockState(),
            Blocks.STONE.defaultBlockState(),
            Blocks.GLOWSTONE.defaultBlockState(),      // emission 15
            Blocks.SEA_LANTERN.defaultBlockState(),    // emission 15
            Blocks.TORCH.defaultBlockState(),          // emission 14
            Blocks.MAGMA_BLOCK.defaultBlockState(),    // emission 3, emissiveRendering true
            Blocks.REDSTONE_LAMP.defaultBlockState(),  // emission 0 (unlit)
            Blocks.JACK_O_LANTERN.defaultBlockState(), // emission 15
        };

        // packedBrightness values: pack(block, sky) over a spread, plus full-bright + zero + odd bits.
        java.util.List<Integer> packed = new java.util.ArrayList<>();
        int[] lvls = {0, 1, 4, 7, 11, 14, 15};
        for (int b : lvls) for (int s : lvls) packed.add(LightCoordsUtil.pack(b, s));
        packed.add(0);
        packed.add(15728880);
        packed.add(LightCoordsUtil.pack(3, 12));
        packed.add(LightCoordsUtil.pack(15, 0));

        for (BlockState state : states) {
            boolean emissive = state.emissiveRendering(LEVEL, POS);
            int lightEmission = state.getLightEmission();
            for (int pb : packed) {
                int got = LevelRenderer.getLightCoords((lvl, p) -> pb, LEVEL, state, POS);
                O.println("LC\t" + (emissive ? 1 : 0) + "\t" + pb + "\t" + lightEmission + "\t" + got);
            }
        }
    }
}

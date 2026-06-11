// Ground truth for BlockModelLighter.prepareQuadFlat (BlockModelLighter.java:198-217) — the flat
// (non-AO) block-lighting path. Drives the REAL public method with a level stub (position-varying
// brightness) + real BlockStates (STONE/AIR/GLOWSTONE/MAGMA_BLOCK for collision/emissive/emission),
// reads QuadInstance.getColor/getLightCoords (all 4 vertices identical), emits the resolved inputs.
//
//   tools/run_groundtruth.ps1 -Tool BlockModelLighterFlatParity -Out mcpp/build/bml_flat.tsv

import com.mojang.blaze3d.vertex.QuadInstance;
import net.minecraft.client.renderer.block.BlockAndTintGetter;
import net.minecraft.client.renderer.block.BlockModelLighter;
import net.minecraft.client.resources.model.geometry.BakedQuad;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.LightCoordsUtil;
import net.minecraft.world.level.CardinalLighting;
import net.minecraft.world.level.ColorResolver;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.lighting.LevelLightEngine;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.material.Fluids;
import org.joml.Vector3f;
import org.joml.Vector3fc;

public class BlockModelLighterFlatParity {
    static final java.io.PrintStream O = System.out;
    static String f(float x) { return String.format("%08x", Float.floatToRawIntBits(x)); }
    static final BlockPos CENTER = BlockPos.ZERO;

    static int blockBrightness(BlockPos p) { return Math.floorMod(p.getX() * 1 + p.getY() * 3 + p.getZ() * 7 + 5, 16); }
    static int skyBrightness(BlockPos p) { return Math.floorMod(p.getX() * 2 + p.getY() * 5 + p.getZ() * 11 + 9, 16); }
    static int packedAt(BlockPos p) { return LightCoordsUtil.pack(blockBrightness(p), skyBrightness(p)); }

    static BlockAndTintGetter levelWith(final CardinalLighting card) {
        return new BlockAndTintGetter() {
            public CardinalLighting cardinalLighting() { return card; }
            public int getBlockTint(BlockPos p, ColorResolver c) { return -1; }
            public BlockState getBlockState(BlockPos p) { return Blocks.AIR.defaultBlockState(); }
            public FluidState getFluidState(BlockPos p) { return Fluids.EMPTY.defaultFluidState(); }
            public int getHeight() { return 0; }
            public int getMinY() { return 0; }
            public BlockEntity getBlockEntity(BlockPos p) { return null; }
            public LevelLightEngine getLightEngine() { throw new UnsupportedOperationException(); }
            public int getBrightness(LightLayer layer, BlockPos p) { return layer == LightLayer.BLOCK ? blockBrightness(p) : skyBrightness(p); }
        };
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        BlockModelLighter lighter = new BlockModelLighter();
        java.lang.reflect.Field faceCubicF = BlockModelLighter.class.getDeclaredField("faceCubic");
        faceCubicF.setAccessible(true);

        BlockState[] states = {
            Blocks.STONE.defaultBlockState(), Blocks.AIR.defaultBlockState(),
            Blocks.GLOWSTONE.defaultBlockState(), Blocks.MAGMA_BLOCK.defaultBlockState()};
        CardinalLighting[] cards = {CardinalLighting.DEFAULT, CardinalLighting.NETHER};
        int[] lightModes = {-1, LightCoordsUtil.pack(7, 9)};  // -1 = compute, else passthrough

        for (Direction d : Direction.values()) {
            for (float[][] quad : quadsFor(d)) {
                for (BlockState st : states) {
                    for (CardinalLighting card : cards) {
                        for (int lm : lightModes) {
                            for (boolean sh : new boolean[]{true, false}) {
                                BlockAndTintGetter level = levelWith(card);
                                Vector3fc[] vp = new Vector3fc[4];
                                for (int i = 0; i < 4; i++) vp[i] = new Vector3f(quad[i][0], quad[i][1], quad[i][2]);
                                BakedQuad.MaterialInfo mat = new BakedQuad.MaterialInfo(null, null, null, -1, sh, 0);
                                BakedQuad bq = new BakedQuad(vp[0], vp[1], vp[2], vp[3], 0L, 0L, 0L, 0L, d, mat);

                                QuadInstance qi = new QuadInstance();
                                lighter.prepareQuadFlat(level, st, CENTER, lm, bq, qi);
                                boolean faceCubic = faceCubicF.getBoolean(lighter);

                                boolean emissive = st.emissiveRendering(level, CENTER);
                                int emission = st.getLightEmission();
                                boolean collisionFull = st.isCollisionShapeFullBlock(level, CENTER);
                                int packAtPos = packedAt(CENTER);
                                int packAtPosDir = packedAt(CENTER.relative(d));

                                StringBuilder sb = new StringBuilder("FLAT\t").append(d.ordinal())
                                    .append('\t').append(card == CardinalLighting.NETHER ? "NETHER" : "DEFAULT")
                                    .append('\t').append(sh ? 1 : 0).append('\t').append(collisionFull ? 1 : 0)
                                    .append('\t').append(emissive ? 1 : 0).append('\t').append(emission)
                                    .append('\t').append(packAtPos).append('\t').append(packAtPosDir).append('\t').append(lm);
                                for (int i = 0; i < 4; i++) sb.append('\t').append(f(quad[i][0])).append('\t').append(f(quad[i][1])).append('\t').append(f(quad[i][2]));
                                sb.append('\t').append(qi.getColor(0)).append('\t').append(qi.getLightCoords(0));
                                // verify all 4 identical (flat path) — emit a sanity flag
                                boolean uniform = qi.getColor(0) == qi.getColor(3) && qi.getLightCoords(0) == qi.getLightCoords(3);
                                sb.append('\t').append(uniform ? 1 : 0).append('\t').append(faceCubic ? 1 : 0);
                                O.println(sb);
                            }
                        }
                    }
                }
            }
        }
    }

    static float[][][] quadsFor(Direction d) {
        return switch (d) {
            case DOWN -> new float[][][]{{{0,0,1},{0,0,0},{1,0,0},{1,0,1}}, {{.25f,0,.75f},{.25f,0,.25f},{.75f,0,.25f},{.75f,0,.75f}}};
            case UP -> new float[][][]{{{0,1,0},{0,1,1},{1,1,1},{1,1,0}}, {{.25f,1,.25f},{.25f,1,.75f},{.75f,1,.75f},{.75f,1,.25f}}};
            case NORTH -> new float[][][]{{{1,1,0},{1,0,0},{0,0,0},{0,1,0}}, {{.75f,.75f,0},{.75f,.25f,0},{.25f,.25f,0},{.25f,.75f,0}}};
            case SOUTH -> new float[][][]{{{0,1,1},{0,0,1},{1,0,1},{1,1,1}}, {{.25f,.75f,1},{.25f,.25f,1},{.75f,.25f,1},{.75f,.75f,1}}};
            case WEST -> new float[][][]{{{0,1,0},{0,0,0},{0,0,1},{0,1,1}}, {{0,.75f,.25f},{0,.25f,.25f},{0,.25f,.75f},{0,.75f,.75f}}};
            case EAST -> new float[][][]{{{1,1,1},{1,0,1},{1,0,0},{1,1,0}}, {{1,.75f,.75f},{1,.25f,.75f},{1,.25f,.25f},{1,.75f,.25f}}};
        };
    }
}

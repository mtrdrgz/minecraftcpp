// Ground truth for the ambient-occlusion BLEND in BlockModelLighter.prepareQuadAmbientOcclusion
// (BlockModelLighter.java:126-196). Drives the REAL method with a level stub: AIR everywhere (so
// every corner-neighbor is non-view-blocking -> all translucentN=true -> shade/lightCornerXY take
// the deterministic DIAGONAL lookup) + per-POSITION-varying brightness (override getBrightness).
// This gives a known LIGHT gradient (distinct lightN/lightCornerN/lightCenter) while shade stays
// uniform (AIR's getShadeBrightness). Reads QuadInstance.getColor/getLightCoords and emits them +
// the resolved inputs the C++ assembly re-applies.
//
//   tools/run_groundtruth.ps1 -Tool BlockModelLighterAOParity -Out mcpp/build/bml_ao.tsv

import com.mojang.blaze3d.vertex.QuadInstance;
import java.util.Locale;
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

public class BlockModelLighterAOParity {
    static final java.io.PrintStream O = System.out;
    static String f(float x) { return String.format("%08x", Float.floatToRawIntBits(x)); }
    static final BlockPos CENTER = BlockPos.ZERO;
    static BlockState AIR;

    // Position-dependent brightness (0..15), shared by the stub and the GT input computation.
    static int blockBrightness(BlockPos p) { return Math.floorMod(p.getX() * 1 + p.getY() * 3 + p.getZ() * 7 + 5, 16); }
    static int skyBrightness(BlockPos p) { return Math.floorMod(p.getX() * 2 + p.getY() * 5 + p.getZ() * 11 + 9, 16); }
    static int lightAt(BlockPos p) { return LightCoordsUtil.pack(blockBrightness(p), skyBrightness(p)); }

    static BlockAndTintGetter levelWith(final CardinalLighting card) {
        return new BlockAndTintGetter() {
            public CardinalLighting cardinalLighting() { return card; }
            public int getBlockTint(BlockPos p, ColorResolver c) { return -1; }
            public BlockState getBlockState(BlockPos p) { return AIR; }
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
        AIR = Blocks.AIR.defaultBlockState();

        BlockModelLighter lighter = new BlockModelLighter();
        java.lang.reflect.Field faceCubicF = BlockModelLighter.class.getDeclaredField("faceCubic");
        faceCubicF.setAccessible(true);
        // AdjacencyInfo.corners (Direction[]) per facing, via the certified table mirror (we need the
        // same corner Directions to compute the resolved light inputs).
        java.lang.reflect.Method adjFrom = Class.forName("net.minecraft.client.renderer.block.BlockModelLighter$AdjacencyInfo")
            .getDeclaredMethod("fromFacing", Direction.class);
        adjFrom.setAccessible(true);
        java.lang.reflect.Field cornersF = Class.forName("net.minecraft.client.renderer.block.BlockModelLighter$AdjacencyInfo")
            .getDeclaredField("corners");
        cornersF.setAccessible(true);

        float uniformShade = (float) lighter_getShade();

        CardinalLighting[] cards = {CardinalLighting.DEFAULT, CardinalLighting.NETHER};
        boolean[] shadeFlags = {true, false};

        for (Direction d : Direction.values()) {
            Direction[] corners = (Direction[]) cornersF.get(adjFrom.invoke(null, d));
            for (float[][] quad : quadsFor(d)) {
                for (CardinalLighting card : cards) {
                    for (boolean sh : shadeFlags) {
                        BlockAndTintGetter level = levelWith(card);
                        Vector3fc[] vp = new Vector3fc[4];
                        for (int i = 0; i < 4; i++) vp[i] = new Vector3f(quad[i][0], quad[i][1], quad[i][2]);
                        BakedQuad.MaterialInfo mat = new BakedQuad.MaterialInfo(null, null, null, -1, sh, 0);
                        BakedQuad bq = new BakedQuad(vp[0], vp[1], vp[2], vp[3], 0L, 0L, 0L, 0L, d, mat);

                        QuadInstance qi = new QuadInstance();
                        lighter.prepareQuadAmbientOcclusion(level, AIR, CENTER, bq, qi);
                        boolean faceCubic = faceCubicF.getBoolean(lighter);

                        // resolved light inputs (replicate the corner-walk; AIR => all translucent => diagonals)
                        BlockPos base = faceCubic ? CENTER.relative(d) : CENTER;
                        int l0 = lightAt(base.relative(corners[0]));
                        int l1 = lightAt(base.relative(corners[1]));
                        int l2 = lightAt(base.relative(corners[2]));
                        int l3 = lightAt(base.relative(corners[3]));
                        int lc02 = lightAt(base.relative(corners[0]).relative(corners[2]));
                        int lc03 = lightAt(base.relative(corners[0]).relative(corners[3]));
                        int lc12 = lightAt(base.relative(corners[1]).relative(corners[2]));
                        int lc13 = lightAt(base.relative(corners[1]).relative(corners[3]));
                        int lCenter = lightAt(CENTER.relative(d));  // AIR @ center+dir not solid-render => always

                        StringBuilder sb = new StringBuilder("AO\t").append(d.ordinal())
                            .append('\t').append(card == CardinalLighting.NETHER ? "NETHER" : "DEFAULT")
                            .append('\t').append(sh ? 1 : 0).append('\t').append(faceCubic ? 1 : 0)
                            .append('\t').append(f(uniformShade));
                        for (int i = 0; i < 4; i++) sb.append('\t').append(f(quad[i][0])).append('\t').append(f(quad[i][1])).append('\t').append(f(quad[i][2]));
                        sb.append('\t').append(l0).append('\t').append(l1).append('\t').append(l2).append('\t').append(l3);
                        sb.append('\t').append(lc02).append('\t').append(lc03).append('\t').append(lc12).append('\t').append(lc13).append('\t').append(lCenter);
                        for (int i = 0; i < 4; i++) sb.append('\t').append(qi.getColor(i));
                        for (int i = 0; i < 4; i++) sb.append('\t').append(qi.getLightCoords(i));
                        O.println(sb);
                    }
                }
            }
        }
    }

    // AIR getShadeBrightness (uniform). Reflection-free: just call it.
    static float lighter_getShade() {
        return AIR.getShadeBrightness(BlockAndTintGetter.EMPTY, CENTER);
    }

    // Quads per face: full (simple branch) + inset/partial (weighted branch) + interior partial.
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

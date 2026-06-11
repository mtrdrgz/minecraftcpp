// Ground truth for BlockModelLighter.prepareQuadShape (BlockModelLighter.java:219-274) — the
// per-quad geometry classification (faceShape[12] + facePartial + faceCubic). Reflection-drives the
// private method on a real BlockModelLighter with a constructed BakedQuad + real BlockStates
// (STONE => isCollisionShapeFullBlock true, AIR => false) + BlockAndTintGetter.EMPTY, then reads the
// instance fields.
//
//   tools/run_groundtruth.ps1 -Tool BlockModelLighterShapeParity -Out mcpp/build/bml_shape.tsv
//
// Row: SHP \t <dir> \t <collisionFull 0/1> \t p0x..p3z(12 %08x) \t faceShape[0..11](12 %08x) \t facePartial \t faceCubic

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.client.renderer.block.BlockAndTintGetter;
import net.minecraft.client.renderer.block.BlockModelLighter;
import net.minecraft.client.resources.model.geometry.BakedQuad;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import org.joml.Vector3f;

public class BlockModelLighterShapeParity {
    static final java.io.PrintStream O = System.out;
    static String f(float x) { return String.format("%08x", Float.floatToRawIntBits(x)); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        BlockModelLighter lighter = new BlockModelLighter();
        Class<?> cls = BlockModelLighter.class;
        Method prep = cls.getDeclaredMethod("prepareQuadShape",
            BlockAndTintGetter.class, BlockState.class, BlockPos.class, BakedQuad.class, boolean.class);
        prep.setAccessible(true);
        Field faceShapeF = cls.getDeclaredField("faceShape");
        Field facePartialF = cls.getDeclaredField("facePartial");
        Field faceCubicF = cls.getDeclaredField("faceCubic");
        faceShapeF.setAccessible(true); facePartialF.setAccessible(true); faceCubicF.setAccessible(true);

        BlockAndTintGetter level = BlockAndTintGetter.EMPTY;
        BlockPos pos = BlockPos.ZERO;
        BlockState[] states = {Blocks.STONE.defaultBlockState(), Blocks.AIR.defaultBlockState()};

        // Representative quads per direction: full face at the plane, partial inset, and an offset.
        for (Direction d : Direction.values()) {
            for (float[][] quad : quadsFor(d)) {
                Vector3f[] vp = new Vector3f[4];
                for (int i = 0; i < 4; i++) vp[i] = new Vector3f(quad[i][0], quad[i][1], quad[i][2]);
                BakedQuad bq = new BakedQuad(vp[0], vp[1], vp[2], vp[3], 0L, 0L, 0L, 0L, d, null);
                for (BlockState st : states) {
                    boolean collisionFull = st.isCollisionShapeFullBlock(level, pos);
                    prep.invoke(lighter, level, st, pos, bq, true);
                    float[] faceShape = (float[]) faceShapeF.get(lighter);
                    boolean facePartial = facePartialF.getBoolean(lighter);
                    boolean faceCubic = faceCubicF.getBoolean(lighter);
                    StringBuilder sb = new StringBuilder("SHP\t").append(d.ordinal()).append('\t').append(collisionFull ? 1 : 0);
                    for (int i = 0; i < 4; i++) sb.append('\t').append(f(quad[i][0])).append('\t').append(f(quad[i][1])).append('\t').append(f(quad[i][2]));
                    for (int i = 0; i < 12; i++) sb.append('\t').append(f(faceShape[i]));
                    sb.append('\t').append(facePartial ? 1 : 0).append('\t').append(faceCubic ? 1 : 0);
                    O.println(sb);
                }
            }
        }
    }

    // A handful of quads on each face's plane: full [0,1], inset [.25,.75], and a non-plane (slanted)
    // quad to exercise min!=max (faceCubic false) + facePartial.
    static float[][][] quadsFor(Direction d) {
        return switch (d) {
            case DOWN -> new float[][][]{
                {{0,0,1},{0,0,0},{1,0,0},{1,0,1}},          // full at y=0
                {{.25f,0,.75f},{.25f,0,.25f},{.75f,0,.25f},{.75f,0,.75f}},  // inset at y=0
                {{0,.5f,1},{0,.5f,0},{1,.5f,0},{1,.5f,1}}}; // full at y=0.5 (interior)
            case UP -> new float[][][]{
                {{0,1,0},{0,1,1},{1,1,1},{1,1,0}},
                {{.25f,1,.25f},{.25f,1,.75f},{.75f,1,.75f},{.75f,1,.25f}},
                {{0,.5f,0},{0,.5f,1},{1,.5f,1},{1,.5f,0}}};
            case NORTH -> new float[][][]{
                {{1,1,0},{1,0,0},{0,0,0},{0,1,0}},
                {{.75f,.75f,0},{.75f,.25f,0},{.25f,.25f,0},{.25f,.75f,0}},
                {{1,1,.5f},{1,0,.5f},{0,0,.5f},{0,1,.5f}}};
            case SOUTH -> new float[][][]{
                {{0,1,1},{0,0,1},{1,0,1},{1,1,1}},
                {{.25f,.75f,1},{.25f,.25f,1},{.75f,.25f,1},{.75f,.75f,1}},
                {{0,1,.5f},{0,0,.5f},{1,0,.5f},{1,1,.5f}}};
            case WEST -> new float[][][]{
                {{0,1,0},{0,0,0},{0,0,1},{0,1,1}},
                {{0,.75f,.25f},{0,.25f,.25f},{0,.25f,.75f},{0,.75f,.75f}},
                {{.5f,1,0},{.5f,0,0},{.5f,0,1},{.5f,1,1}}};
            case EAST -> new float[][][]{
                {{1,1,1},{1,0,1},{1,0,0},{1,1,0}},
                {{1,.75f,.75f},{1,.25f,.75f},{1,.25f,.25f},{1,.75f,.25f}},
                {{.5f,1,1},{.5f,0,1},{.5f,0,0},{.5f,1,0}}};
        };
    }
}

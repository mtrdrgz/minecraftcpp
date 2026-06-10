// Ground truth for the two bounded helpers the element-level bake (UnbakedCuboidGeometry.bake)
// composes on top of the certified FaceBakery.bakeQuad:
//   DFUV = FaceBakery.defaultFaceUV(from, to, facing)   (FaceBakery.java:26-35, @VisibleForTesting)
//   DROT = Direction.rotate(Matrix4fc, Direction)         (Direction.java:121-125 + getApproximateNearest)
//
//   tools/run_groundtruth.ps1 -Tool ModelElementHelpersParity -Out mcpp/build/model_element_helpers.tsv
//
// Rows (tab-separated; floats %08x rawIntBits, ints decimal):
//   DFUV  facing  fx fy fz  tx ty tz   minU minV maxU maxV
//   DROT  facing  matKind  qIdx  axis  angleHex   expectedOrdinal
//     matKind 0 = identity matrix (qIdx=-1 axis=-1 angle=0)
//     matKind 1 = rotM(qIdx) = Matrix4f.rotation(Quaternionf(QS[qIdx]))   (arbitrary)
//     matKind 2 = SingleAxisRotation(axis, angle).transformation()         (axis-aligned)

import com.mojang.math.Quadrant;
import java.lang.reflect.Method;
import net.minecraft.client.resources.model.cuboid.CuboidFace;
import net.minecraft.client.resources.model.cuboid.CuboidRotation;
import net.minecraft.client.resources.model.cuboid.FaceBakery;
import net.minecraft.core.Direction;
import org.joml.Matrix4f;
import org.joml.Quaternionf;
import org.joml.Vector3f;
import org.joml.Vector3fc;

public class ModelElementHelpersParity {
    static final java.io.PrintStream O = System.out;
    static String f(float x) { return String.format("%08x", Float.floatToRawIntBits(x)); }

    static final float[][] QS = {
        {0, 0, 0, 1}, {0.5f, 0.5f, 0.5f, 0.5f}, {0.5f, 0, 0, 0.5f},
        {-0.5f, 0.5f, -0.5f, 0.5f}, {0.125f, 0.375f, -0.625f, 0.75f}
    };
    static Matrix4f rotM(int i) {
        return new Matrix4f().rotation(new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3]));
    }

    public static void main(String[] args) throws Exception {
        Method defaultFaceUV = FaceBakery.class.getDeclaredMethod(
            "defaultFaceUV", Vector3fc.class, Vector3fc.class, Direction.class);
        defaultFaceUV.setAccessible(true);

        Direction[] dirs = Direction.values();

        // ── DFUV: defaultFaceUV over a range of boxes for every facing ──
        float[][][] boxes = {
            {{0, 0, 0}, {16, 16, 16}},
            {{2, 3, 4}, {14, 12, 10}},
            {{5, 0, 5}, {11, 16, 11}},
            {{0, 6, 0}, {16, 10, 16}},
            {{1, 1, 1}, {15, 15, 15}},
            {{0, 0, 7}, {16, 16, 9}},   // thin slab in z
            {{7.5f, 2.25f, 3.125f}, {12.0625f, 13.75f, 9.9375f}},  // fractional
        };
        for (Direction d : dirs) {
            for (float[][] box : boxes) {
                Vector3f from = new Vector3f(box[0][0], box[0][1], box[0][2]);
                Vector3f to = new Vector3f(box[1][0], box[1][1], box[1][2]);
                CuboidFace.UVs uv = (CuboidFace.UVs) defaultFaceUV.invoke(null, from, to, d);
                O.println("DFUV\t" + d.ordinal()
                    + "\t" + f(from.x()) + "\t" + f(from.y()) + "\t" + f(from.z())
                    + "\t" + f(to.x()) + "\t" + f(to.y()) + "\t" + f(to.z())
                    + "\t" + f(uv.minU()) + "\t" + f(uv.minV()) + "\t" + f(uv.maxU()) + "\t" + f(uv.maxV()));
            }
        }

        // ── DROT: Direction.rotate under identity / arbitrary / axis-aligned matrices ──
        for (Direction facing : dirs) {
            // identity
            emitDrot(facing, 0, -1, -1, 0f, new Matrix4f());
            // arbitrary rotations rotM(0..4)
            for (int qi = 0; qi < QS.length; qi++) emitDrot(facing, 1, qi, -1, 0f, rotM(qi));
            // axis-aligned single-axis rotations (the realistic cull-rotate case)
            int[] axes = {0, 1, 2};
            float[] angles = {0f, 90f, 180f, 270f, -90f, 45f, 22.5f};
            for (int ax : axes) {
                for (float ang : angles) {
                    Matrix4f m = new CuboidRotation.SingleAxisRotation(Direction.Axis.values()[ax], ang).transformation();
                    emitDrot(facing, 2, -1, ax, ang, m);
                }
            }
        }
    }

    static void emitDrot(Direction facing, int matKind, int qIdx, int axis, float angle, Matrix4f m) {
        Direction res = Direction.rotate(m, facing);
        O.println("DROT\t" + facing.ordinal() + "\t" + matKind + "\t" + qIdx + "\t" + axis
            + "\t" + f(angle) + "\t" + res.ordinal());
    }
}

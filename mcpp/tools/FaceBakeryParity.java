// Ground-truth generator for the geometry helpers of
// net.minecraft.client.resources.model.cuboid.FaceBakery (private static methods,
// reached via reflection): rotateVertexBy, findClosestDirection, calculateFacing,
// cornerToCenter, centerToCorner. Deterministic float math on org.joml; no Bootstrap.
// The C++ port (render/model/FaceBakery.h) must match bit-for-bit (floats as raw
// IEEE-754 bits; direction ordinals as ints, -1 for null).
//
//   tools/run_groundtruth.ps1 -Tool FaceBakeryParity -Out mcpp/build/face_bakery.tsv

import com.mojang.math.Quadrant;
import java.lang.reflect.Method;
import net.minecraft.client.renderer.FaceInfo;
import net.minecraft.client.resources.model.cuboid.CuboidFace;
import net.minecraft.client.resources.model.cuboid.CuboidRotation;
import net.minecraft.client.resources.model.cuboid.FaceBakery;
import net.minecraft.core.Direction;
import org.joml.Matrix4f;
import org.joml.Quaternionf;
import org.joml.Vector3f;
import org.joml.Vector3fc;

public class FaceBakeryParity {
    static final java.io.PrintStream O = System.out;
    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String v3(Vector3f v) { return b(v.x) + "\t" + b(v.y) + "\t" + b(v.z); }

    static final float[][] QS = {
        {0,0,0,1}, {0.5f,0.5f,0.5f,0.5f}, {0.5f,0,0,0.5f}, {-0.5f,0.5f,-0.5f,0.5f}, {0.125f,0.375f,-0.625f,0.75f}
    };
    static final float[][] VERTS = {
        {0,0,0}, {1,1,1}, {0.5f,0.25f,0.75f}, {16,16,16}, {-2,3,-4}, {0.0625f,0.9375f,0.5f}
    };
    static final float[][] ORIGINS = { {0.5f,0.5f,0.5f}, {0,0,0}, {0.25f,0.75f,0.5f} };
    static final float[][] DIRS = {
        {0,1,0}, {0,-1,0}, {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1}, {0.9f,0.1f,0.2f},
        {0.3f,0.3f,0.31f}, {0,0,0}, {-0.7f,-0.71f,0.1f}, {0.5f,0.5f,0.5f}
    };

    public static void main(String[] args) throws Exception {
        Method rotateVertexBy = FaceBakery.class.getDeclaredMethod("rotateVertexBy", Vector3f.class, Vector3fc.class, org.joml.Matrix4fc.class);
        Method findClosest = FaceBakery.class.getDeclaredMethod("findClosestDirection", Vector3f.class);
        Method calcFacing = FaceBakery.class.getDeclaredMethod("calculateFacing", Vector3fc[].class);
        Method cornerToCenter = FaceBakery.class.getDeclaredMethod("cornerToCenter", float.class);
        Method centerToCorner = FaceBakery.class.getDeclaredMethod("centerToCorner", float.class);
        rotateVertexBy.setAccessible(true); findClosest.setAccessible(true); calcFacing.setAccessible(true);
        cornerToCenter.setAccessible(true); centerToCorner.setAccessible(true);

        for (float[] val : VERTS) {
            O.println("C2C\t" + b(val[0]) + "\t" + b((float) cornerToCenter.invoke(null, val[0])));
            O.println("CEN2C\t" + b(val[0]) + "\t" + b((float) centerToCorner.invoke(null, val[0])));
        }

        for (int qi = 0; qi < QS.length; qi++) {
            Matrix4f m = new Matrix4f().rotation(new Quaternionf(QS[qi][0], QS[qi][1], QS[qi][2], QS[qi][3]));
            for (float[] vt : VERTS) for (float[] og : ORIGINS) {
                Vector3f vertex = new Vector3f(vt[0], vt[1], vt[2]);
                rotateVertexBy.invoke(null, vertex, new Vector3f(og[0], og[1], og[2]), m);
                O.println("ROTV\t" + qi + "\t" + b(vt[0])+"\t"+b(vt[1])+"\t"+b(vt[2])+"\t"+b(og[0])+"\t"+b(og[1])+"\t"+b(og[2])+"\t"+v3(vertex));
            }
        }

        for (float[] d : DIRS) {
            Direction r = (Direction) findClosest.invoke(null, new Vector3f(d[0], d[1], d[2]));
            O.println("FCD\t" + b(d[0])+"\t"+b(d[1])+"\t"+b(d[2])+"\t" + (r == null ? -1 : r.ordinal()));
        }

        // calculateFacing over triangles of corner positions
        for (int i = 0; i < VERTS.length; i++) {
            float[] p0 = VERTS[i % VERTS.length], p1 = VERTS[(i+1) % VERTS.length], p2 = VERTS[(i+2) % VERTS.length];
            Vector3fc[] pos = new Vector3fc[]{ new Vector3f(p0[0],p0[1],p0[2]), new Vector3f(p1[0],p1[1],p1[2]), new Vector3f(p2[0],p2[1],p2[2]) };
            Direction r = (Direction) calcFacing.invoke(null, (Object) pos);
            O.println("FACE\t" + b(p0[0])+"\t"+b(p0[1])+"\t"+b(p0[2])+"\t"+b(p1[0])+"\t"+b(p1[1])+"\t"+b(p1[2])+"\t"+b(p2[0])+"\t"+b(p2[1])+"\t"+b(p2[2])+"\t" + (r == null ? -1 : r.ordinal()));
        }

        // ── bakeVertex POSITION path (FaceInfo.select.div(16) + element + model rotation) ──
        Direction[] DIRS = Direction.values();
        Direction.Axis[] AXES = { Direction.Axis.X, Direction.Axis.Y, Direction.Axis.Z };
        Vector3f BLOCK_MIDDLE = new Vector3f(0.5f, 0.5f, 0.5f);
        float[][] BOXES = { {0,0,0, 16,16,16}, {2,3,4, 14,12,10} };
        // element variants: {flag, axis, angle, rescale}; -1 axis means none
        float[][] ELEMS = { {0,0,0,0}, {1,0,22.5f,0}, {1,1,45f,1}, {1,2,-45f,1} };
        Vector3f elemOrigin = new Vector3f(0.5f, 0.5f, 0.5f);
        int[] MODELS = { -1, 1, 3 }; // index into QS, or -1 for none
        for (int f = 0; f < 6; f++) for (int idx = 0; idx < 4; idx++) for (float[] box : BOXES) {
            Vector3f from = new Vector3f(box[0], box[1], box[2]);
            Vector3f to = new Vector3f(box[3], box[4], box[5]);
            for (float[] el : ELEMS) for (int mdl : MODELS) {
                Vector3f vertex = FaceInfo.fromFacing(DIRS[f]).getVertexInfo(idx).select(from, to).div(16.0F);
                int elemFlag = (int) el[0];
                if (elemFlag == 1) {
                    CuboidRotation cr = new CuboidRotation(elemOrigin, new CuboidRotation.SingleAxisRotation(AXES[(int) el[1]], el[2]), el[3] != 0);
                    rotateVertexBy.invoke(null, vertex, cr.origin(), cr.transform());
                }
                if (mdl >= 0) {
                    Matrix4f mm = new Matrix4f().rotation(new Quaternionf(QS[mdl][0], QS[mdl][1], QS[mdl][2], QS[mdl][3]));
                    rotateVertexBy.invoke(null, vertex, BLOCK_MIDDLE, mm);
                }
                O.println("BVP\t" + f + "\t" + idx
                    + "\t" + b(box[0])+"\t"+b(box[1])+"\t"+b(box[2])+"\t"+b(box[3])+"\t"+b(box[4])+"\t"+b(box[5])
                    + "\t" + elemFlag + "\t" + (int) el[1] + "\t" + b(el[2]) + "\t" + (int) el[3]
                    + "\t" + b(elemOrigin.x)+"\t"+b(elemOrigin.y)+"\t"+b(elemOrigin.z)
                    + "\t" + mdl + "\t" + v3(vertex));
            }
        }

        // ── bakeVertex UV path: CuboidFace.getU/getV + uvRotation + optional uvTransform ──
        Quadrant[] QUADS = Quadrant.values(); // R0,R90,R180,R270 -> shift 0,1,2,3
        float[][] UVSETS = { {0,0,16,16}, {2,4,14,12}, {16,0,0,16}, {5,5,11,11} };
        for (float[] uv : UVSETS) {
            CuboidFace.UVs uvs = new CuboidFace.UVs(uv[0], uv[1], uv[2], uv[3]);
            for (int q = 0; q < QUADS.length; q++) {
                for (int idx = 0; idx < 4; idx++) {
                    for (int mdl : new int[]{ -1, 2 }) { // -1 = identity uvTransform, else rotation(QS[mdl])
                        float rawU = CuboidFace.getU(uvs, QUADS[q], idx);
                        float rawV = CuboidFace.getV(uvs, QUADS[q], idx);
                        float tu, tv;
                        if (mdl < 0) { tu = rawU; tv = rawV; }
                        else {
                            Matrix4f uvm = new Matrix4f().rotation(new Quaternionf(QS[mdl][0], QS[mdl][1], QS[mdl][2], QS[mdl][3]));
                            float cu = (float) cornerToCenter.invoke(null, rawU);
                            float cv = (float) cornerToCenter.invoke(null, rawV);
                            Vector3f t = new Vector3f(cu, cv, 0.0F);
                            uvm.transformPosition(t);
                            tu = (float) centerToCorner.invoke(null, t.x);
                            tv = (float) centerToCorner.invoke(null, t.y);
                        }
                        O.println("BVUV\t" + b(uv[0])+"\t"+b(uv[1])+"\t"+b(uv[2])+"\t"+b(uv[3])
                            + "\t" + q + "\t" + idx + "\t" + mdl + "\t" + b(tu) + "\t" + b(tv));
                    }
                }
            }
        }

        // ── recalculateWinding: feed canonical face corners in a shuffled order ──
        Method recalcWinding = FaceBakery.class.getDeclaredMethod("recalculateWinding", Vector3fc[].class, long[].class, Direction.class);
        recalcWinding.setAccessible(true);
        int[][] SHUFFLES = { {0,1,2,3}, {1,2,3,0}, {3,2,1,0}, {2,0,3,1}, {0,3,2,1} };
        for (int f = 0; f < 6; f++) for (float[] box : BOXES) {
            Vector3f from = new Vector3f(box[0], box[1], box[2]);
            Vector3f to = new Vector3f(box[3], box[4], box[5]);
            // canonical corners for this face
            Vector3f[] canon = new Vector3f[4];
            for (int i = 0; i < 4; i++) canon[i] = FaceInfo.fromFacing(DIRS[f]).getVertexInfo(i).select(from, to).div(16.0F);
            for (int[] sh : SHUFFLES) {
                Vector3fc[] pos = new Vector3fc[4];
                long[] uvs = new long[4];
                for (int i = 0; i < 4; i++) { pos[i] = new Vector3f(canon[sh[i]]); uvs[i] = i; }
                StringBuilder sb = new StringBuilder("RWND\t" + f);
                for (int i = 0; i < 4; i++) sb.append('\t').append(b(pos[i].x())).append('\t').append(b(pos[i].y())).append('\t').append(b(pos[i].z())); // input
                recalcWinding.invoke(null, pos, uvs, DIRS[f]);
                for (int i = 0; i < 4; i++) sb.append('\t').append(b(pos[i].x())).append('\t').append(b(pos[i].y())).append('\t').append(b(pos[i].z())); // output
                for (int i = 0; i < 4; i++) sb.append('\t').append(uvs[i]); // output perm
                O.println(sb.toString());
            }
        }
    }
}

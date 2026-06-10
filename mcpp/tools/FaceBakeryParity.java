// Ground-truth generator for the geometry helpers of
// net.minecraft.client.resources.model.cuboid.FaceBakery (private static methods,
// reached via reflection): rotateVertexBy, findClosestDirection, calculateFacing,
// cornerToCenter, centerToCorner. Deterministic float math on org.joml; no Bootstrap.
// The C++ port (render/model/FaceBakery.h) must match bit-for-bit (floats as raw
// IEEE-754 bits; direction ordinals as ints, -1 for null).
//
//   tools/run_groundtruth.ps1 -Tool FaceBakeryParity -Out mcpp/build/face_bakery.tsv

import java.lang.reflect.Method;
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
    }
}

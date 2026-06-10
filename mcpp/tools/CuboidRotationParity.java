// Ground-truth generator for net.minecraft.client.resources.model.cuboid.
// CuboidRotation — the block-model element rotation transform (single-axis +
// EulerXYZ, with optional rescale). Pure float math on org.joml; no Bootstrap.
// The C++ port (render/model/CuboidRotation.h) must match the resulting transform
// matrix bit-for-bit (floats as raw IEEE-754 bits).
//
//   tools/run_groundtruth.ps1 -Tool CuboidRotationParity -Out mcpp/build/cuboid_rotation.tsv

import net.minecraft.client.resources.model.cuboid.CuboidRotation;
import net.minecraft.core.Direction;
import org.joml.Matrix4fc;
import org.joml.Vector3f;

public class CuboidRotationParity {
    static final java.io.PrintStream O = System.out;
    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String m4(Matrix4fc m) {
        return b(m.m00())+"\t"+b(m.m01())+"\t"+b(m.m02())+"\t"+b(m.m03())+"\t"
             + b(m.m10())+"\t"+b(m.m11())+"\t"+b(m.m12())+"\t"+b(m.m13())+"\t"
             + b(m.m20())+"\t"+b(m.m21())+"\t"+b(m.m22())+"\t"+b(m.m23())+"\t"
             + b(m.m30())+"\t"+b(m.m31())+"\t"+b(m.m32())+"\t"+b(m.m33());
    }

    static final Direction.Axis[] AXES = { Direction.Axis.X, Direction.Axis.Y, Direction.Axis.Z };
    static final float[] ANGLES = { -45f, -22.5f, 0f, 22.5f, 45f };

    public static void main(String[] args) {
        Vector3f origin = new Vector3f(0.5f, 0.5f, 0.5f);
        for (int a = 0; a < AXES.length; a++) {
            for (float ang : ANGLES) {
                for (boolean rescale : new boolean[]{ false, true }) {
                    CuboidRotation cr = new CuboidRotation(origin, new CuboidRotation.SingleAxisRotation(AXES[a], ang), rescale);
                    O.println("SAR\t" + a + "\t" + b(ang) + "\t" + (rescale ? 1 : 0) + "\t" + m4(cr.transform()));
                }
            }
        }
        float[][] EULERS = { {0,0,0}, {22.5f,0,0}, {0,45f,0}, {0,0,-22.5f}, {45f,45f,45f}, {-22.5f,22.5f,-45f}, {90f,0,0}, {0,180f,0} };
        for (float[] e : EULERS) {
            for (boolean rescale : new boolean[]{ false, true }) {
                CuboidRotation cr = new CuboidRotation(origin, new CuboidRotation.EulerXYZRotation(e[0], e[1], e[2]), rescale);
                O.println("EUL\t" + b(e[0]) + "\t" + b(e[1]) + "\t" + b(e[2]) + "\t" + (rescale ? 1 : 0) + "\t" + m4(cr.transform()));
            }
        }
    }
}

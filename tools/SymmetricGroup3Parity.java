// Ground-truth generator for com.mojang.math.SymmetricGroup3 (Minecraft 26.1.2),
// driving the REAL enum from client.jar against the C++ port in
// render/model/SymmetricGroup3.h.
//
// SymmetricGroup3 is pure enum / permutation / org.joml float math — NO GL/GPU/window —
// so it loads and runs headless. The octahedral_parity gate already covers permute(int)
// and compose(ordinal); the surfaces certified HERE (ungated elsewhere) are:
//   * transformation()        — the 9-float Matrix3fc per group (raw IEEE-754 bits)
//   * inverse().ordinal()     — the DIRECT INVERSE_TABLE entry
//   * permuteVector(Vector3f) — float vector permutation (mutates in place)
//   * permuteVector(Vector3i) — int   vector permutation
//   * permuteAxis(Axis)       — Axis.VALUES[permute(axis.ordinal())]
//
// transformation() is built with Matrix3f.zero().set(col,row,1f) (plain stores, no
// arithmetic), so every element is bit-exact. permuteVector reads PRIVATE p0/p1/p2;
// we read them by reflection so the input vectors can be permuted exactly as Java does.
//
//   tools/run_groundtruth.ps1 -Tool SymmetricGroup3Parity -Out mcpp/build/symmetric_group3.tsv

import com.mojang.math.SymmetricGroup3;
import net.minecraft.core.Direction;
import org.joml.Matrix3fc;
import org.joml.Vector3f;
import org.joml.Vector3i;
import java.lang.reflect.Field;

public class SymmetricGroup3Parity {
    static final java.io.PrintStream O = System.out;
    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // All 9 column-major elements of a Matrix3fc: m00,m01,m02, m10,m11,m12, m20,m21,m22.
    static String mat(Matrix3fc m) {
        return b(m.m00())+"\t"+b(m.m01())+"\t"+b(m.m02())+"\t"
             + b(m.m10())+"\t"+b(m.m11())+"\t"+b(m.m12())+"\t"
             + b(m.m20())+"\t"+b(m.m21())+"\t"+b(m.m22());
    }

    // Representative float vectors: distinct components so any permutation is observable;
    // mixed signs, fractions, and a non-finite to exercise the raw-bit path.
    static final float[][] VF = {
        { 1.0f, 2.0f, 3.0f },
        { -1.5f, 4.25f, -7.0f },
        { 0.0f, -0.0f, 100.5f },
        { 1.0e-7f, -3.5f, 2.0f },
        { Float.NaN, 5.0f, -6.0f },
        { 123456.78f, 0.125f, -0.001f },
    };
    // Representative int vectors.
    static final int[][] VI = {
        { 1, 2, 3 },
        { -10, 20, -30 },
        { 0, Integer.MIN_VALUE, Integer.MAX_VALUE },
        { 7, -7, 0 },
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Field fp0 = SymmetricGroup3.class.getDeclaredField("p0"); fp0.setAccessible(true);
        Field fp1 = SymmetricGroup3.class.getDeclaredField("p1"); fp1.setAccessible(true);
        Field fp2 = SymmetricGroup3.class.getDeclaredField("p2"); fp2.setAccessible(true);

        SymmetricGroup3[] sym = SymmetricGroup3.values();
        Direction.Axis[] axes = Direction.Axis.values();

        for (SymmetricGroup3 g : sym) {
            int ord = g.ordinal();

            // transformation() — the 9-float matrix.
            O.println("TRANSFORM\t" + ord + "\t" + mat(g.transformation()));

            // inverse().ordinal() — direct table entry.
            O.println("INVERSE\t" + ord + "\t" + g.inverse().ordinal());

            // permute(i) and permuteAxis(axis) (re-covered for completeness).
            for (int i = 0; i < 3; i++) O.println("PERMUTE\t" + ord + "\t" + i + "\t" + g.permute(i));
            for (Direction.Axis ax : axes)
                O.println("PERMUTEAXIS\t" + ord + "\t" + ax.ordinal() + "\t" + g.permuteAxis(ax).ordinal());

            // permuteVector(Vector3f): mutates in place — print the resulting x,y,z.
            for (float[] v : VF) {
                Vector3f vec = new Vector3f(v[0], v[1], v[2]);
                g.permuteVector(vec); // returns the same (mutated) vec
                O.println("PVECF\t" + ord + "\t" + b(v[0]) + "\t" + b(v[1]) + "\t" + b(v[2])
                    + "\t" + b(vec.x) + "\t" + b(vec.y) + "\t" + b(vec.z));
            }

            // permuteVector(Vector3i).
            for (int[] v : VI) {
                Vector3i vec = new Vector3i(v[0], v[1], v[2]);
                g.permuteVector(vec);
                O.println("PVECI\t" + ord + "\t" + v[0] + "\t" + v[1] + "\t" + v[2]
                    + "\t" + vec.x + "\t" + vec.y + "\t" + vec.z);
            }

            // compose(ordinal) — re-covered so this gate stands alone.
            for (SymmetricGroup3 s : sym)
                O.println("COMPOSE\t" + ord + "\t" + s.ordinal() + "\t" + g.compose(s).ordinal());

            // Sanity: the reflected private permutation triple == permute(0..2).
            int p0 = fp0.getInt(g), p1 = fp1.getInt(g), p2 = fp2.getInt(g);
            O.println("PRIV\t" + ord + "\t" + p0 + "\t" + p1 + "\t" + p2);
        }
    }
}

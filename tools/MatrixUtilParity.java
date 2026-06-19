// Ground-truth generator for the PURE static helpers of com.mojang.math.MatrixUtil
// (Minecraft 26.1.2) and the com.mojang.math.GivensParameters record they use.
// Verifies the C++ port in mcpp/src/render/model/MatrixUtil.h (which reuses the
// certified render/model/Joml.h Matrix4f / Quaternionf).
//
// Covered:
//   mulComponentWise(Matrix4f, float)        -> 16 scaled floats + JOML properties()
//   isPureTranslation / isIdentity / checkPropertyRaw(matrix, bit)
//   GivensParameters.fromUnnormalized / fromPositiveAngle / inverse / cos / sin /
//     aroundX|Y|Z(Quaternionf)
//   approxGivensQuat(a11,a12,a22)  (private static, reflection)
//   qrGivensQuat(a1,a2)            (private static, reflection)
//
// SKIPPED (iterative Jacobi SVD, out of scope, not bit-exact-safe to certify here):
//   svdDecompose / eigenvalueJacobi / stepJacobi / similarityTransform.
//
// To make the gate independent of *how* a matrix was built, every matrix row carries
// the INPUT matrix as 16 raw float bits + its stored JOML properties() int; the C++
// side reconstructs that exact matrix (fields + properties) and recomputes. The input
// matrices are built from rotation(EXACT-float quaternion)/scale/translate/raw set so
// no live sin/cos enters mulComponentWise itself. fromPositiveAngle/qrGivensQuat do
// exercise (float)Math.sin / Math.hypot — emitted and compared bit-for-bit (libm
// sqrt/hypot are correctly rounded, matching std:: on this toolchain; see gate note).
//
//   tools/run_groundtruth.ps1 -Tool MatrixUtilParity -Out mcpp/build/matrix_util.tsv

import java.lang.reflect.Method;
import org.joml.Matrix4f;
import org.joml.Quaternionf;
import com.mojang.math.GivensParameters;
import com.mojang.math.MatrixUtil;

public class MatrixUtilParity {
    static final java.io.PrintStream O = System.out;

    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // 16 raw float bits in column-major field order m00,m01,m02,m03,m10,... + properties()
    static String m4p(Matrix4f m) {
        return b(m.m00()) + "\t" + b(m.m01()) + "\t" + b(m.m02()) + "\t" + b(m.m03()) + "\t"
             + b(m.m10()) + "\t" + b(m.m11()) + "\t" + b(m.m12()) + "\t" + b(m.m13()) + "\t"
             + b(m.m20()) + "\t" + b(m.m21()) + "\t" + b(m.m22()) + "\t" + b(m.m23()) + "\t"
             + b(m.m30()) + "\t" + b(m.m31()) + "\t" + b(m.m32()) + "\t" + b(m.m33()) + "\t"
             + m.properties();
    }

    static Matrix4f raw(float[] p) {
        return new Matrix4f().set(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
                                  p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
    }
    static Matrix4f copy(Matrix4f m) { return new Matrix4f(m); }

    static final float[][] QS = {
        {0,0,0,1}, {0.5f,0.5f,0.5f,0.5f}, {0.5f,0,0,0.5f}, {0,0.25f,0,0.75f},
        {-0.5f,0.5f,-0.5f,0.5f}, {1,0,0,0}, {0.125f,0.375f,-0.625f,0.75f}
    };
    static final float[][] TS = { {0,0,0}, {1,2,3}, {-5,0.5f,7}, {0.25f,-0.75f,1.5f} };
    static final float[][] SS = { {1,1,1}, {2,3,4}, {-1,0.5f,2}, {0.25f,0.25f,0.25f}, {-1,-1,-1} };
    static final float[] FACTORS = { 0.0f, 1.0f, -1.0f, 2.0f, 0.5f, -3.25f, 100.0f, 1.0E-7f, -0.0f };

    static final float[][] RAWS = {
        {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1},          // identity
        {1,0,0,0, 0,1,0,0, 0,0,1,0, 5,6,7,1},          // pure translation
        {2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,1},          // uniform scale
        {0.5f,0.1f,-0.2f,0, 0.3f,0.6f,0.05f,0, -0.1f,0.2f,0.9f,0, 1,2,3,1}, // general affine
        {1,0,0,0, 0,1,0,0, 0,0,1,-1, 0,0,0,0},         // perspective-shaped
        {-1,0,0,0, 0,-1,0,0, 0,0,-1,0, 0,0,0,1},       // -identity rotation part
        {3,0,0,0, 0,3,0,0, 0,0,3,0, 0,0,0,1},          // factor 1/3 -> identity after mulCW
    };

    // MUL row: <tag> <factorBits> <input m4p (17)> <output m4p (17)>
    static void emitMul(String tag, Matrix4f m, float f) {
        String in = m4p(m);
        Matrix4f r = MatrixUtil.mulComponentWise(m, f); // mutates m in place, returns it
        O.println(tag + "\t" + b(f) + "\t" + in + "\t" + m4p(r));
    }

    // PROPS row: <tag> <input m4p (17)> rawBit(1,2,4,8,16) isPureTranslation isIdentity
    static void emitProps(String tag, Matrix4f m) {
        String in = m4p(m);
        int[] bits = {1, 2, 4, 8, 16};
        StringBuilder sb = new StringBuilder(tag).append("\t").append(in);
        for (int bit : bits) sb.append("\t").append(MatrixUtil.checkPropertyRaw(copy(m), bit) ? 1 : 0);
        sb.append("\t").append(MatrixUtil.isPureTranslation(copy(m)) ? 1 : 0);
        sb.append("\t").append(MatrixUtil.isIdentity(copy(m)) ? 1 : 0);
        O.println(sb.toString());
    }

    static String giv(GivensParameters g) {
        return b(g.sinHalf()) + "\t" + b(g.cosHalf()) + "\t" + b(g.cos()) + "\t" + b(g.sin());
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Method approx = MatrixUtil.class.getDeclaredMethod(
            "approxGivensQuat", float.class, float.class, float.class);
        approx.setAccessible(true);
        Method qr = MatrixUtil.class.getDeclaredMethod(
            "qrGivensQuat", float.class, float.class);
        qr.setAccessible(true);

        // ── mulComponentWise ──────────────────────────────────────────────────────
        for (int i = 0; i < QS.length; i++)
            for (float f : FACTORS)
                emitMul("MULCW_ROT_" + i,
                    new Matrix4f().rotation(new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3])), f);
        for (int s = 0; s < SS.length; s++)
            for (float f : FACTORS)
                emitMul("MULCW_SCALE_" + s, new Matrix4f().scale(SS[s][0], SS[s][1], SS[s][2]), f);
        for (int t = 0; t < TS.length; t++)
            for (float f : FACTORS)
                emitMul("MULCW_TR_" + t, new Matrix4f().translation(TS[t][0], TS[t][1], TS[t][2]), f);
        for (int r = 0; r < RAWS.length; r++)
            for (float f : FACTORS)
                emitMul("MULCW_RAW_" + r, raw(RAWS[r]), f);
        // combination: translate ∘ rotate ∘ scale (general matrix from the JOML side;
        // the C++ side reconstructs it from the emitted input bits, so its own rotate()
        // restriction is irrelevant here).
        for (int i = 0; i < QS.length; i++)
            emitMul("MULCW_COMBO_" + i,
                new Matrix4f().translation(1, 2, 3)
                    .rotate(new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3]))
                    .scale(SS[i % SS.length][0], SS[i % SS.length][1], SS[i % SS.length][2]),
                2.0f);

        // ── property predicates ───────────────────────────────────────────────────
        for (int r = 0; r < RAWS.length; r++) emitProps("PROPS_RAW_" + r, raw(RAWS[r]));
        for (int t = 0; t < TS.length; t++)   emitProps("PROPS_TR_" + t, new Matrix4f().translation(TS[t][0], TS[t][1], TS[t][2]));
        for (int s = 0; s < SS.length; s++)   emitProps("PROPS_SCALE_" + s, new Matrix4f().scale(SS[s][0], SS[s][1], SS[s][2]));
        for (int i = 0; i < QS.length; i++)   emitProps("PROPS_ROT_" + i, new Matrix4f().rotation(new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3])));
        emitProps("PROPS_IDENT", new Matrix4f());

        // ── GivensParameters.fromUnnormalized / inverse / cos / sin / aroundX|Y|Z ──
        float[] GV = { 0f, 1f, -1f, 0.5f, -0.5f, 2f, -3f, 0.1f, -0.1f, 1.0E-7f, 3.5f, -2.25f };
        for (float sh : GV) for (float ch : GV) {
            // (0,0) makes fromUnnormalized normalize by 1/sqrt(0) -> NaN; the only diverging
            // case is the NaN sign bit, unreachable from any real (finite, nonzero) matrix column.
            if (sh == 0f && ch == 0f) continue;
            GivensParameters g = GivensParameters.fromUnnormalized(sh, ch);
            O.println("GIV_FROMUNNORM\t" + b(sh) + "\t" + b(ch) + "\t" + giv(g));
            O.println("GIV_INV\t" + b(sh) + "\t" + b(ch) + "\t" + giv(g.inverse()));
            Quaternionf qx = new Quaternionf(9,9,9,9); g.aroundX(qx);
            Quaternionf qy = new Quaternionf(9,9,9,9); g.aroundY(qy);
            Quaternionf qz = new Quaternionf(9,9,9,9); g.aroundZ(qz);
            O.println("GIV_AROUND\t" + b(sh) + "\t" + b(ch)
                + "\t" + b(qx.x)+"\t"+b(qx.y)+"\t"+b(qx.z)+"\t"+b(qx.w)
                + "\t" + b(qy.x)+"\t"+b(qy.y)+"\t"+b(qy.z)+"\t"+b(qy.w)
                + "\t" + b(qz.x)+"\t"+b(qz.y)+"\t"+b(qz.z)+"\t"+b(qz.w));
        }
        float[] ANG = { 0f, 0.5f, 1.0f, 1.5707964f, 3.1415927f, 0.7853982f, -0.7853982f,
                        2.5f, -2.0f, 0.123456f, (float)(Math.PI / 4) };
        for (float a : ANG)
            O.println("GIV_FROMANGLE\t" + b(a) + "\t" + giv(GivensParameters.fromPositiveAngle(a)));

        // ── approxGivensQuat / qrGivensQuat (private static) ──────────────────────
        float[] AV = { 0f, 1f, -1f, 0.5f, -0.5f, 2f, -2f, 100f, -100f, 1.0E-3f, 1.0E-7f,
                       -1.0E-7f, 3.0f, -3.0f, 0.25f };
        float[] A12 = { 0f, 1f, -1f, 0.5f, -0.5f, 2f, 1.0E-7f };
        for (float a11 : AV) for (float a12 : A12) for (float a22 : AV) {
            GivensParameters g = (GivensParameters) approx.invoke(null, a11, a12, a22);
            O.println("APPROX\t" + b(a11) + "\t" + b(a12) + "\t" + b(a22) + "\t" + giv(g));
        }
        for (float a1 : AV) for (float a2 : AV) {
            GivensParameters g = (GivensParameters) qr.invoke(null, a1, a2);
            O.println("QR\t" + b(a1) + "\t" + b(a2) + "\t" + giv(g));
        }
    }
}

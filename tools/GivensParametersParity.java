// Ground-truth generator for com.mojang.math.GivensParameters (Minecraft 26.1.2),
// driving the REAL record from client.jar against the C++ port in
// render/model/GivensParameters.h.
//
// GivensParameters is pure org.joml float math — NO GL/GPU/window — so it loads
// and runs headless. The MatrixUtil gate already covers the scalar/Quaternionf
// helpers (fromUnnormalized/fromPositiveAngle/inverse/cos/sin/aroundX|Y|Z(Quaternionf));
// the THREE Matrix3f overloads aroundX/Y/Z(Matrix3f) are NOT ported or gated anywhere
// else — this tool certifies them (plus re-covers the rest as a self-contained class).
//
// Runs under JOML's DEFAULT options (joml.useMathFma=false, joml.fastmath=false), so
// org.joml.Math.invsqrt(float) == 1.0f/(float)Math.sqrt((double)x), Math.sin(float) ==
// (float)Math.sin((double)x), Math.cosFromSin == cosFromSinInternal. All floats are
// exchanged as raw IEEE-754 bits (%08x of Float.floatToRawIntBits).
//
//   tools/run_groundtruth.ps1 -Tool GivensParametersParity -Out mcpp/build/givens_parameters.tsv
//
// The Matrix3f.aroundX/Y/Z(Matrix3f) overloads only WRITE a subset of the 9 elements;
// the rest pass through unchanged. We seed every input Matrix3f from a known non-zero,
// non-identity pattern (SEED) so the untouched elements are well-defined, then emit all
// 9 — this verifies the pass-through too, not just the written block.

import org.joml.Matrix3f;
import org.joml.Quaternionf;
import com.mojang.math.GivensParameters;

public class GivensParametersParity {
    static final java.io.PrintStream O = System.out;
    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // sinHalf/cosHalf accessors + derived cos()/sin().
    static String giv(GivensParameters g) {
        return b(g.sinHalf()) + "\t" + b(g.cosHalf()) + "\t" + b(g.cos()) + "\t" + b(g.sin());
    }

    // All 9 column-major elements m00,m01,m02, m10,m11,m12, m20,m21,m22.
    static String mat(Matrix3f m) {
        return b(m.m00)+"\t"+b(m.m01)+"\t"+b(m.m02)+"\t"
             + b(m.m10)+"\t"+b(m.m11)+"\t"+b(m.m12)+"\t"
             + b(m.m20)+"\t"+b(m.m21)+"\t"+b(m.m22);
    }

    // A deterministic non-identity, all-distinct-nonzero seed so the untouched
    // matrix elements (pass-through) are verifiable and the C++ side starts from
    // the same bits. (column-major Matrix3f(m00,m01,m02, m10,m11,m12, m20,m21,m22))
    static Matrix3f seed() {
        return new Matrix3f(
            1.5f,  2.5f,  3.5f,
            4.5f,  5.5f,  6.5f,
            7.5f,  8.5f,  9.5f);
    }

    // (sinHalf, cosHalf) pairs: zeros, ±1, fractions, large, tiny, mixed signs.
    static final float[] GV = {
        0f, 1f, -1f, 0.5f, -0.5f, 2f, -3f, 0.1f, -0.1f, 1.0E-7f, 3.5f, -2.25f, 0.0625f, 100f
    };
    // Positive angles for fromPositiveAngle (radians). FINITE only.
    static final float[] ANG = {
        0f, 0.5f, 1.0f, 1.5707964f, 3.1415927f, 0.7853982f, 2.5f, 0.123456f,
        (float)(Math.PI / 4), 6.2831855f, 0.001f
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // fromUnnormalized -> giv, inverse, aroundX/Y/Z(Quaternionf), aroundX/Y/Z(Matrix3f).
        for (float sh : GV) for (float ch : GV) {
            // (0,0): fromUnnormalized normalizes by 1/sqrt(0) -> NaN; only divergence is a
            // NaN sign bit, unreachable from any real (finite, nonzero) matrix column. Skip.
            if (sh == 0f && ch == 0f) continue;
            GivensParameters g = GivensParameters.fromUnnormalized(sh, ch);
            O.println("FROMUNNORM\t" + b(sh) + "\t" + b(ch) + "\t" + giv(g));
            O.println("INV\t" + b(sh) + "\t" + b(ch) + "\t" + giv(g.inverse()));

            // Quaternionf overloads (input pre-filled with a sentinel; set() overwrites all 4).
            Quaternionf qx = new Quaternionf(7,7,7,7); g.aroundX(qx);
            Quaternionf qy = new Quaternionf(7,7,7,7); g.aroundY(qy);
            Quaternionf qz = new Quaternionf(7,7,7,7); g.aroundZ(qz);
            O.println("AROUND_Q\t" + b(sh) + "\t" + b(ch)
                + "\t" + b(qx.x)+"\t"+b(qx.y)+"\t"+b(qx.z)+"\t"+b(qx.w)
                + "\t" + b(qy.x)+"\t"+b(qy.y)+"\t"+b(qy.z)+"\t"+b(qy.w)
                + "\t" + b(qz.x)+"\t"+b(qz.y)+"\t"+b(qz.z)+"\t"+b(qz.w));

            // Matrix3f overloads — the previously-ungated paths. Seeded inputs.
            Matrix3f mx = g.aroundX(seed());
            Matrix3f my = g.aroundY(seed());
            Matrix3f mz = g.aroundZ(seed());
            O.println("AROUND_MX\t" + b(sh) + "\t" + b(ch) + "\t" + mat(mx));
            O.println("AROUND_MY\t" + b(sh) + "\t" + b(ch) + "\t" + mat(my));
            O.println("AROUND_MZ\t" + b(sh) + "\t" + b(ch) + "\t" + mat(mz));
        }

        // fromPositiveAngle -> giv, and its Matrix3f application (exercises the
        // sin/cosFromSin path feeding cos()/sin() into the matrix block).
        for (float a : ANG) {
            GivensParameters g = GivensParameters.fromPositiveAngle(a);
            O.println("FROMANGLE\t" + b(a) + "\t" + giv(g));
            Matrix3f mx = g.aroundX(seed());
            Matrix3f my = g.aroundY(seed());
            Matrix3f mz = g.aroundZ(seed());
            O.println("ANGLE_MX\t" + b(a) + "\t" + mat(mx));
            O.println("ANGLE_MY\t" + b(a) + "\t" + mat(my));
            O.println("ANGLE_MZ\t" + b(a) + "\t" + mat(mz));
        }
    }
}

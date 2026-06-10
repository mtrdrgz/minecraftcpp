// Ground-truth generator for the pure org.joml.Quaternionf operations mirrored by
// render/model/QuaternionMath.h: mul(Quaternionfc) / conjugate / normalize / dot /
// lengthSquared / invert / rotationXYZ.
//
// Calls the REAL org.joml.Quaternionf from the shipped joml-1.10.8.jar (on the
// run_groundtruth classpath via 26.1.2/libs/*). Runs under JOML's DEFAULT options
// (joml.useMathFma=false, joml.fastmath=false), so Math.fma == plain a*b+c and
// Math.sin == (float)java.lang.Math.sin((double)x).
//
// The deterministic battery (mul/conjugate/normalize/dot/lengthSquared/invert) is
// built from EXACT float quaternion components (no sin/cos enters) and is bit-exact.
// rotationXYZ uses libm sin and is emitted too; it matches the same way the
// certified Joml.h rotationX/Y/Z rows do.
//
// Floats exchanged as raw IEEE-754 bits (%08x of Float.floatToRawIntBits).
//
//   tools/run_groundtruth.ps1 -Tool QuaternionMathParity -Out mcpp/build/quaternionf_math.tsv

import org.joml.Quaternionf;

public class QuaternionMathParity {
    static final java.io.PrintStream O = System.out;
    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String q(Quaternionf p) { return b(p.x) + "\t" + b(p.y) + "\t" + b(p.z) + "\t" + b(p.w); }

    // Exact-float quaternion components — FINITE/PHYSICAL, real edges, negatives,
    // zeros, non-unit magnitudes (invert/normalize must handle non-unit). No NaN/Inf.
    static final float[][] QS = {
        {0f, 0f, 0f, 1f},                 // identity
        {0.5f, 0.5f, 0.5f, 0.5f},         // unit
        {-0.5f, 0.5f, -0.5f, 0.5f},       // unit, mixed signs
        {0.5f, 0f, 0f, 0.5f},             // non-unit (len^2 = 0.5)
        {0f, 0.25f, 0f, 0.75f},
        {0.25f, -0.25f, 0.5f, 1f},
        {1f, 0f, 0f, 0f},                 // pure i (180 about x)
        {0f, 1f, 0f, 0f},                 // pure j
        {0f, 0f, 1f, 0f},                 // pure k
        {0.125f, 0.375f, -0.625f, 0.75f},
        {-0.5f, -0.5f, 0.5f, 0.5f},
        {2f, -3f, 4f, -5f},               // large, non-unit, signed
        {0.0625f, 0.0625f, 0.0625f, 0.0625f},
        {-1.5f, 0.5f, 2.25f, -0.75f},
        {0f, 0f, 0f, 2f},                 // scalar only, non-unit
    };

    // Euler angle triples (radians) for rotationXYZ. FINITE only.
    static final float[][] EULER = {
        {0f, 0f, 0f},
        {0.5f, 0.5f, 0.5f},
        {1.5707964f, 0f, 0f},
        {0f, 1.5707964f, 0f},
        {0f, 0f, 1.5707964f},
        {3.1415927f, 0.7853982f, -0.7853982f},
        {0.3f, 1.234f, -2.0f},
        {-3.0f, 2.5f, 0.123456f},
        {0.7853982f, 0.7853982f, 0.7853982f},
        {-1.5707964f, 1.0f, -0.5f},
    };

    public static void main(String[] args) throws Exception {
        // mul: every ordered pair (this * q). new from components so each row is fresh.
        for (int i = 0; i < QS.length; i++) {
            for (int j = 0; j < QS.length; j++) {
                Quaternionf a = new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3]);
                Quaternionf c = new Quaternionf(QS[j][0], QS[j][1], QS[j][2], QS[j][3]);
                a.mul(c);
                O.println("MUL\t" + i + "\t" + j + "\t" + q(a));
            }
        }
        // conjugate, normalize, invert (in-place on a fresh copy each time); dot, lengthSquared.
        for (int i = 0; i < QS.length; i++) {
            Quaternionf cj = new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3]).conjugate();
            O.println("CONJ\t" + i + "\t" + q(cj));

            Quaternionf nr = new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3]).normalize();
            O.println("NORM\t" + i + "\t" + q(nr));

            // invert is undefined at the zero quaternion (1/0); QS has no zero quat, all safe.
            Quaternionf iv = new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3]).invert();
            O.println("INV\t" + i + "\t" + q(iv));

            Quaternionf ls = new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3]);
            O.println("LENSQ\t" + i + "\t" + b(ls.lengthSquared()));

            for (int j = 0; j < QS.length; j++) {
                Quaternionf da = new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3]);
                Quaternionf db = new Quaternionf(QS[j][0], QS[j][1], QS[j][2], QS[j][3]);
                O.println("DOT\t" + i + "\t" + j + "\t" + b(da.dot(db)));
            }
        }
        // rotationXYZ (libm sin path).
        for (int e = 0; e < EULER.length; e++) {
            Quaternionf r = new Quaternionf().rotationXYZ(EULER[e][0], EULER[e][1], EULER[e][2]);
            O.println("ROTXYZ\t" + b(EULER[e][0]) + "\t" + b(EULER[e][1]) + "\t" + b(EULER[e][2]) + "\t" + q(r));
        }
    }
}

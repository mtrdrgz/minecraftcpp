// Ground-truth generator for the PURE (transcendental-free) arithmetic of
// org.joml.Vector4f (JOML 1.10.8 — the exact jar Minecraft 26.1.2 ships).
// Verifies the C++ port in mcpp/src/render/model/Vector4fMath.h (which reuses the
// certified render/model/Joml.h jfma / Matrix4f).
//
// Covered (all bit-exact: plain float add/sub/mul/div + org.joml.Math.fma, which by
// default is the two-rounding a*b+c, NOT java.lang.Math.fma):
//   set(Vector4fc), add/sub/mul/div(Vector4fc, dest), negate(dest),
//   fma(Vector4fc,Vector4fc,dest), fma(float,Vector4fc,dest),
//   lerp(Vector4fc,float,dest), dot(Vector4fc), lengthSquared(), distanceSquared(Vector4fc),
//   mul(Matrix4fc,dest)  [exercises all four property-dispatch kernels:
//                         IDENTITY / TRANSLATION / AFFINE / GENERIC].
//
// The matrix-transform rows carry the matrix as 16 raw float bits (column-major field
// order m00,m01,...,m33) PLUS its stored JOML properties() int, so the C++ side rebuilds
// a byte-identical matrix and reproduces the exact dispatch branch — no reliance on
// builder property inference matching across the port. Matrices are produced from JOML
// builders so each of the four property categories is represented:
//   IDENTITY   : new Matrix4f()
//   TRANSLATION: new Matrix4f().translation(...)            (props has TRANSLATION bit)
//   AFFINE     : scaling / rotation / scaling+rotate         (AFFINE, no TRANSLATION)
//   GENERIC    : perspective / frustum                       (PERSPECTIVE, no AFFINE)
//
// Every emitted value is raw IEEE-754 bits (Float.floatToRawIntBits); ints decimal.
//
//   tools/run_groundtruth.ps1 -Tool Vector4fMathParity -Out mcpp/build/vector4f_math.tsv

import org.joml.Matrix4f;
import org.joml.Vector4f;

@SuppressWarnings({"deprecation", "unchecked"})
public class Vector4fMathParity {
    static final java.io.PrintStream O = System.out;

    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // 4 raw float bits x,y,z,w of a Vector4f
    static String v4(Vector4f v) { return b(v.x) + "\t" + b(v.y) + "\t" + b(v.z) + "\t" + b(v.w); }

    // 16 raw float bits column-major field order + properties()
    static String m4p(Matrix4f m) {
        return b(m.m00()) + "\t" + b(m.m01()) + "\t" + b(m.m02()) + "\t" + b(m.m03()) + "\t"
             + b(m.m10()) + "\t" + b(m.m11()) + "\t" + b(m.m12()) + "\t" + b(m.m13()) + "\t"
             + b(m.m20()) + "\t" + b(m.m21()) + "\t" + b(m.m22()) + "\t" + b(m.m23()) + "\t"
             + b(m.m30()) + "\t" + b(m.m31()) + "\t" + b(m.m32()) + "\t" + b(m.m33()) + "\t"
             + m.properties();
    }

    static Vector4f V(float x, float y, float z, float w) { return new Vector4f(x, y, z, w); }

    // Finite, physically-plausible homogeneous vectors (clip-space-ish points/colors/dirs).
    static final Vector4f[] VECS = {
        V(0f, 0f, 0f, 1f),
        V(1f, 1f, 1f, 1f),
        V(-1f, 2.5f, -3.25f, 1f),
        V(0.5f, -0.25f, 0.125f, 1f),
        V(12.0f, -7.0f, 3.0f, 1f),
        V(100.0f, 50.0f, -25.0f, 2.0f),
        V(-0.333f, 0.666f, -0.999f, 0.5f),
        V(3.14159f, -2.71828f, 1.41421f, 1.5f),
        V(0f, 0f, 0f, 0f),
        V(-8.5f, 0f, 16.25f, -1f),
    };

    // Matrices spanning every property-dispatch branch of mul(Matrix4fc, dest).
    static Matrix4f[] mats() {
        return new Matrix4f[] {
            new Matrix4f(),                                            // IDENTITY
            new Matrix4f().translation(3.0f, -5.0f, 7.0f),            // TRANSLATION
            new Matrix4f().translation(-0.25f, 12.5f, 0.0f),         // TRANSLATION
            new Matrix4f().scaling(2.0f, 0.5f, -3.0f),               // AFFINE (no translation)
            new Matrix4f().scaling(1.0f, 1.0f, 1.0f),                // AFFINE-ish (orthonormal scale 1)
            new Matrix4f().rotation((float)(Math.PI / 3.0), 0.0f, 1.0f, 0.0f), // AFFINE rotation
            new Matrix4f().rotationXYZ(0.3f, -0.7f, 1.1f),           // AFFINE rotation
            new Matrix4f().rotationXYZ(0.3f, -0.7f, 1.1f).scale(2.0f, 3.0f, 0.5f), // AFFINE rot+scale
            new Matrix4f().translationRotateScale(1f, 2f, 3f, 0f, 0.2f, 0f, 0.98f, 1.5f, 1.5f, 1.5f), // AFFINE+translation
            new Matrix4f().perspective(1.2217305f, 16.0f / 9.0f, 0.05f, 1000.0f, true),  // GENERIC (perspective)
            new Matrix4f().perspective(1.0471976f, 4.0f / 3.0f, 0.1f, 100.0f, false),    // GENERIC (perspective)
            new Matrix4f().frustum(-1f, 1f, -1f, 1f, 0.05f, 100f, true),                 // GENERIC (frustum)
        };
    }

    static void emitMul(int mi, Matrix4f m, int vi, Vector4f v) {
        Vector4f src = new Vector4f(v);            // preserve input vector (mul writes dest)
        Vector4f dest = new Vector4f();            // separate dest (default 0,0,0,1)
        src.mul(m, dest);
        // tag \t mi \t vi \t srcV4 \t m4p[17] \t destV4
        O.println("MUL\t" + mi + "\t" + vi + "\t" + v4(v) + "\t" + m4p(m) + "\t" + v4(dest));
    }

    public static void main(String[] args) {
        // ── matrix transform mul(Matrix4fc, dest) over every (matrix, vector) pair ──
        Matrix4f[] MATS = mats();
        for (int mi = 0; mi < MATS.length; mi++)
            for (int vi = 0; vi < VECS.length; vi++)
                emitMul(mi, MATS[mi], vi, VECS[vi]);

        // ── component-wise binary ops over every ordered (a, b) vector pair ──────────
        for (int ai = 0; ai < VECS.length; ai++) {
            for (int bi = 0; bi < VECS.length; bi++) {
                Vector4f a = VECS[ai], bb = VECS[bi];
                // add
                O.println("ADD\t" + v4(a) + "\t" + v4(bb) + "\t" + v4(new Vector4f(a).add(bb, new Vector4f())));
                // sub
                O.println("SUB\t" + v4(a) + "\t" + v4(bb) + "\t" + v4(new Vector4f(a).sub(bb, new Vector4f())));
                // mul (component-wise)
                O.println("MULV\t" + v4(a) + "\t" + v4(bb) + "\t" + v4(new Vector4f(a).mul(bb, new Vector4f())));
                // div (component-wise)
                O.println("DIV\t" + v4(a) + "\t" + v4(bb) + "\t" + v4(new Vector4f(a).div(bb, new Vector4f())));
                // dot
                O.println("DOT\t" + v4(a) + "\t" + v4(bb) + "\t" + b(a.dot(bb)));
                // distanceSquared
                O.println("DIST2\t" + v4(a) + "\t" + v4(bb) + "\t" + b(a.distanceSquared(bb)));
            }
        }

        // ── unary / scalar ops over every vector ─────────────────────────────────────
        float[] TS = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, -0.5f, 1.5f, 3.14159f };
        for (int ai = 0; ai < VECS.length; ai++) {
            Vector4f a = VECS[ai];
            O.println("NEG\t" + v4(a) + "\t" + v4(new Vector4f(a).negate(new Vector4f())));
            O.println("LEN2\t" + v4(a) + "\t" + b(a.lengthSquared()));
            // lerp toward every other vector at several t
            for (int bi = 0; bi < VECS.length; bi++) {
                Vector4f bb = VECS[bi];
                for (float t : TS)
                    O.println("LERP\t" + v4(a) + "\t" + v4(bb) + "\t" + b(t)
                            + "\t" + v4(new Vector4f(a).lerp(bb, t, new Vector4f())));
            }
            // fma(Vector4fc, Vector4fc, dest) and fma(float, Vector4fc, dest)
            for (int bi = 0; bi < VECS.length; bi++) {
                Vector4f bb = VECS[bi];
                for (int ci = 0; ci < VECS.length; ci++) {
                    Vector4f c = VECS[ci];
                    O.println("FMAVV\t" + v4(a) + "\t" + v4(bb) + "\t" + v4(c)
                            + "\t" + v4(new Vector4f(a).fma(bb, c, new Vector4f())));
                }
                for (float t : TS)
                    O.println("FMASV\t" + v4(a) + "\t" + b(t) + "\t" + v4(bb)
                            + "\t" + v4(new Vector4f(a).fma(t, bb, new Vector4f())));
            }
        }
    }
}

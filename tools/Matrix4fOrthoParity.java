// Ground-truth generator for the PURE orthographic-projection setters of
// org.joml.Matrix4f (JOML 1.10.8 — the exact jar Minecraft 26.1.2 ships).
// Verifies the C++ port in mcpp/src/render/model/Matrix4fOrtho.h (which reuses
// the certified render/model/Joml.h Matrix4f).
//
// Covered (transcendental-free -> bit-exact float arithmetic):
//   setOrtho(l, r, b, t, zNear, zFar, zZeroToOne)            (FFFFFFZ)
//   setOrthoLH(l, r, b, t, zNear, zFar, zZeroToOne)          (FFFFFFZ)
//   setOrthoSymmetric(width, height, zNear, zFar, zZeroToOne) (FFFFZ)
//   setOrthoSymmetricLH(width, height, zNear, zFar, zZeroToOne) (FFFFZ)
//
// Each row carries every input as raw float bits + the zZeroToOne flag, and the
// full resulting matrix as 16 raw float bits (column-major field order
// m00,m01,...,m33) plus its stored JOML properties() int. To exercise the
// `(properties & PROPERTY_IDENTITY) != 0` reset guard from the bytecode, every
// call is run twice: once on a fresh identity Matrix4f, and once on a matrix
// pre-dirtied with a translation (so the setter must re-identity first).
//
//   tools/run_groundtruth.ps1 -Tool Matrix4fOrthoParity -Out mcpp/build/matrix4f_ortho.tsv

import org.joml.Matrix4f;

public class Matrix4fOrthoParity {
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

    // Fresh identity matrix (properties has PROPERTY_IDENTITY set -> setter skips reset).
    static Matrix4f ident() { return new Matrix4f(); }

    // Pre-dirtied matrix: a translation clears PROPERTY_IDENTITY -> setter must
    // re-identity (and overwrite the m30/m31/m32 translation it left behind).
    static Matrix4f dirty() { return new Matrix4f().translation(3.0f, -5.0f, 7.0f); }

    // 6-arg (FFFFFFZ) row: tag \t l r b t zn zf (6 floats) \t z01 \t result m4p (17)
    static void emit6(String tag, float l, float r, float bb, float t, float zn, float zf, boolean z01, Matrix4f m) {
        O.println(tag + "\t" + b(l) + "\t" + b(r) + "\t" + b(bb) + "\t" + b(t) + "\t" + b(zn) + "\t" + b(zf)
                + "\t" + (z01 ? 1 : 0) + "\t" + m4p(m));
    }

    // 5-arg (FFFFZ) row: tag \t w h zn zf (4 floats) \t z01 \t result m4p (17)
    static void emit5(String tag, float w, float h, float zn, float zf, boolean z01, Matrix4f m) {
        O.println(tag + "\t" + b(w) + "\t" + b(h) + "\t" + b(zn) + "\t" + b(zf)
                + "\t" + (z01 ? 1 : 0) + "\t" + m4p(m));
    }

    public static void main(String[] args) {
        // Finite, physically-plausible ortho bounds: GUI-style (0..W, 0..H), centered,
        // inverted-Y, and a few generic ranges + near/far pairs.
        float[][] LRBT = {
            { 0f,  854f, 480f,   0f },   // GUI default (invertY layout: top=0,bottom=H)
            { 0f, 1920f,   0f, 1080f },  // GUI non-inverted
            {-1f,    1f,  -1f,   1f },   // centered unit
            {-10f,  10f, -7.5f, 7.5f },  // centered wide
            { 5f,   25f,  3f,   13f },   // off-origin
            { 0.5f, 4.5f, -2.25f, 6.75f },
            {-100f, 50f, -30f, 120f },
        };
        float[][] ZNF = {
            { -1f,  1f },
            { 1000f, -1000f },   // Minecraft GUI uses near=1000, far=-1000 era ranges
            { 0.05f, 1000f },
            { 0.1f,  100f },
            { -3.5f, 21.5f },
            { 0f,    16f },
        };
        boolean[] Z = { false, true };

        for (float[] q : LRBT)
            for (float[] nf : ZNF)
                for (boolean z01 : Z) {
                    float l = q[0], r = q[1], bb = q[2], t = q[3], zn = nf[0], zf = nf[1];
                    emit6("ORTHO_ID",      l, r, bb, t, zn, zf, z01, ident().setOrtho(l, r, bb, t, zn, zf, z01));
                    emit6("ORTHO_DIRTY",   l, r, bb, t, zn, zf, z01, dirty().setOrtho(l, r, bb, t, zn, zf, z01));
                    emit6("ORTHOLH_ID",    l, r, bb, t, zn, zf, z01, ident().setOrthoLH(l, r, bb, t, zn, zf, z01));
                    emit6("ORTHOLH_DIRTY", l, r, bb, t, zn, zf, z01, dirty().setOrthoLH(l, r, bb, t, zn, zf, z01));
                }

        // Symmetric variants: width/height (full extents) + near/far.
        float[][] WH = {
            { 2f, 2f }, { 854f, 480f }, { 1920f, 1080f }, { 20f, 15f },
            { 4f, 9f }, { 0.5f, 0.25f }, { 100f, 30f },
        };
        for (float[] wh : WH)
            for (float[] nf : ZNF)
                for (boolean z01 : Z) {
                    float w = wh[0], h = wh[1], zn = nf[0], zf = nf[1];
                    emit5("ORTHOSYM_ID",      w, h, zn, zf, z01, ident().setOrthoSymmetric(w, h, zn, zf, z01));
                    emit5("ORTHOSYM_DIRTY",   w, h, zn, zf, z01, dirty().setOrthoSymmetric(w, h, zn, zf, z01));
                    emit5("ORTHOSYMLH_ID",    w, h, zn, zf, z01, ident().setOrthoSymmetricLH(w, h, zn, zf, z01));
                    emit5("ORTHOSYMLH_DIRTY", w, h, zn, zf, z01, dirty().setOrthoSymmetricLH(w, h, zn, zf, z01));
                }
    }
}

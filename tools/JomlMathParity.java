// Ground-truth generator for the org.joml operations the C++ render/model/Joml.h
// port mirrors — restricted to the DETERMINISTIC (non-transcendental) subset that
// is bit-exact regardless of libm: Matrix4f rotation(Quaternionf)/mul/translate/
// scale/invertAffine/transformPosition/transformDirection, Vector3f ops, Matrix3f
// mul/scaling, GeometryUtils.normal. (rotationX/Y/Z and rotation(angle,axis) call
// Math.sin and are libm-dependent, so they are intentionally excluded.)
//
// All matrices are built from rotation(quaternion) with EXACT float quaternion
// components, so no sin/cos enters. Floats exchanged as raw IEEE-754 bits.
//
//   tools/run_groundtruth.ps1 -Tool JomlMathParity -Out mcpp/build/joml_math.tsv

import org.joml.Matrix3f;
import org.joml.Matrix4f;
import org.joml.Quaternionf;
import org.joml.Vector3f;

public class JomlMathParity {
    static final java.io.PrintStream O = System.out;
    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static String m4(Matrix4f m) {
        return b(m.m00()) + "\t" + b(m.m01()) + "\t" + b(m.m02()) + "\t" + b(m.m03()) + "\t"
             + b(m.m10()) + "\t" + b(m.m11()) + "\t" + b(m.m12()) + "\t" + b(m.m13()) + "\t"
             + b(m.m20()) + "\t" + b(m.m21()) + "\t" + b(m.m22()) + "\t" + b(m.m23()) + "\t"
             + b(m.m30()) + "\t" + b(m.m31()) + "\t" + b(m.m32()) + "\t" + b(m.m33());
    }
    static String v3(Vector3f v) { return b(v.x) + "\t" + b(v.y) + "\t" + b(v.z); }

    // Exact-float quaternion components (no normalization needed for bit-exactness).
    static final float[][] QS = {
        {0,0,0,1}, {0.5f,0.5f,0.5f,0.5f}, {0.5f,0,0,0.5f}, {0,0.25f,0,0.75f},
        {-0.5f,0.5f,-0.5f,0.5f}, {0.25f,-0.25f,0.5f,1f}, {1,0,0,0}, {0,1,0,0},
        {0.125f,0.375f,-0.625f,0.75f}, {-0.5f,-0.5f,0.5f,0.5f}
    };
    static final float[][] VS = {
        {0,0,0}, {1,2,3}, {-1.5f,0.5f,2.25f}, {10,-20,30}, {0.125f,-0.375f,0.625f}, {100,100,100}
    };
    static final float[][] TS = { {0,0,0}, {1,2,3}, {-5,0.5f,7}, {0.25f,-0.75f,1.5f} };
    static final float[][] SS = { {1,1,1}, {2,3,4}, {-1,0.5f,2}, {0.25f,0.25f,0.25f} };

    static Matrix4f rot(float[] q) { return new Matrix4f().rotation(new Quaternionf(q[0], q[1], q[2], q[3])); }

    public static void main(String[] args) {
        for (int i = 0; i < QS.length; i++) {
            O.println("ROT\t" + i + "\t" + m4(rot(QS[i])));
            for (float[] t : TS) O.println("TRANSLATE\t" + i + "\t" + b(t[0]) + "\t" + b(t[1]) + "\t" + b(t[2]) + "\t" + m4(rot(QS[i]).translate(t[0], t[1], t[2])));
            for (float[] s : SS) O.println("SCALE\t" + i + "\t" + b(s[0]) + "\t" + b(s[1]) + "\t" + b(s[2]) + "\t" + m4(rot(QS[i]).scale(s[0], s[1], s[2])));
            O.println("INVAFF\t" + i + "\t" + m4(rot(QS[i]).invertAffine()));
            for (float[] v : VS) {
                Vector3f p = new Vector3f(v[0], v[1], v[2]); rot(QS[i]).transformPosition(p);
                O.println("TFPOS\t" + i + "\t" + b(v[0]) + "\t" + b(v[1]) + "\t" + b(v[2]) + "\t" + v3(p));
                Vector3f d = new Vector3f(v[0], v[1], v[2]); rot(QS[i]).transformDirection(d);
                O.println("TFDIR\t" + i + "\t" + b(v[0]) + "\t" + b(v[1]) + "\t" + b(v[2]) + "\t" + v3(d));
            }
        }
        for (int i = 0; i < QS.length; i++)
            for (int j = 0; j < QS.length; j++)
                O.println("MUL\t" + i + "\t" + j + "\t" + m4(rot(QS[i]).mul(rot(QS[j]))));

        // Transcendental rotations — does (float)Math.sin survive the float cast
        // identically to the C++ (float)std::sin? Emit and let the gate decide.
        float[] ANG = { 0f, 0.5f, 1.0f, 1.5707964f, 3.1415927f, 0.3f, 1.234f, -2.0f,
                        0.7853982f, -0.7853982f, 2.5f, 6.2831855f, 0.123456f, -3.0f };
        for (float a : ANG) {
            O.println("ROTX\t" + b(a) + "\t" + m4(new Matrix4f().rotationX(a)));
            O.println("ROTY\t" + b(a) + "\t" + m4(new Matrix4f().rotationY(a)));
            O.println("ROTZ\t" + b(a) + "\t" + m4(new Matrix4f().rotationZ(a)));
            O.println("ROTAXIS\t" + b(a) + "\t" + b(0.5773503f) + "\t" + b(0.5773503f) + "\t" + b(0.5773503f)
                + "\t" + m4(new Matrix4f().rotation(a, 0.5773503f, 0.5773503f, 0.5773503f)));
        }

        // Vector3f ops — rows carry inputs(3[,3]) then result.
        for (float[] a : VS) for (float[] c : VS) {
            String in = b(a[0])+"\t"+b(a[1])+"\t"+b(a[2])+"\t"+b(c[0])+"\t"+b(c[1])+"\t"+b(c[2]);
            O.println("VADD\t" + in + "\t" + v3(new Vector3f(a[0],a[1],a[2]).add(new Vector3f(c[0],c[1],c[2]))));
            O.println("VSUB\t" + in + "\t" + v3(new Vector3f(a[0],a[1],a[2]).sub(new Vector3f(c[0],c[1],c[2]))));
            O.println("VMUL\t" + in + "\t" + v3(new Vector3f(a[0],a[1],a[2]).mul(new Vector3f(c[0],c[1],c[2]))));
            O.println("VDOT\t" + in + "\t" + b(new Vector3f(a[0],a[1],a[2]).dot(new Vector3f(c[0],c[1],c[2]))));
        }
        for (float[] a : VS) {
            O.println("VDIV\t" + b(a[0])+"\t"+b(a[1])+"\t"+b(a[2]) + "\t" + b(2.0f) + "\t" + v3(new Vector3f(a[0],a[1],a[2]).div(2.0f)));
            O.println("VNORM\t" + b(a[0]+1f)+"\t"+b(a[1]+1f)+"\t"+b(a[2]+1f) + "\t" + v3(new Vector3f(a[0]+1f,a[1]+1f,a[2]+1f).normalize()));
        }
        // GeometryUtils.normal over triangles
        for (int x = 0; x < VS.length; x++) {
            float[] v0 = VS[(x) % VS.length], v1 = VS[(x+1) % VS.length], v2 = VS[(x+2) % VS.length];
            Vector3f dest = new Vector3f();
            org.joml.GeometryUtils.normal(new Vector3f(v0[0],v0[1],v0[2]), new Vector3f(v1[0],v1[1],v1[2]), new Vector3f(v2[0],v2[1],v2[2]), dest);
            O.println("TRINORM\t"+b(v0[0])+"\t"+b(v0[1])+"\t"+b(v0[2])+"\t"+b(v1[0])+"\t"+b(v1[1])+"\t"+b(v1[2])+"\t"+b(v2[0])+"\t"+b(v2[1])+"\t"+b(v2[2])+"\t"+v3(dest));
        }
        // Matrix3f scaling + mul
        for (float[] s : SS) {
            Matrix3f m = new Matrix3f().scaling(s[0], s[1], s[2]);
            O.println("M3SCALE\t"+b(s[0])+"\t"+b(s[1])+"\t"+b(s[2])+"\t"
                + b(m.m00())+"\t"+b(m.m01())+"\t"+b(m.m02())+"\t"+b(m.m10())+"\t"+b(m.m11())+"\t"+b(m.m12())+"\t"+b(m.m20())+"\t"+b(m.m21())+"\t"+b(m.m22()));
            for (float[] s2 : SS) {
                Matrix3f r = new Matrix3f().scaling(s[0],s[1],s[2]).mul(new Matrix3f().scaling(s2[0],s2[1],s2[2]));
                O.println("M3MUL\t"+b(s[0])+"\t"+b(s[1])+"\t"+b(s[2])+"\t"+b(s2[0])+"\t"+b(s2[1])+"\t"+b(s2[2])+"\t"
                    + b(r.m00())+"\t"+b(r.m01())+"\t"+b(r.m02())+"\t"+b(r.m10())+"\t"+b(r.m11())+"\t"+b(r.m12())+"\t"+b(r.m20())+"\t"+b(r.m21())+"\t"+b(r.m22()));
            }
        }
    }
}

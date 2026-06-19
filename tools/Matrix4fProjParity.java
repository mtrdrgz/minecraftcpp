// Ground-truth generator for the org.joml.Matrix4f projection builders Minecraft
// 26.1.2 uses to build camera/HUD projection matrices:
//
//   - Matrix4f.setOrtho(left,right,bottom,top,zNear,zFar,zZeroToOne)
//   - Matrix4f.setPerspective(fovy,aspect,zNear,zFar,zZeroToOne)
//   - Matrix4f.perspective(fovy,aspect,zNear,zFar,zZeroToOne)  (on a fresh identity
//     matrix, which is the only state Camera.createProjectionMatrixForCulling uses)
//
// Calls the REAL org.joml.Matrix4f (from the shipped joml-1.10.8.jar). Emits the
// 16 matrix floats as raw IEEE-754 bits (%08x). FINITE/PHYSICAL inputs only.
//
// setPerspective goes through org.joml.Math.tan = (float) java.lang.Math.tan((double)x),
// a libm transcendental — the C++ test uses std::tan; this gate asserts the two agree
// bit-for-bit on these inputs.
//
//   tools/run_groundtruth.ps1 -Tool Matrix4fProjParity -Out mcpp/build/matrix4f_proj.tsv

import org.joml.Matrix4f;

public class Matrix4fProjParity {
    static final java.io.PrintStream O = System.out;
    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String z(boolean v) { return v ? "1" : "0"; }

    static String m16(Matrix4f m) {
        return b(m.m00()) + "\t" + b(m.m01()) + "\t" + b(m.m02()) + "\t" + b(m.m03()) + "\t"
             + b(m.m10()) + "\t" + b(m.m11()) + "\t" + b(m.m12()) + "\t" + b(m.m13()) + "\t"
             + b(m.m20()) + "\t" + b(m.m21()) + "\t" + b(m.m22()) + "\t" + b(m.m23()) + "\t"
             + b(m.m30()) + "\t" + b(m.m31()) + "\t" + b(m.m32()) + "\t" + b(m.m33());
    }

    // Finite, physically meaningful orthographic frusta.
    // {left, right, bottom, top, zNear, zFar}
    static final float[][] ORTHO = {
        {0f, 854f, 0f, 480f, 1000f, 21000f},      // typical Minecraft HUD/ortho
        {0f, 854f, 480f, 0f, 1000f, 21000f},      // invertY variant (bottom>top)
        {0f, 1920f, 0f, 1080f, 1000f, 21000f},
        {0f, 1920f, 1080f, 0f, 1000f, 21000f},
        {-1f, 1f, -1f, 1f, -1f, 1f},              // unit cube
        {-1f, 1f, 1f, -1f, -1f, 1f},
        {0f, 1280f, 0f, 720f, 0.05f, 1000f},
        {0f, 1280f, 720f, 0f, 0.05f, 1000f},
        {-10f, 10f, -5f, 5f, 0.1f, 100f},
        {2.5f, 7.5f, -3.25f, 4.75f, 0.5f, 50f},
        {0f, 320f, 0f, 240f, 1f, 16f},
        {-100f, 100f, -100f, 100f, 0.5f, 2048f},
    };

    // Finite, physically meaningful perspective parameters.
    // {fovy(radians), aspect, zNear, zFar}
    static final float[][] PERSP = {
        {1.2217305f, 16f / 9f, 0.05f, 1024f},         // 70deg, 16:9 (default MC FOV)
        {1.5707964f, 16f / 9f, 0.05f, 1024f},         // 90deg
        {0.7853982f, 4f / 3f, 0.05f, 256f},           // 45deg, 4:3
        {1.0471976f, 21f / 9f, 0.1f, 4096f},          // 60deg, ultrawide
        {1.2217305f, 1f, 0.05f, 1000f},               // 70deg, square
        {1.3962634f, 1280f / 720f, 0.05f, 512f},      // 80deg
        {0.5235988f, 16f / 10f, 0.5f, 100f},          // 30deg
        {2.0943952f, 16f / 9f, 0.05f, 2048f},         // 120deg
        {1.2217305f, 2.5f, 0.01f, 32f},
        {1.5707964f, 1.0f, 1.0f, 100f},
    };

    public static void main(String[] args) throws Exception {
        // org.joml.Matrix4f's projection builders are pure float math with no
        // net.minecraft registry/world dependency, so no Bootstrap is needed. (If a
        // future addition pulls in a net.minecraft type that errors with "Not
        // bootstrapped", prepend: net.minecraft.SharedConstants.tryDetectVersion();
        // net.minecraft.server.Bootstrap.bootStrap();)

        boolean[] zzto = { false, true };

        for (float[] o : ORTHO) {
            for (boolean z2o : zzto) {
                Matrix4f m = new Matrix4f().setOrtho(o[0], o[1], o[2], o[3], o[4], o[5], z2o);
                O.println("ORTHO\t" + b(o[0]) + "\t" + b(o[1]) + "\t" + b(o[2]) + "\t" + b(o[3])
                    + "\t" + b(o[4]) + "\t" + b(o[5]) + "\t" + z(z2o) + "\t" + m16(m));
            }
        }

        for (float[] p : PERSP) {
            for (boolean z2o : zzto) {
                Matrix4f sp = new Matrix4f().setPerspective(p[0], p[1], p[2], p[3], z2o);
                O.println("SETPERSP\t" + b(p[0]) + "\t" + b(p[1]) + "\t" + b(p[2]) + "\t" + b(p[3])
                    + "\t" + z(z2o) + "\t" + m16(sp));

                // perspective(...) on a fresh identity matrix dispatches to setPerspective;
                // this is exactly what Camera.createProjectionMatrixForCulling exercises.
                Matrix4f pp = new Matrix4f().perspective(p[0], p[1], p[2], p[3], z2o);
                O.println("PERSP\t" + b(p[0]) + "\t" + b(p[1]) + "\t" + b(p[2]) + "\t" + b(p[3])
                    + "\t" + z(z2o) + "\t" + m16(pp));
            }
        }
    }
}

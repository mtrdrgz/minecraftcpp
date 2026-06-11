// Ground-truth generator for the pure orientation math of
// net.minecraft.client.Camera#setRotation (MC 26.1.2), mirrored by
// client/CameraRotationMath.h.
//
// Camera cannot be reflected (its field initialisers call Minecraft.getInstance()
// and allocate GL resources), so this harness drives the REAL org.joml classes
// the engine links — org.joml.Quaternionf#rotationYXZ and
// org.joml.Vector3f#rotate(Quaternionfc) — and reproduces the setRotation body
// verbatim from Camera.java:
//
//   FORWARDS = new Vector3f(0, 0, -1);
//   UP       = new Vector3f(0, 1,  0);
//   LEFT     = new Vector3f(-1, 0, 0);
//   rotation.rotationYXZ((float)Math.PI - yRot*(float)(Math.PI/180.0),
//                        -xRot*(float)(Math.PI/180.0), 0.0F);
//   FORWARDS.rotate(rotation, forwards);
//   UP.rotate(rotation, up);
//   LEFT.rotate(rotation, left);
//
// Runs under JOML's DEFAULT options (joml.useMathFma=false, joml.fastmath=false),
// so Math.fma == plain a*b+c and Math.sin == (float)Math.sin((double)x).
//
// Three row types are emitted so a mismatch localises to the failing primitive:
//   ROTYXZ  <angleY> <angleX> <angleZ>  <qx qy qz qw>   (rotationYXZ alone)
//   VROTATE <qx qy qz qw> <vx vy vz>     <rx ry rz>      (Vector3f.rotate alone)
//   SETROT  <yRotDeg> <xRotDeg>          <qx qy qz qw> <fx fy fz> <ux uy uz> <lx ly lz>
//
// Floats exchanged as raw IEEE-754 bits (%08x of Float.floatToRawIntBits).
//
//   tools/run_groundtruth.ps1 -Tool CameraRotationMathParity -Out mcpp/build/camera_rotation_math.tsv

import org.joml.Quaternionf;
import org.joml.Vector3f;

public class CameraRotationMathParity {
    static final java.io.PrintStream O = System.out;
    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String q(Quaternionf p) { return b(p.x) + "\t" + b(p.y) + "\t" + b(p.z) + "\t" + b(p.w); }
    static String v(Vector3f p) { return b(p.x) + "\t" + b(p.y) + "\t" + b(p.z); }

    // Exact-float Euler triples (radians) for rotationYXZ(angleY, angleX, angleZ).
    // FINITE only; real edges incl. +-PI, +-PI/2, the angles setRotation feeds, zeros.
    static final float[][] EULER = {
        {0f, 0f, 0f},
        {1.5707964f, 0f, 0f},
        {0f, 1.5707964f, 0f},
        {0f, 0f, 1.5707964f},
        {3.1415927f, 0f, 0f},
        {3.1415927f, 0.7853982f, 0f},
        {0.5f, 0.5f, 0.5f},
        {-1.5707964f, 1.0f, 0f},
        {2.3561945f, -0.7853982f, 0f},
        {0.7853982f, 0.7853982f, 0.7853982f},
        {-3.0f, 2.5f, 0f},
        {6.2831855f, -0.123456f, 0f},
    };

    // Exact-float quaternions to feed Vector3f.rotate (unit + non-unit + signed).
    static final float[][] QS = {
        {0f, 0f, 0f, 1f},
        {0.5f, 0.5f, 0.5f, 0.5f},
        {-0.5f, 0.5f, -0.5f, 0.5f},
        {0.5f, 0f, 0f, 0.5f},          // non-unit (k != 1 path)
        {0f, 0.7071068f, 0f, 0.7071068f},
        {0.25f, -0.25f, 0.5f, 1f},
        {1f, 0f, 0f, 0f},
        {-1.5f, 0.5f, 2.25f, -0.75f},  // large, non-unit, signed
    };

    // Exact-float vectors for VROTATE incl. the camera basis vectors.
    static final float[][] VS = {
        {0f, 0f, -1f},      // FORWARDS
        {0f, 1f, 0f},       // UP
        {-1f, 0f, 0f},      // LEFT
        {1f, 0f, 0f},
        {0.5f, -0.25f, 0.75f},
        {-2f, 3f, -4f},
    };

    // (yRotDeg, xRotDeg) the camera actually receives: yaw in [-180,180], pitch in
    // [-90,90] plus the literal edges and a couple of arbitrary finite values.
    static final float[][] ANGLES = {
        {0f, 0f},
        {90f, 0f},
        {-90f, 0f},
        {180f, 0f},
        {-180f, 0f},
        {0f, 90f},
        {0f, -90f},
        {45f, 30f},
        {-135f, -45f},
        {135.5f, 12.5f},
        {360f, 0f},
        {-179.9f, 89.9f},
        {12.34f, -56.78f},
    };

    // Verbatim copy of net.minecraft.client.Camera#setRotation (the part that
    // produces rotation/forwards/up/left). Uses the REAL org.joml classes.
    static void setRotation(float yRot, float xRot,
                            Quaternionf rotation, Vector3f forwards, Vector3f up, Vector3f left) {
        final Vector3f FORWARDS = new Vector3f(0.0F, 0.0F, -1.0F);
        final Vector3f UP = new Vector3f(0.0F, 1.0F, 0.0F);
        final Vector3f LEFT = new Vector3f(-1.0F, 0.0F, 0.0F);
        rotation.rotationYXZ((float) Math.PI - yRot * (float) (Math.PI / 180.0),
                             -xRot * (float) (Math.PI / 180.0), 0.0F);
        FORWARDS.rotate(rotation, forwards);
        UP.rotate(rotation, up);
        LEFT.rotate(rotation, left);
    }

    public static void main(String[] args) throws Exception {
        for (int e = 0; e < EULER.length; e++) {
            Quaternionf r = new Quaternionf().rotationYXZ(EULER[e][0], EULER[e][1], EULER[e][2]);
            O.println("ROTYXZ\t" + b(EULER[e][0]) + "\t" + b(EULER[e][1]) + "\t" + b(EULER[e][2]) + "\t" + q(r));
        }

        for (int i = 0; i < QS.length; i++) {
            for (int j = 0; j < VS.length; j++) {
                Quaternionf qq = new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3]);
                Vector3f vec = new Vector3f(VS[j][0], VS[j][1], VS[j][2]);
                Vector3f dest = new Vector3f();
                vec.rotate(qq, dest);
                O.println("VROTATE\t" + q(qq) + "\t" + v(vec) + "\t" + v(dest));
            }
        }

        for (int a = 0; a < ANGLES.length; a++) {
            Quaternionf rot = new Quaternionf();
            Vector3f fwd = new Vector3f(), up = new Vector3f(), left = new Vector3f();
            setRotation(ANGLES[a][0], ANGLES[a][1], rot, fwd, up, left);
            O.println("SETROT\t" + b(ANGLES[a][0]) + "\t" + b(ANGLES[a][1]) + "\t"
                + q(rot) + "\t" + v(fwd) + "\t" + v(up) + "\t" + v(left));
        }
    }
}

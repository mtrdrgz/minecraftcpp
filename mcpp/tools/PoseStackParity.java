// Ground-truth generator for com.mojang.blaze3d.vertex.PoseStack (Minecraft 26.1.2).
//
// Drives the REAL PoseStack through batteries of op sequences and, after every op,
// dumps the top Pose's 16 pose floats + 9 normal floats (raw IEEE-754 bits, tab
// separated). The C++ pose_stack_parity test replays the identical sequences and
// compares bit-for-bit. PoseStack.Pose's pose/normal fields are private, read via
// reflection. All matrix construction uses exact-float quaternion / translate /
// scale values so the only transcendental-free org.joml paths are exercised
// (rotation(quat)/rotateTranslation/rotateAffine, mul, invert, transpose — all
// fma/plain-arith, bit-exact regardless of libm).
//
//   tools/run_groundtruth.ps1 -Tool PoseStackParity -Out mcpp/build/pose_stack.tsv
//
// Row format:  <TAG>\t<seq>\t<step>\t<16 pose>\t<9 normal>\t<trustedNormals 0/1>

import com.mojang.blaze3d.vertex.PoseStack;
import java.lang.reflect.Field;
import org.joml.Matrix3f;
import org.joml.Matrix4f;
import org.joml.Quaternionf;

public class PoseStackParity {
    static final java.io.PrintStream O = System.out;

    static Field F_POSE, F_NORMAL, F_TRUSTED;
    static {
        try {
            Class<?> pc = Class.forName("com.mojang.blaze3d.vertex.PoseStack$Pose");
            F_POSE = pc.getDeclaredField("pose");     F_POSE.setAccessible(true);
            F_NORMAL = pc.getDeclaredField("normal"); F_NORMAL.setAccessible(true);
            F_TRUSTED = pc.getDeclaredField("trustedNormals"); F_TRUSTED.setAccessible(true);
        } catch (Exception e) { throw new RuntimeException(e); }
    }

    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static String m4(Matrix4f m) {
        return b(m.m00()) + "\t" + b(m.m01()) + "\t" + b(m.m02()) + "\t" + b(m.m03()) + "\t"
             + b(m.m10()) + "\t" + b(m.m11()) + "\t" + b(m.m12()) + "\t" + b(m.m13()) + "\t"
             + b(m.m20()) + "\t" + b(m.m21()) + "\t" + b(m.m22()) + "\t" + b(m.m23()) + "\t"
             + b(m.m30()) + "\t" + b(m.m31()) + "\t" + b(m.m32()) + "\t" + b(m.m33());
    }
    static String m3(Matrix3f m) {
        return b(m.m00()) + "\t" + b(m.m01()) + "\t" + b(m.m02()) + "\t"
             + b(m.m10()) + "\t" + b(m.m11()) + "\t" + b(m.m12()) + "\t"
             + b(m.m20()) + "\t" + b(m.m21()) + "\t" + b(m.m22());
    }

    // Emit the top Pose state.
    static int seq, step;
    static void dump(String tag, PoseStack ps) {
        try {
            Object pose = ps.last();
            Matrix4f p = (Matrix4f) F_POSE.get(pose);
            Matrix3f n = (Matrix3f) F_NORMAL.get(pose);
            boolean tr = F_TRUSTED.getBoolean(pose);
            O.println(tag + "\t" + seq + "\t" + step + "\t" + m4(p) + "\t" + m3(n) + "\t" + (tr ? 1 : 0));
            step++;
        } catch (Exception e) { throw new RuntimeException(e); }
    }

    // Exact-float quaternions (already unit, no normalization needed for bit match).
    static final float[][] QS = {
        {0,0,0,1}, {0.5f,0.5f,0.5f,0.5f}, {0.5f,0,0,0.5f}, {0,0.25f,0,0.75f},
        {-0.5f,0.5f,-0.5f,0.5f}, {0.125f,0.375f,-0.625f,0.75f}, {1,0,0,0},
        {0,1,0,0}, {0,0,1,0}, {-0.5f,-0.5f,0.5f,0.5f}
    };
    static final float[][] TS = { {1,2,3}, {-5,0.5f,7}, {0.25f,-0.75f,1.5f}, {0,0,0}, {-1.5f,2.5f,-3.5f} };
    static final float[][] SS = { {2,3,4}, {-1,0.5f,2}, {0.25f,0.25f,0.25f}, {-2,-2,-2}, {2,2,2},
                                  {-1,-1,-1}, {1,1,1}, {0.5f,-0.5f,0.5f}, {-3,3,3}, {-2,2,-2} };

    static Quaternionf q(int i) { return new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3]); }

    public static void main(String[] args) throws Exception {
        seq = 0;

        // --- B1: identity -> mulPose(quat)  (Pose.rotate from identity: rotation path)
        for (int i = 0; i < QS.length; i++) {
            PoseStack ps = new PoseStack(); step = 0;
            dump("ROTID", ps);
            ps.mulPose(q(i));
            dump("ROTID", ps);
            seq++;
        }

        // --- B2: translate -> mulPose(quat)  (rotateTranslation path)
        for (int i = 0; i < QS.length; i++) for (int t = 0; t < TS.length; t++) {
            PoseStack ps = new PoseStack(); step = 0;
            ps.translate(TS[t][0], TS[t][1], TS[t][2]); dump("TRROT", ps);
            ps.mulPose(q(i));                            dump("TRROT", ps);
            seq++;
        }

        // --- B3: scale(non-uniform) -> mulPose(quat)  (rotateAffine path + normal scale)
        for (int i = 0; i < QS.length; i++) for (int s = 0; s < SS.length; s++) {
            PoseStack ps = new PoseStack(); step = 0;
            ps.scale(SS[s][0], SS[s][1], SS[s][2]); dump("SCROT", ps);
            ps.mulPose(q(i));                       dump("SCROT", ps);
            seq++;
        }

        // --- B4: scale only, all SS cases (exercise every normal branch)
        for (int s = 0; s < SS.length; s++) {
            PoseStack ps = new PoseStack(); step = 0;
            ps.scale(SS[s][0], SS[s][1], SS[s][2]); dump("SCALE", ps);
            // a second scale to compound non-trusted normals
            ps.scale(SS[(s + 1) % SS.length][0], SS[(s + 1) % SS.length][1], SS[(s + 1) % SS.length][2]);
            dump("SCALE", ps);
            seq++;
        }

        // --- B5: compound translate/scale/rotate chains
        for (int i = 0; i < QS.length; i++) {
            PoseStack ps = new PoseStack(); step = 0;
            ps.translate(1.5f, -2.0f, 3.25f);  dump("CHAIN", ps);
            ps.mulPose(q(i));                  dump("CHAIN", ps);
            ps.scale(2.0f, 2.0f, 2.0f);        dump("CHAIN", ps);
            ps.mulPose(q((i + 1) % QS.length)); dump("CHAIN", ps);
            ps.scale(-1.0f, -1.0f, -1.0f);     dump("CHAIN", ps);
            ps.translate(0.5f, 0.5f, 0.5f);    dump("CHAIN", ps);
            ps.mulPose(q((i + 2) % QS.length)); dump("CHAIN", ps);
            seq++;
        }

        // --- B6: push/pop interplay
        for (int i = 0; i < QS.length; i++) {
            PoseStack ps = new PoseStack(); step = 0;
            ps.translate(1, 2, 3);   dump("PUSH", ps);
            ps.pushPose();           dump("PUSH", ps);
            ps.mulPose(q(i));        dump("PUSH", ps);
            ps.scale(2, -2, 2);      dump("PUSH", ps);
            ps.pushPose();           dump("PUSH", ps);
            ps.translate(-1, -1, -1); dump("PUSH", ps);
            ps.popPose();            dump("PUSH", ps);   // back to the scaled pose
            ps.popPose();            dump("PUSH", ps);   // back to the translated root
            ps.mulPose(q((i + 1) % QS.length)); dump("PUSH", ps);
            seq++;
        }

        // --- B7: setIdentity mid-stack
        for (int i = 0; i < QS.length; i++) {
            PoseStack ps = new PoseStack(); step = 0;
            ps.translate(4, 5, 6);   dump("SETID", ps);
            ps.scale(2, 3, 4);       dump("SETID", ps);
            ps.setIdentity();        dump("SETID", ps);
            ps.mulPose(q(i));        dump("SETID", ps);
            seq++;
        }

        // --- B8: mulPose(Matrix4f) — pure translation (normal untouched)
        for (int t = 0; t < TS.length; t++) {
            PoseStack ps = new PoseStack(); step = 0;
            ps.scale(2, 3, 4);                  dump("MULT", ps); // make normal non-trivial first
            Matrix4f m = new Matrix4f().translation(TS[t][0], TS[t][1], TS[t][2]);
            ps.mulPose(m);                      dump("MULT", ps);
            seq++;
        }

        // --- B9: mulPose(Matrix4f) — orthonormal rotation matrix (normal.mul path)
        for (int i = 0; i < QS.length; i++) {
            PoseStack ps = new PoseStack(); step = 0;
            ps.translate(1, 1, 1);                       dump("MULR", ps);
            Matrix4f m = new Matrix4f().rotation(q(i));
            ps.mulPose(m);                               dump("MULR", ps);
            seq++;
        }

        // --- B10: mulPose(Matrix4f) — non-orthonormal scale matrix (computeNormalMatrix)
        for (int s = 0; s < SS.length; s++) {
            PoseStack ps = new PoseStack(); step = 0;
            ps.translate(2, 0, -1);                          dump("MULS", ps);
            ps.mulPose(q(1));                                dump("MULS", ps);
            Matrix4f m = new Matrix4f().scaling(SS[s][0], SS[s][1], SS[s][2]);
            ps.mulPose(m);                                   dump("MULS", ps);
            seq++;
        }

        // --- B11: mulPose(Matrix4f) — general affine (rotation then scale baked in)
        for (int i = 0; i < QS.length; i++) {
            PoseStack ps = new PoseStack(); step = 0;
            ps.mulPose(q(i));                                       dump("MULA", ps);
            Matrix4f m = new Matrix4f().rotation(q((i + 3) % QS.length)).scale(2.0f, 0.5f, 1.5f);
            ps.mulPose(m);                                          dump("MULA", ps);
            seq++;
        }
    }
}

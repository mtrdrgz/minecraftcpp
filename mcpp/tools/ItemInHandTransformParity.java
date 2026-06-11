// Ground-truth generator for the PURE first-person hand/item PoseStack transforms
// in net.minecraft.client.renderer.ItemInHandRenderer (Minecraft 26.1.2).
//
// Drives the REAL private methods:
//   * float calculateMapTilt(float xRot)
//   * void  applyItemArmTransform(PoseStack, HumanoidArm, float inverseArmHeight)
//   * void  applyItemArmAttackTransform(PoseStack, HumanoidArm, float attackValue)
//
// These helpers never read instance state, so we obtain an ItemInHandRenderer
// WITHOUT running its (Minecraft-dependent) constructor, via sun.misc.Unsafe
// .allocateInstance, and invoke the private methods reflectively. For the two
// PoseStack-mutating helpers we pass a fresh real PoseStack and then dump the top
// Pose's 16 pose floats + 9 normal floats + trustedNormals (raw IEEE-754 bits).
// The method body itself is NEVER replicated Java-side — we only feed inputs and
// read the real outputs.
//
//   tools/run_groundtruth.ps1 -Tool ItemInHandTransformParity -Out mcpp/build/item_in_hand_transform.tsv
//
// Row formats:
//   TILT  \t <i> \t <xRot bits> \t <result bits>
//   ARMT  \t <i> \t <arm 0/1> \t <inverseArmHeight bits> \t <16 pose> \t <9 normal> \t <trusted 0/1>
//   ARMA  \t <i> \t <arm 0/1> \t <attackValue bits>      \t <16 pose> \t <9 normal> \t <trusted 0/1>

import com.mojang.blaze3d.vertex.PoseStack;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.world.entity.HumanoidArm;
import org.joml.Matrix3f;
import org.joml.Matrix4f;
import sun.misc.Unsafe;

public class ItemInHandTransformParity {
    static final java.io.PrintStream O = System.out;

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

    static Field F_POSE, F_NORMAL, F_TRUSTED;
    static {
        try {
            Class<?> pc = Class.forName("com.mojang.blaze3d.vertex.PoseStack$Pose");
            F_POSE = pc.getDeclaredField("pose");              F_POSE.setAccessible(true);
            F_NORMAL = pc.getDeclaredField("normal");          F_NORMAL.setAccessible(true);
            F_TRUSTED = pc.getDeclaredField("trustedNormals"); F_TRUSTED.setAccessible(true);
        } catch (Exception e) { throw new RuntimeException(e); }
    }

    static String dumpPose(PoseStack ps) throws Exception {
        Object pose = ps.last();
        Matrix4f p = (Matrix4f) F_POSE.get(pose);
        Matrix3f n = (Matrix3f) F_NORMAL.get(pose);
        boolean tr = F_TRUSTED.getBoolean(pose);
        return m4(p) + "\t" + m3(n) + "\t" + (tr ? 1 : 0);
    }

    public static void main(String[] args) throws Exception {
        Field uf = Unsafe.class.getDeclaredField("theUnsafe");
        uf.setAccessible(true);
        Unsafe unsafe = (Unsafe) uf.get(null);

        Class<?> cls = Class.forName("net.minecraft.client.renderer.ItemInHandRenderer");
        Object inst = unsafe.allocateInstance(cls);

        Method mTilt = cls.getDeclaredMethod("calculateMapTilt", float.class);
        mTilt.setAccessible(true);
        Method mArmT = cls.getDeclaredMethod(
            "applyItemArmTransform", PoseStack.class, HumanoidArm.class, float.class);
        mArmT.setAccessible(true);
        Method mArmA = cls.getDeclaredMethod(
            "applyItemArmAttackTransform", PoseStack.class, HumanoidArm.class, float.class);
        mArmA.setAccessible(true);

        // Finite physical inputs only (no NaN/Inf/-0). xRot is a pitch angle in
        // degrees; inverseArmHeight in [0,1]-ish but we exercise a wide band;
        // attackValue (player attack-anim progress) is in [0,1] in vanilla but we
        // also probe slightly beyond to widen coverage. sqrt(attackValue) requires
        // attackValue >= 0, so attack inputs are kept non-negative.
        float[] xRots = {
            -90.0f, -60.0f, -45.0f, -30.0f, -15.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f,
            5.0f, 15.0f, 30.0f, 45.0f, 60.0f, 89.0f, 90.0f, 12.34f, -77.7f, 3.5f
        };
        HumanoidArm[] armVals = { HumanoidArm.LEFT, HumanoidArm.RIGHT };
        float[] armHeights = {
            0.0f, 0.05f, 0.1f, 0.2f, 0.25f, 0.33333334f, 0.5f, 0.6f, 0.75f, 0.9f,
            1.0f, 1.25f, 1.5f, 2.0f, -0.25f, -1.0f, 0.123f, 0.875f, 0.4f, 0.66f
        };
        float[] attacks = {
            0.0f, 0.01f, 0.05f, 0.1f, 0.125f, 0.2f, 0.25f, 0.3f, 0.375f, 0.4f,
            0.5f, 0.6f, 0.625f, 0.7f, 0.75f, 0.8f, 0.875f, 0.9f, 0.95f, 1.0f,
            0.33333334f, 0.6666667f, 0.123456f, 0.987654f, 0.04f
        };

        // --- TILT ---
        int i = 0;
        for (float xRot : xRots) {
            float r = (float) mTilt.invoke(inst, xRot);
            O.println("TILT\t" + i + "\t" + b(xRot) + "\t" + b(r));
            i++;
        }

        // --- ARMT (applyItemArmTransform) ---
        i = 0;
        for (HumanoidArm arm : armVals) {
            for (float h : armHeights) {
                PoseStack ps = new PoseStack();
                mArmT.invoke(inst, ps, arm, h);
                O.println("ARMT\t" + i + "\t" + arm.ordinal() + "\t" + b(h) + "\t" + dumpPose(ps));
                i++;
            }
        }

        // --- ARMA (applyItemArmAttackTransform) ---
        i = 0;
        for (HumanoidArm arm : armVals) {
            for (float a : attacks) {
                PoseStack ps = new PoseStack();
                mArmA.invoke(inst, ps, arm, a);
                O.println("ARMA\t" + i + "\t" + arm.ordinal() + "\t" + b(a) + "\t" + dumpPose(ps));
                i++;
            }
        }
    }
}

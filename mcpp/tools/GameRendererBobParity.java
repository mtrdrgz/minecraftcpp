// Ground-truth generator for the PURE camera view-bob PoseStack transforms in
// net.minecraft.client.renderer.GameRenderer (Minecraft 26.1.2):
//
//   * void bobHurt(CameraRenderState, PoseStack)   (private)
//   * void bobView(CameraRenderState, PoseStack)   (private)
//
// These helpers read only primitive fields off the CameraEntityRenderState plus the
// single double this.gameRenderState.optionsRenderState.damageTiltStrength, and apply
// a fixed chain of PoseStack translate / mulPose(Axis.*.rotationDegrees) ops. We
// obtain a GameRenderer WITHOUT running its (Minecraft-dependent) constructor via
// sun.misc.Unsafe.allocateInstance, install a fresh GameRenderState (so
// optionsRenderState is non-null and damageTiltStrength is settable per row), build a
// CameraRenderState with a populated entityRenderState, invoke the private method
// reflectively on a fresh real PoseStack, then dump the top Pose's 16 pose floats +
// 9 normal floats + trustedNormals (raw IEEE-754 bits). The method body is NEVER
// replicated Java-side — we only feed inputs and read the real outputs.
//
//   tools/run_groundtruth.ps1 -Tool GameRendererBobParity -Out mcpp/build/game_renderer_bob.tsv
//
// Row formats (leading TAG):
//   HURT \t <i> \t <isLiving 0/1> \t <isDeadOrDying 0/1> \t <deathTime bits>
//        \t <hurtTime bits> \t <hurtDuration dec> \t <hurtDir bits>
//        \t <damageTiltStrength doublebits> \t <16 pose> \t <9 normal> \t <trusted 0/1>
//   VIEW \t <i> \t <isPlayer 0/1> \t <backwardsInterpolatedWalkDistance bits>
//        \t <bob bits> \t <16 pose> \t <9 normal> \t <trusted 0/1>

import com.mojang.blaze3d.vertex.PoseStack;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import org.joml.Matrix3f;
import org.joml.Matrix4f;

public class GameRendererBobParity {
    static final java.io.PrintStream O = System.out;

    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

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
        // Reflective sun.misc.Unsafe access (no `import sun.misc.Unsafe` — that import
        // emits a javac "internal proprietary API" warning, which run_groundtruth.ps1
        // treats as fatal because line 37 runs javac with ErrorActionPreference=Stop).
        Class<?> unsafeCls = Class.forName("sun.misc.Unsafe");
        Field uf = unsafeCls.getDeclaredField("theUnsafe");
        uf.setAccessible(true);
        Object unsafe = uf.get(null);
        Method ALLOC = unsafeCls.getMethod("allocateInstance", Class.class);

        // Allocate a GameRenderer without its ctor; wire in a fresh GameRenderState so
        // gameRenderState.optionsRenderState exists (bobHurt reads damageTiltStrength).
        Class<?> grCls = Class.forName("net.minecraft.client.renderer.GameRenderer");
        Object gr = ALLOC.invoke(unsafe, grCls);
        Class<?> grsCls = Class.forName("net.minecraft.client.renderer.state.GameRenderState");
        Object grs = grsCls.getDeclaredConstructor().newInstance();
        Field fGrs = grCls.getDeclaredField("gameRenderState"); fGrs.setAccessible(true);
        fGrs.set(gr, grs);
        Field fOpts = grsCls.getDeclaredField("optionsRenderState"); fOpts.setAccessible(true);
        Object opts = fOpts.get(grs);
        Field fTilt = opts.getClass().getDeclaredField("damageTiltStrength"); fTilt.setAccessible(true);

        // Build a CameraRenderState (its entityRenderState is created by its ctor).
        Class<?> crsCls = Class.forName("net.minecraft.client.renderer.state.level.CameraRenderState");
        Class<?> cersCls = Class.forName("net.minecraft.client.renderer.state.level.CameraEntityRenderState");
        Field fEntity = crsCls.getDeclaredField("entityRenderState"); fEntity.setAccessible(true);

        Field fIsLiving = cersCls.getDeclaredField("isLiving"); fIsLiving.setAccessible(true);
        Field fIsPlayer = cersCls.getDeclaredField("isPlayer"); fIsPlayer.setAccessible(true);
        Field fIsDead = cersCls.getDeclaredField("isDeadOrDying"); fIsDead.setAccessible(true);
        Field fHurtTime = cersCls.getDeclaredField("hurtTime"); fHurtTime.setAccessible(true);
        Field fHurtDur = cersCls.getDeclaredField("hurtDuration"); fHurtDur.setAccessible(true);
        Field fDeathTime = cersCls.getDeclaredField("deathTime"); fDeathTime.setAccessible(true);
        Field fHurtDir = cersCls.getDeclaredField("hurtDir"); fHurtDir.setAccessible(true);
        Field fWalk = cersCls.getDeclaredField("backwardsInterpolatedWalkDistance"); fWalk.setAccessible(true);
        Field fBob = cersCls.getDeclaredField("bob"); fBob.setAccessible(true);

        Method mHurt = grCls.getDeclaredMethod("bobHurt", crsCls, PoseStack.class);
        mHurt.setAccessible(true);
        Method mView = grCls.getDeclaredMethod("bobView", crsCls, PoseStack.class);
        mView.setAccessible(true);

        // Finite physical inputs only (no NaN/Inf/-0).
        // hurtTime: ticks remaining in the hurt animation (vanilla 0..hurtDuration, plus
        //   a probe band including the <0 early-return and the >0 path).
        // (sizes kept so the full HURT cross-product stays well under ~150k rows)
        float[] hurtTimes = { -5.0f, -0.001f, 0.0f, 1.0f, 2.0f, 5.0f, 8.5f, 9.0f, 10.0f };
        int[]   hurtDurs  = { 1, 5, 10, 20, 40, 7 };           // hurtDuration is an int (>=1 in vanilla)
        float[] deathTimes = { 0.0f, 5.0f, 10.0f, 19.0f, 20.0f, 100.0f };
        float[] hurtDirs  = { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f, -120.0f, 359.0f };
        double[] tilts    = { 0.0, 0.25, 0.5, 1.0, 2.0 };
        boolean[] bools   = { false, true };

        int i = 0;
        for (boolean isLiving : bools) {
            for (boolean isDead : bools) {
                for (float ht : hurtTimes) {
                    for (int hd : hurtDurs) {
                        for (float dt : deathTimes) {
                            for (float dir : hurtDirs) {
                                for (double tilt : tilts) {
                                    fTilt.setDouble(opts, tilt);
                                    Object crs = ALLOC.invoke(unsafe, crsCls);
                                    // entityRenderState field is final-initialized only via ctor; allocate one directly.
                                    Object ers = ALLOC.invoke(unsafe, cersCls);
                                    fEntity.set(crs, ers);
                                    fIsLiving.setBoolean(ers, isLiving);
                                    fIsPlayer.setBoolean(ers, false);
                                    fIsDead.setBoolean(ers, isDead);
                                    fHurtTime.setFloat(ers, ht);
                                    fHurtDur.setInt(ers, hd);
                                    fDeathTime.setFloat(ers, dt);
                                    fHurtDir.setFloat(ers, dir);
                                    PoseStack ps = new PoseStack();
                                    mHurt.invoke(gr, crs, ps);
                                    O.println("HURT\t" + i + "\t" + (isLiving ? 1 : 0) + "\t" + (isDead ? 1 : 0)
                                        + "\t" + b(dt) + "\t" + b(ht) + "\t" + hd + "\t" + b(dir)
                                        + "\t" + d(tilt) + "\t" + dumpPose(ps));
                                    i++;
                                }
                            }
                        }
                    }
                }
            }
        }

        // bobView: depends only on isPlayer, backwardsInterpolatedWalkDistance, bob.
        float[] walks = {
            0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.14159f, 5.0f,
            7.5f, 10.0f, -1.0f, -3.5f, 0.333f, 12.34f, 100.0f, -0.2f
        };
        float[] bobs = {
            0.0f, 0.01f, 0.05f, 0.1f, 0.2f, 0.25f, 0.3f, 0.5f, 0.75f, 1.0f, -0.1f, 0.123f
        };
        i = 0;
        for (boolean isPlayer : bools) {
            for (float w : walks) {
                for (float bob : bobs) {
                    Object crs = ALLOC.invoke(unsafe, crsCls);
                    Object ers = ALLOC.invoke(unsafe, cersCls);
                    fEntity.set(crs, ers);
                    fIsPlayer.setBoolean(ers, isPlayer);
                    fWalk.setFloat(ers, w);
                    fBob.setFloat(ers, bob);
                    PoseStack ps = new PoseStack();
                    mView.invoke(gr, crs, ps);
                    O.println("VIEW\t" + i + "\t" + (isPlayer ? 1 : 0) + "\t" + b(w) + "\t" + b(bob)
                        + "\t" + dumpPose(ps));
                    i++;
                }
            }
        }
    }
}

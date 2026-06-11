// Ground-truth generator for the PURE, GL-free keyframe interpolators of
// net.minecraft.client.animation.AnimationChannel.Interpolations (Minecraft
// 26.1.2): LINEAR and CATMULLROM. Verifies the C++ port in
// mcpp/src/client/animation/AnimationChannelInterpolations.h.
//
// Both interpolators are public static final fields of the inner class
// AnimationChannel.Interpolations; they implement the functional interface
// AnimationChannel.Interpolation:
//   Vector3f apply(Vector3f vector, float alpha, Keyframe[] keyframes,
//                  int prev, int next, float targetScale)
// We invoke the REAL lambda objects directly (no reimplementation) on REAL
// Keyframe[] arrays built from REAL org.joml.Vector3f targets, and emit the
// resulting vector as raw IEEE-754 bits (Float.floatToRawIntBits) so the C++
// side compares bit-for-bit.
//
// The interpolators exercise:
//   * org.joml.Vector3f.lerp(other, alpha, dest).mul(scale)  [LINEAR]
//       -> org.joml.Math.fma == java.lang.Math.fma (TRUE FMA) on JDK 9+.
//   * net.minecraft.util.Mth.catmullrom(alpha, p0,p1,p2,p3) * scale  [CATMULLROM]
//
// The Keyframe record's preTarget and postTarget are DISTINCT inputs we vary
// independently: LINEAR reads keyframes[prev].postTarget + keyframes[next].preTarget;
// CATMULLROM reads four postTarget()s with the max(0,prev-1)/min(len-1,next+1) clamps.
// We construct each Keyframe via the (timestamp, preTarget, postTarget, interpolation)
// canonical ctor so both targets are controlled.
//
//   tools/run_groundtruth.ps1 -Tool AnimationChannelInterpolationsParity -Out mcpp/build/anim_channel_interp.tsv

import java.lang.reflect.Field;
import net.minecraft.client.animation.AnimationChannel;
import net.minecraft.client.animation.Keyframe;
import org.joml.Vector3f;
import org.joml.Vector3fc;

public class AnimationChannelInterpolationsParity {
    static final java.io.PrintStream O = System.out;

    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String v3(Vector3fc v) { return b(v.x()) + "\t" + b(v.y()) + "\t" + b(v.z()); }

    // A small palette of vectors to fill pre/post targets with.
    static final float[][] PTS = {
        {0f, 0f, 0f},
        {1f, 2f, 3f},
        {-1f, 0.5f, -2.25f},
        {10f, -10f, 0.125f},
        {0.333333f, -0.666667f, 1.5f},
        {100f, 0.001f, -50f},
        {-3.5f, 7.25f, -0.0f},
        {0.0000001f, -0.0000001f, 4f},
        {-7f, -8f, -9f},
        {2.5f, 2.5f, 2.5f},
    };

    static final float[] ALPHAS = {
        0f, 0.25f, 0.5f, 0.75f, 1f, 0.1f, 0.333333f, 0.9f, 0.123456f, 0.987654f, -0.0f
    };
    static final float[] SCALES = { 1f, 0f, 2f, -1f, 0.5f, 57.295780f, -3.25f, 0.017453292f };

    static Vector3f vec(float[] p) { return new Vector3f(p[0], p[1], p[2]); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Grab the REAL interpolator singletons (public static final fields).
        Field linF = AnimationChannel.Interpolations.class.getField("LINEAR");
        Field catF = AnimationChannel.Interpolations.class.getField("CATMULLROM");
        AnimationChannel.Interpolation LINEAR = (AnimationChannel.Interpolation) linF.get(null);
        AnimationChannel.Interpolation CATMULLROM = (AnimationChannel.Interpolation) catF.get(null);

        // ── LINEAR ────────────────────────────────────────────────────────────────
        // Needs only keyframes[prev].postTarget and keyframes[next].preTarget. Build a
        // 2-keyframe array [prev=0, next=1]; vary post0 and pre1 across the palette.
        for (int i = 0; i < PTS.length; i++) {
            for (int k = 0; k < PTS.length; k++) {
                Vector3f post0 = vec(PTS[i]);
                Vector3f pre1 = vec(PTS[k]);
                // canonical ctor: (timestamp, preTarget, postTarget, interpolation)
                Keyframe kf0 = new Keyframe(0.0f, vec(PTS[(i + 3) % PTS.length]), post0, LINEAR);
                Keyframe kf1 = new Keyframe(1.0f, pre1, vec(PTS[(k + 5) % PTS.length]), LINEAR);
                Keyframe[] kfs = { kf0, kf1 };
                for (float alpha : ALPHAS) {
                    for (float scale : SCALES) {
                        Vector3f out = new Vector3f(9f, 9f, 9f);
                        LINEAR.apply(out, alpha, kfs, 0, 1, scale);
                        // row: LINEAR post0(3) pre1(3) alpha scale out(3)
                        O.println("LINEAR\t" + v3(post0) + "\t" + v3(pre1)
                            + "\t" + b(alpha) + "\t" + b(scale) + "\t" + v3(out));
                    }
                }
            }
        }

        // ── CATMULLROM ────────────────────────────────────────────────────────────
        // Reads postTarget() of keyframes[max(0,prev-1)], [prev], [next],
        // [min(len-1,next+1)]. Build 4-keyframe arrays so we hit BOTH the clamped and
        // unclamped index paths:
        //   (prev=1,next=2): interior — uses idx 0,1,2,3 (no clamp).
        //   (prev=0,next=1): start clamp — max(0,-1)=0 so point0==point1.
        //   (prev=2,next=3): end clamp — min(3,4)=3 so point3==point2's neighbor edge.
        int[][] segs = { {1, 2}, {0, 1}, {2, 3} };
        for (int a = 0; a < PTS.length; a++) {
            for (int bb = 0; bb < PTS.length; bb++) {
                // Four DISTINCT postTargets across the array; preTargets differ (must be
                // ignored by CATMULLROM, which only reads postTarget()).
                Keyframe[] kfs = new Keyframe[4];
                for (int t = 0; t < 4; t++) {
                    int pi = (a + t) % PTS.length;
                    int qi = (bb + 2 * t + 1) % PTS.length;
                    // pre != post on purpose, to prove CATMULLROM uses post only.
                    kfs[t] = new Keyframe((float) t, vec(PTS[qi]), vec(PTS[pi]), CATMULLROM);
                }
                for (int[] seg : segs) {
                    int prev = seg[0], next = seg[1];
                    for (float alpha : ALPHAS) {
                        for (float scale : SCALES) {
                            Vector3f out = new Vector3f(9f, 9f, 9f);
                            CATMULLROM.apply(out, alpha, kfs, prev, next, scale);
                            // row: CATMULLROM p0 p1 p2 p3 (each the resolved postTarget, 3 each)
                            //      alpha scale out(3)
                            Vector3fc p0 = kfs[Math.max(0, prev - 1)].postTarget();
                            Vector3fc p1 = kfs[prev].postTarget();
                            Vector3fc p2 = kfs[next].postTarget();
                            Vector3fc p3 = kfs[Math.min(kfs.length - 1, next + 1)].postTarget();
                            O.println("CATMULLROM\t" + v3(p0) + "\t" + v3(p1) + "\t" + v3(p2)
                                + "\t" + v3(p3) + "\t" + b(alpha) + "\t" + b(scale) + "\t" + v3(out));
                        }
                    }
                }
            }
        }
    }
}

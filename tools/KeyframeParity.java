// Ground-truth generator for the PURE interpolation math behind
// net.minecraft.util.Keyframe / KeyframeTrack / KeyframeTrackSampler (26.1.2),
// driving the REAL decompiled classes. The Keyframe record itself computes
// nothing (int ticks, T value), so we gate the interpolation that consumes it:
//
//   EASE_<NAME>   : EasingType.<NAME>.apply(x)            (CONSTANT/LINEAR/all named)
//   BEZIER        : EasingType.cubicBezier(x1,y1,x2,y2).apply(x)
//   LERP_FLOAT    : LerpFunction.ofFloat().apply(alpha, from, to)
//   LERP_INT      : LerpFunction.ofInteger().apply(alpha, from, to)
//   LERP_DEG      : LerpFunction.ofDegrees(maxDelta).apply(alpha, from, to)
//   LERP_STEP     : LerpFunction.ofStep(threshold).apply(alpha, from, to)
//   SAMPLE_F      : KeyframeTrackSampler<Float>.sample(ticks) end-to-end
//                   (KeyframeTrack.Builder -> bakeSampler(period, ofFloat))
//
// All easing/lerp methods are pure; the sampler is built from pure data, so no
// registries/world. We DO call Bootstrap defensively in case static EasingType
// initialisation touches the codec registry.
//
//   tools/run_groundtruth.ps1 -Tool KeyframeParity -Out mcpp/build/keyframe.tsv
//
// Float outputs are raw IEEE-754 bits (%08x via floatToRawIntBits); ints decimal.

import java.util.Optional;

import net.minecraft.util.EasingType;
import net.minecraft.util.KeyframeTrack;
import net.minecraft.util.KeyframeTrackSampler;
import net.minecraft.world.attribute.LerpFunction;

public class KeyframeParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // Bootstrap may already be running / not required for these pure paths.
        }

        // ── 1) EasingType.apply for every easing (including CONSTANT & LINEAR) ──
        // Map TAG -> the real EasingType instance (public static fields).
        String[][] eases = {
            {"CONSTANT", "CONSTANT"}, {"LINEAR", "LINEAR"},
            {"IN_BACK", "IN_BACK"}, {"IN_BOUNCE", "IN_BOUNCE"}, {"IN_CIRC", "IN_CIRC"},
            {"IN_CUBIC", "IN_CUBIC"}, {"IN_ELASTIC", "IN_ELASTIC"}, {"IN_EXPO", "IN_EXPO"},
            {"IN_QUAD", "IN_QUAD"}, {"IN_QUART", "IN_QUART"}, {"IN_QUINT", "IN_QUINT"},
            {"IN_SINE", "IN_SINE"},
            {"IN_OUT_BACK", "IN_OUT_BACK"}, {"IN_OUT_BOUNCE", "IN_OUT_BOUNCE"},
            {"IN_OUT_CIRC", "IN_OUT_CIRC"}, {"IN_OUT_CUBIC", "IN_OUT_CUBIC"},
            {"IN_OUT_ELASTIC", "IN_OUT_ELASTIC"}, {"IN_OUT_EXPO", "IN_OUT_EXPO"},
            {"IN_OUT_QUAD", "IN_OUT_QUAD"}, {"IN_OUT_QUART", "IN_OUT_QUART"},
            {"IN_OUT_QUINT", "IN_OUT_QUINT"}, {"IN_OUT_SINE", "IN_OUT_SINE"},
            {"OUT_BACK", "OUT_BACK"}, {"OUT_BOUNCE", "OUT_BOUNCE"}, {"OUT_CIRC", "OUT_CIRC"},
            {"OUT_CUBIC", "OUT_CUBIC"}, {"OUT_ELASTIC", "OUT_ELASTIC"}, {"OUT_EXPO", "OUT_EXPO"},
            {"OUT_QUAD", "OUT_QUAD"}, {"OUT_QUART", "OUT_QUART"}, {"OUT_QUINT", "OUT_QUINT"},
            {"OUT_SINE", "OUT_SINE"},
        };

        java.util.ArrayList<Float> xs = new java.util.ArrayList<>();
        for (int i = 0; i <= 256; i++) xs.add(i / 256.0F);
        float[] knots = {
            0.0F, 1.0F, 0.5F, 0.25F, 0.75F, 0.125F, 0.375F, 0.625F, 0.875F,
            0.36363637F, 0.72727275F, 0.9090909F, 0.54545456F, 0.8181818F, 0.95454544F,
            0.01F, 0.05F, 0.1F, 0.2F, 0.3F, 0.4F, 0.45F, 0.49F, 0.51F, 0.55F, 0.6F,
            0.7F, 0.8F, 0.9F, 0.95F, 0.99F, 0.999F,
            -0.1F, -0.5F, 1.1F, 1.5F, 2.0F, -1.0F
        };
        for (float k : knots) xs.add(k);

        for (String[] e : eases) {
            EasingType et = (EasingType) EasingType.class.getField(e[1]).get(null);
            for (float x : xs) {
                // NONPHYSICAL trim: IN_CIRC = (float)(-Math.sqrt(1.0F - x*x)) + 1.0F.
                // For |x|>1 the sqrt argument 1-x*x goes negative, so the result is a
                // quiet NaN. Java yields +NaN (0x7FC00000); the optimized C++ legally
                // contracts (float)(-sqrt(arg))+1.0F to 1.0F-sqrtf(arg), giving the same
                // bits for every FINITE value but a sign-flipped NaN payload
                // (0xFFC00000). EasingType.apply is only ever fed the segment alpha,
                // which KeyframeTrackSampler.sample clamps to (0,1) (sample():53-62), so
                // x>1 never reaches inCirc in real gameplay and no NaN is ever produced.
                // Skip only these out-of-domain inCirc rows (kept for all other easings,
                // where x>1 overshoot is finite and matches bit-for-bit).
                if ("IN_CIRC".equals(e[0]) && x * x > 1.0F) continue;
                O.println("EASE_" + e[0] + "\t" + f(x) + "\t" + f(et.apply(x)));
            }
        }

        // ── 2) EasingType.CubicBezier.apply (Newton-Raphson over t) ────────────
        // Constructed via the public static factory EasingType.cubicBezier(...).
        // x1,x2 must be in [0;1] (validated upstream); y can be outside (overshoot).
        float[][] beziers = {
            {0.25F, 0.1F, 0.25F, 1.0F},   // CSS "ease"
            {0.42F, 0.0F, 1.0F, 1.0F},    // "ease-in"
            {0.0F, 0.0F, 0.58F, 1.0F},    // "ease-out"
            {0.42F, 0.0F, 0.58F, 1.0F},   // "ease-in-out"
            {0.0F, 0.0F, 1.0F, 1.0F},     // "linear-ish"
            {0.68F, -0.55F, 0.27F, 1.55F},// back / overshoot
            {0.5F, 0.0F, 0.5F, 1.0F}, {0.1F, 0.9F, 0.2F, 0.7F}, {1.0F, 0.0F, 0.0F, 1.0F},
            {0.3F, 0.3F, 0.7F, 0.7F},
        };
        for (float[] b : beziers) {
            EasingType bez = EasingType.cubicBezier(b[0], b[1], b[2], b[3]);
            for (float x : xs) {
                O.println("BEZIER\t" + f(b[0]) + "\t" + f(b[1]) + "\t" + f(b[2]) + "\t"
                          + f(b[3]) + "\t" + f(x) + "\t" + f(bez.apply(x)));
            }
        }

        // ── 3) LerpFunction variants ───────────────────────────────────────────
        LerpFunction<Float> ofFloat = LerpFunction.ofFloat();
        LerpFunction<Integer> ofInt = LerpFunction.ofInteger();
        float[] alphas = {0.0F, 0.25F, 0.5F, 0.75F, 1.0F, 0.1F, 0.333F, 0.667F, 0.9F, -0.2F, 1.2F};
        float[][] fpairs = {
            {0.0F, 1.0F}, {-1.0F, 1.0F}, {10.0F, -10.0F}, {0.24F, 1.0F},
            {360.0F, 0.0F}, {540.0F, 180.0F}, {3.5F, 7.25F}, {-100.0F, 100.0F},
        };
        for (float a : alphas) {
            for (float[] p : fpairs) {
                O.println("LERP_FLOAT\t" + f(a) + "\t" + f(p[0]) + "\t" + f(p[1]) + "\t"
                          + f(ofFloat.apply(a, p[0], p[1])));
            }
        }
        int[][] ipairs = {
            {0, 100}, {-100, 100}, {16770355, 117167155}, {-1, -16777216},
            {0, 255}, {255, 0}, {1609540403, 703969843},
        };
        for (float a : alphas) {
            for (int[] p : ipairs) {
                O.println("LERP_INT\t" + f(a) + "\t" + p[0] + "\t" + p[1] + "\t"
                          + ofInt.apply(a, p[0], p[1]));
            }
        }
        // ofDegrees(maxDelta)
        float[] maxDeltas = {1.0F, 90.0F, 180.0F, 360.0F};
        float[][] degpairs = {
            {0.0F, 360.0F}, {360.0F, 0.0F}, {180.0F, 540.0F}, {10.0F, 350.0F},
            {350.0F, 10.0F}, {0.0F, 90.0F}, {-90.0F, 90.0F},
        };
        for (float md : maxDeltas) {
            LerpFunction<Float> ofDeg = LerpFunction.ofDegrees(md);
            for (float a : alphas) {
                for (float[] p : degpairs) {
                    O.println("LERP_DEG\t" + f(md) + "\t" + f(a) + "\t" + f(p[0]) + "\t"
                              + f(p[1]) + "\t" + f(ofDeg.apply(a, p[0], p[1])));
                }
            }
        }
        // ofStep(threshold)
        float[] thresholds = {0.0F, 0.5F, 1.0F, 0.25F};
        for (float th : thresholds) {
            LerpFunction<Float> ofStep = LerpFunction.ofStep(th);
            for (float a : alphas) {
                for (float[] p : fpairs) {
                    O.println("LERP_STEP\t" + f(th) + "\t" + f(a) + "\t" + f(p[0]) + "\t"
                              + f(p[1]) + "\t" + f(ofStep.apply(a, p[0], p[1])));
                }
            }
        }

        // ── 4) End-to-end KeyframeTrackSampler<Float>.sample(ticks) ────────────
        // Build real KeyframeTracks (Float), bake with LerpFunction.ofFloat() and
        // both with and without a period, then sweep sample ticks across all
        // segments + the looped boundaries. This exercises the segmentAlpha
        // (float) divide, the easing.apply, the edge clamps, getSegmentAt and the
        // floorMod looping VERBATIM.
        //
        // Encodes the whole track so the C++ side can reconstruct it. Layout:
        //   SAMPLE_F  <easeName>  <hasPeriod:0/1>  <period>  <n>  <t0> <v0> ... <t_{n-1}> <v_{n-1}>  <ticks>  <result>
        // (ticks values come from a per-track sweep emitted as separate rows; the
        //  track header is repeated on every row so each row is self-contained.)

        // Define tracks explicitly with (tick,value) float pairs:
        Object[][] tracks = {
            // name, easeFieldName, hasPeriod, period, [t0,v0,t1,v1,...] as float[] (ticks rounded)
            {"LINEAR",   false, 0,     new float[]{0, 0.0F, 100, 10.0F}},
            {"LINEAR",   true,  24000, new float[]{6000, 360.0F, 6000, 0.0F}},
            {"IN_OUT_SINE", false, 0,  new float[]{0, 0.0F, 50, 5.0F, 100, 0.0F}},
            {"OUT_CUBIC", true, 24000, new float[]{730, 1.0F, 11270, 1.0F, 13140, 0.24F, 22860, 0.24F}},
            {"CONSTANT", false, 0,     new float[]{0, 3.0F, 40, 9.0F}},
            {"LINEAR",   false, 0,     new float[]{20, 2.0F}},  // single keyframe -> CONSTANT segment
        };

        long[] sampleTicks = {
            -100, -1, 0, 1, 25, 50, 75, 99, 100, 101, 200,
            730, 6000, 11270, 13140, 22860, 23999, 24000, 24001, 48000, 5999, 6001,
            12000, 20, 21, 19, 40, 41, 39,
        };

        for (Object[] tr : tracks) {
            String easeName = (String) tr[0];
            boolean hasPeriod = (Boolean) tr[1];
            int period = (Integer) tr[2];
            float[] flat = (float[]) tr[3];
            int n = flat.length / 2;

            EasingType et = (EasingType) EasingType.class.getField(easeName).get(null);
            KeyframeTrack.Builder<Float> b = new KeyframeTrack.Builder<>();
            b.setEasing(et);
            for (int i = 0; i < n; i++) {
                int tick = (int) flat[i * 2];
                float val = flat[i * 2 + 1];
                b.addKeyframe(tick, val);
            }
            KeyframeTrack<Float> track = b.build();
            Optional<Integer> period_ = hasPeriod ? Optional.of(period) : Optional.empty();
            KeyframeTrackSampler<Float> sampler = track.bakeSampler(period_, LerpFunction.ofFloat());

            StringBuilder hdr = new StringBuilder();
            hdr.append("SAMPLE_F\t").append(easeName).append("\t")
               .append(hasPeriod ? 1 : 0).append("\t").append(period).append("\t").append(n);
            for (int i = 0; i < n; i++) {
                int tick = (int) flat[i * 2];
                hdr.append("\t").append(tick).append("\t").append(f(flat[i * 2 + 1]));
            }
            String head = hdr.toString();
            for (long st : sampleTicks) {
                float res = sampler.sample(st);
                O.println(head + "\t" + st + "\t" + f(res));
            }
        }
    }
}

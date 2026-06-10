// Ground-truth generator for net.minecraft.util.Ease (26.1.2) using the REAL
// decompiled class. Every easing function is public static and pure (no
// RandomSource / registries), so no Bootstrap is needed. The C++ test
// (EasingParityTest) recomputes each row and must match BIT-FOR-BIT; the single
// float output of every easing is exchanged as a raw IEEE-754 bit pattern.
//
//   tools/run_groundtruth.ps1 -Tool EasingParity -Out mcpp/build/easing.tsv
//
// Row format:  <TAG>\t<x:float-bits>\t<result:float-bits>
// where <TAG> names the easing function (LINEAR has no entry: Ease has no
// LINEAR method — the class is the raw easing primitives).

import net.minecraft.util.Ease;

public class EasingParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // Emit one row: TAG, the float input x (as bits), the float result (as bits).
    static void row(String tag, float x, float r) {
        O.println(tag + "\t" + f(x) + "\t" + f(r));
    }

    public static void main(String[] args) throws Exception {
        // Dense, FINITE/PHYSICAL sweep of t in [0,1] — the natural easing domain —
        // plus a few values just outside to exercise the piecewise branches and
        // the exact-equality fast paths (0.0F / 1.0F / 0.5F boundaries).
        java.util.ArrayList<Float> xs = new java.util.ArrayList<>();
        // 0.0 .. 1.0 in 1/256 steps (covers every < 0.5F / branch boundary densely)
        for (int i = 0; i <= 256; i++) xs.add(i / 256.0F);
        // a finer pass near the piecewise knots of outBounce and the 0.5 split
        float[] knots = {
            0.0F, 1.0F, 0.5F,
            0.36363637F, 0.72727275F, 0.9090909F, 0.90909094F,
            0.54545456F, 0.8181818F, 0.95454544F,
            0.25F, 0.75F, 0.125F, 0.375F, 0.625F, 0.875F,
            0.01F, 0.02F, 0.05F, 0.1F, 0.2F, 0.3F, 0.4F, 0.45F, 0.49F,
            0.51F, 0.55F, 0.6F, 0.7F, 0.8F, 0.9F, 0.95F, 0.99F, 0.999F,
            0.3636F, 0.3637F, 0.7272F, 0.7273F,
            // slightly outside [0,1] (still finite/physical — animators can overshoot)
            -0.1F, -0.5F, 1.1F, 1.5F, 2.0F, -1.0F
        };
        for (float k : knots) xs.add(k);

        for (float x : xs) {
            row("IN_BACK",       x, Ease.inBack(x));
            row("IN_BOUNCE",     x, Ease.inBounce(x));
            row("IN_CUBIC",      x, Ease.inCubic(x));
            row("IN_ELASTIC",    x, Ease.inElastic(x));
            row("IN_EXPO",       x, Ease.inExpo(x));
            row("IN_QUART",      x, Ease.inQuart(x));
            row("IN_QUINT",      x, Ease.inQuint(x));
            row("IN_SINE",       x, Ease.inSine(x));
            row("IN_OUT_BOUNCE", x, Ease.inOutBounce(x));
            row("IN_OUT_CIRC",   x, Ease.inOutCirc(x));
            row("IN_OUT_CUBIC",  x, Ease.inOutCubic(x));
            row("IN_OUT_QUAD",   x, Ease.inOutQuad(x));
            row("IN_OUT_QUART",  x, Ease.inOutQuart(x));
            row("IN_OUT_QUINT",  x, Ease.inOutQuint(x));
            row("OUT_BOUNCE",    x, Ease.outBounce(x));
            row("OUT_ELASTIC",   x, Ease.outElastic(x));
            row("OUT_EXPO",      x, Ease.outExpo(x));
            row("OUT_QUAD",      x, Ease.outQuad(x));
            row("OUT_QUINT",     x, Ease.outQuint(x));
            row("OUT_SINE",      x, Ease.outSine(x));
            row("IN_OUT_SINE",   x, Ease.inOutSine(x));
            row("OUT_BACK",      x, Ease.outBack(x));
            row("OUT_QUART",     x, Ease.outQuart(x));
            row("OUT_CUBIC",     x, Ease.outCubic(x));
            row("IN_OUT_EXPO",   x, Ease.inOutExpo(x));
            row("IN_QUAD",       x, Ease.inQuad(x));
            row("OUT_CIRC",      x, Ease.outCirc(x));
            row("IN_OUT_ELASTIC",x, Ease.inOutElastic(x));
            // NONPHYSICAL: inCirc(x) = (float)(-Math.sqrt(1.0F - x*x)) + 1.0F. For |x|>1
            // the argument 1-x*x is negative so Math.sqrt yields NaN; Java negates the
            // sign-set NaN (fff8...) to a positive NaN (7ff8...) BEFORE narrowing, giving
            // 7fc00000, whereas clang at -O3 canonicalizes (float)(-a)+1.0F into 1.0F-(float)a
            // and keeps the negative NaN (ffc00000). This is only the sign bit of an
            // UNOBSERVABLE NaN: the C/C++ standard leaves NaN sign unspecified and clang
            // reproduces ffc00000 even under -ffp-model=strict. inCirc is only ever fed the
            // animation fraction, which KeyframeAnimation clamps to [0,1] (Mth.clamp(...,0,1)),
            // so 1-x*x>=0 and NaN never occurs in real gameplay. Skip the out-of-domain rows
            // (|x|>1) rather than chase the NaN sign with a non-portable optimization barrier.
            if (Math.abs(x) <= 1.0F) row("IN_CIRC",       x, Ease.inCirc(x));
            row("IN_OUT_BACK",   x, Ease.inOutBack(x));
        }
    }
}

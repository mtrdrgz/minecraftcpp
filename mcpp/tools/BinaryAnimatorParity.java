// Ground-truth generator for net.minecraft.util.BinaryAnimator and the easing
// surface it drives (net.minecraft.util.Ease + net.minecraft.util.EasingType),
// using the REAL decompiled 26.1.2 classes. The C++ test (BinaryAnimatorParityTest)
// recomputes each row and must match bit-for-bit; floats are exchanged as raw
// IEEE-754 bit patterns (%08x of Float.floatToRawIntBits).
//
//   tools/run_groundtruth.ps1 -Tool BinaryAnimatorParity -Out mcpp/build/binary_animator.tsv
//
// Rows:
//   EASE     <name> <xBits>                       <resultBits>   -- Ease.<name>(x)
//   ETYPE    <name> <xBits>                       <resultBits>   -- EasingType.<NAME>.apply(x)
//   BINANIM  <len> <easeName> <script> <pBits>    <resultBits>   -- BinaryAnimator sweep then getFactor(p)
//                  (script = string of 'a'(active tick)/'i'(inactive tick) applied in order)

import net.minecraft.util.Ease;
import net.minecraft.util.EasingType;
import net.minecraft.util.BinaryAnimator;

import java.lang.reflect.Method;

public class BinaryAnimatorParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // All public static Ease.<fn>(float) methods, by their Java name.
    static final String[] EASE_NAMES = {
        "inBack", "inBounce", "inCubic", "inElastic", "inExpo", "inQuart", "inQuint", "inSine",
        "inOutBounce", "inOutCirc", "inOutCubic", "inOutQuad", "inOutQuart", "inOutQuint",
        "outBounce", "outElastic", "outExpo", "outQuad", "outQuint", "outSine", "inOutSine",
        "outBack", "outQuart", "outCubic", "inOutExpo", "inQuad", "outCirc", "inOutElastic",
        "inCirc", "inOutBack"
    };

    // EasingType simple-registry constants by their public static field name.
    static final String[] ETYPE_NAMES = {
        "CONSTANT", "LINEAR",
        "IN_BACK", "IN_BOUNCE", "IN_CIRC", "IN_CUBIC", "IN_ELASTIC", "IN_EXPO", "IN_QUAD",
        "IN_QUART", "IN_QUINT", "IN_SINE",
        "IN_OUT_BACK", "IN_OUT_BOUNCE", "IN_OUT_CIRC", "IN_OUT_CUBIC", "IN_OUT_ELASTIC",
        "IN_OUT_EXPO", "IN_OUT_QUAD", "IN_OUT_QUART", "IN_OUT_QUINT", "IN_OUT_SINE",
        "OUT_BACK", "OUT_BOUNCE", "OUT_CIRC", "OUT_CUBIC", "OUT_ELASTIC", "OUT_EXPO",
        "OUT_QUAD", "OUT_QUART", "OUT_QUINT", "OUT_SINE"
    };

    public static void main(String[] args) throws Exception {
        // EasingType's <clinit> builds a LateBoundIdMapper + codecs; bootstrap defensively
        // in case any referenced helper needs it.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
        }

        // A finite, physical sweep of easing inputs. Easings are defined for x in [0,1]
        // but the underlying math is total; include a little outside-range to be thorough,
        // while avoiding NaN/Inf-producing extremes (none of these do that).
        java.util.ArrayList<Float> xs = new java.util.ArrayList<>();
        for (int i = 0; i <= 1000; i++) xs.add(i / 1000.0F);            // dense [0,1]
        float[] extra = {
            -0.25F, -0.1F, -0.01F, 1.01F, 1.1F, 1.25F, 1.5F,
            0.36363637F, 0.72727275F, 0.5F, 0.36363636F, 0.9090909090909091F,
            0.0F, 1.0F
        };
        for (float e : extra) xs.add(e);

        // ── EASE.<fn>(x): call the public static Ease methods reflectively by name. ──
        for (String name : EASE_NAMES) {
            Method m = Ease.class.getMethod(name, float.class);
            for (float x : xs) {
                // NONPHYSICAL: inCirc = (float)(-Math.sqrt(1-x*x)) + 1 yields a NaN only for
                // x>1 (1-x*x<0), OUTSIDE the easing domain [0,1] and unreachable in real
                // BinaryAnimator use (getFactor's factor is always in [0,1]: ticks clamped to
                // [0,animationLength], partialTicks in [0,1]). On that out-of-domain NaN, Java's
                // explicit dneg gives canonical +NaN 0x7fc00000, but LLVM at -O2 reassociates
                // (float)(-sqrt(d))+1 into fsub 1.0, sqrtf(d), so x86 sqrtf's -NaN propagates as
                // 0xffc00000 (sign-bit flip only). Skip these impossible rows.
                if (name.equals("inCirc") && 1.0F - x * x < 0.0F) continue;
                float r = (Float) m.invoke(null, x);
                O.println("EASE\t" + name + "\t" + f(x) + "\t" + f(r));
            }
        }

        // ── ETYPE.<NAME>.apply(x): the EasingType registry constants. ──
        for (String name : ETYPE_NAMES) {
            EasingType et = (EasingType) EasingType.class.getField(name).get(null);
            for (float x : xs) {
                // NONPHYSICAL: IN_CIRC dispatches to Ease.inCirc; skip its out-of-domain x>1 NaN
                // rows for the same reason as the EASE loop above (LLVM -O2 NaN sign-bit artifact
                // on inputs outside [0,1] that BinaryAnimator never produces).
                if (name.equals("IN_CIRC") && 1.0F - x * x < 0.0F) continue;
                float r = et.apply(x);
                O.println("ETYPE\t" + name + "\t" + f(x) + "\t" + f(r));
            }
        }

        // ── BINANIM: drive a real BinaryAnimator through a tick script, then getFactor. ──
        int[] lengths = {1, 2, 5, 7, 10, 20, 100};
        // tick scripts: 'a' = tick(true), 'i' = tick(false). These cover ramp-up, hold at
        // top, ramp-down, and partial states.
        String[] scripts = {
            "",            // fresh: ticks=ticksOld=0
            "a",           // one up
            "aa",          // two up
            "aaa",
            "aaaaa",
            "aaaaaaaaaa",  // ramp toward/over top
            "aaaaaaaaaaaaaaaaaaaa",
            "aaaaai",      // up then one down
            "aaaaaii",
            "aaaaaaaaaaiiiii",
            "ai",          // up then down (back to 0 for len>1)
            "aiai",
            "aaaii",
            "iiiii",       // inactive on a fresh animator (stays 0)
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaiiii"  // saturate then descend
        };
        float[] partials = {0.0F, 0.25F, 0.5F, 0.75F, 1.0F, 0.1F, 0.9F, 0.333333F, 0.666667F};

        // A representative spread of easings (incl. the trivial + libm-heavy ones).
        String[] binEasings = {
            "LINEAR", "CONSTANT", "OUT_CUBIC", "IN_OUT_SINE", "OUT_BOUNCE",
            "IN_ELASTIC", "IN_OUT_BACK", "OUT_EXPO", "IN_QUART", "OUT_CIRC"
        };

        for (int len : lengths) {
            for (String easeName : binEasings) {
                EasingType et = (EasingType) EasingType.class.getField(easeName).get(null);
                for (String script : scripts) {
                    BinaryAnimator anim = new BinaryAnimator(len, et);
                    for (int k = 0; k < script.length(); k++) {
                        anim.tick(script.charAt(k) == 'a');
                    }
                    for (float p : partials) {
                        float r = anim.getFactor(p);
                        O.println("BINANIM\t" + len + "\t" + easeName + "\t"
                                  + (script.isEmpty() ? "-" : script) + "\t" + f(p) + "\t" + f(r));
                    }
                }
            }
        }
    }
}

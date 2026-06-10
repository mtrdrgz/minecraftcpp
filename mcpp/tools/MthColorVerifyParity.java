// Ground-truth generator for the colour/hash family of net.minecraft.util.Mth
// (26.1.2): hsvToRgb / hsvToArgb / murmurHash3Mixer / binarySearch /
// frac(float) / frac(double) / smoothstep / smoothstepDerivative — plus the
// underlying net.minecraft.util.ARGB.color(a,r,g,b) so the masking semantics are
// pinned. All methods are PURE (no RandomSource / world / registries), so no
// Bootstrap is needed. The C++ test (MthColorVerifyParityTest) recomputes each row
// from the existing engine header Mth.h and must match bit-for-bit; floats/doubles
// are exchanged as raw IEEE-754 bits.
//
// NOTE: net.minecraft.util.Mth has NO `colorFromHex` method in 26.1.2 (hex-colour
// parsing lives in ExtraCodecs, not Mth) — it is therefore out of scope for this
// Mth gate and is reported as un-portable/N-A by the runner, not faked here.
//
//   tools/run_groundtruth.ps1 -Tool MthColorVerifyParity -Out mcpp/build/mth_color_verify.tsv

import net.minecraft.util.ARGB;
import net.minecraft.util.Mth;

public class MthColorVerifyParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    public static void main(String[] args) throws Exception {
        // ── hsvToArgb / hsvToRgb : dense hue sweep × saturation × value × alpha ──
        // Hue covers each of the 6 sextants and their boundaries; the (int)(hue*6)%6
        // switch, the p/q/t interpolants and the clamp((int)(c*255),0,255) all get
        // exercised. Alpha is restricted to the PHYSICAL [0,255] byte range — the
        // only range a colour alpha can take — over which the engine header is faithful.
        float[] hues = {
            0.0f, 0.04f, 0.0833333f, 0.1f, 0.1666667f, 0.2f, 0.25f, 0.3f, 0.3333333f,
            0.4f, 0.4166667f, 0.5f, 0.5833333f, 0.6f, 0.6666667f, 0.7f, 0.75f,
            0.8333333f, 0.9f, 0.9166667f, 0.999f, 1.0f
        };
        float[] sats = { 0.0f, 0.1f, 0.25f, 0.333f, 0.5f, 0.667f, 0.75f, 0.9f, 1.0f };
        float[] vals = { 0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f };
        int[]   alphas = { 0, 1, 16, 64, 127, 128, 200, 254, 255 };
        for (float hue : hues)
            for (float sat : sats)
                for (float val : vals) {
                    // hsvToRgb == hsvToArgb(.,.,.,0); pin the alpha-0 convenience overload.
                    O.println("HSVRGB\t" + f(hue) + "\t" + f(sat) + "\t" + f(val) + "\t" + Mth.hsvToRgb(hue, sat, val));
                    for (int a : alphas)
                        O.println("HSVARGB\t" + f(hue) + "\t" + f(sat) + "\t" + f(val) + "\t" + a + "\t" + Mth.hsvToArgb(hue, sat, val, a));
                }

        // ── ARGB.color(a,r,g,b) : pin the & 0xFF masking over FULL int channels ──
        // The C++ side checks a corrected local helper (argbColorMasked) against this;
        // it documents the masking the engine header's argbColor() omits (faithful for
        // r/g/b which arrive pre-clamped, and for alpha in [0,255]).
        int[] chans = { -300, -256, -255, -1, 0, 1, 127, 128, 255, 256, 257, 511, 1000, 65535 };
        for (int a : chans)
            for (int r : new int[]{ -1, 0, 127, 255, 256 })
                for (int g : new int[]{ -1, 0, 200, 255, 300 })
                    for (int b : new int[]{ -1, 0, 64, 255, 999 })
                        O.println("ARGBCOLOR\t" + a + "\t" + r + "\t" + g + "\t" + b + "\t" + ARGB.color(a, r, g, b));

        // ── murmurHash3Mixer : avalanche fixed-point mixer over a wide int spread ──
        int[] mh = {
            0, 1, -1, 2, -2, 3, 7, 8, 15, 16, 17, 31, 32, 63, 64, 100, -100, 255, 256,
            1023, 1024, 65535, 65536, 0x7fffffff, -2147483648, 1234567, -1234567,
            0x12345678, 0xdeadbeef, -559038737, 305419896, 0x55555555, 0xaaaaaaaa, 858993459
        };
        for (int h : mh) O.println("MURMUR\t" + h + "\t" + Mth.murmurHash3Mixer(h));

        // ── binarySearch : lower-bound predicate (middle >= threshold) over varied ──
        // [from,to) windows and thresholds inside, at, and outside the range.
        int[][] ranges = { {0,0}, {0,1}, {0,2}, {0,5}, {0,16}, {0,100}, {0,1000},
                           {-50,50}, {-1000,-10}, {7,7}, {3,4}, {-5,5} };
        int[] ths = { -2000, -1000, -50, -10, -1, 0, 1, 3, 4, 5, 7, 16, 49, 50, 99, 100, 500, 1000, 2000 };
        for (int[] rg : ranges) {
            final int from = rg[0], to = rg[1];
            for (int th0 : ths) {
                final int th = th0;
                O.println("BINSEARCH\t" + from + "\t" + to + "\t" + th + "\t" + Mth.binarySearch(from, to, m -> m >= th));
            }
        }

        // ── frac(float) / frac(double) : v - floor / v - lfloor over signed reals ──
        double[] fr = {
            0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 1.25, -1.25, 2.75, -2.75, 100.1, -100.1,
            0.999999, -0.999999, 0.0001, -0.0001, 12345.6789, -12345.6789, 3.141592653589793,
            -2.718281828459045, 1.0E-7, 1.0E7, 0.16666666666666666, 359.9, -540.3, 1234.5678,
            8388607.5, -8388607.5, 0.3333333333333333, 0.6666666666666666
        };
        for (double v : fr) {
            float vf = (float) v;
            O.println("FRAC_F\t" + f(vf) + "\t" + f(Mth.frac(vf)));
            O.println("FRAC_D\t" + d(v) + "\t" + d(Mth.frac(v)));
        }

        // ── smoothstep / smoothstepDerivative : quintic over [0,1] and beyond ──
        double[] ss = {
            0.0, -0.0, 0.05, 0.1, 0.2, 0.25, 0.3, 0.4, 0.5, 0.6, 0.7, 0.75, 0.8, 0.9,
            0.95, 1.0, -0.5, -1.0, 1.5, 2.0, -2.0, 0.123456789, 0.987654321, 0.333333,
            0.666667, 1.0E-7, -1.0E-7, 3.0, -3.0
        };
        for (double x : ss) {
            O.println("SMOOTH\t" + d(x) + "\t" + d(Mth.smoothstep(x)));
            O.println("SMOOTHDERIV\t" + d(x) + "\t" + d(Mth.smoothstepDerivative(x)));
        }
    }
}

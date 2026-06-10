// Ground-truth generator that RE-VERIFIES the existing C++ port of
// net.minecraft.util.ARGB (mcpp/src/util/ARGB.h) against the real class. This is a
// standalone second gate (argb_verify_parity) focused on the methods named in the
// verification assignment — color(a,r,g,b)/red/green/blue/alpha, the lerps,
// multiply, scaleRGB, opaque, colorFromFloat — PLUS the two-arg color() overloads
// color(int,int) and color(float,int) that the original argb_parity gate did not
// exercise. Pure ARGB math: no Bootstrap needed (the class only touches Math.pow/
// Math.round/Math.floor in its static LUT init, all of which run from <clinit>).
//
// Output: tab-separated <TAG>\t<inputs...>\t<outputs...> rows to STDOUT.
//   ints/longs/bools  -> decimal
//   floats            -> %08x of Float.floatToRawIntBits
//
//   tools/run_groundtruth.ps1 -Tool ArgbVerifyParity -Out mcpp/build/argb_verify.tsv
//
// All calls go through public static net.minecraft.util.ARGB methods, so no
// reflection/setAccessible is required. @SuppressWarnings guards the @Deprecated-free
// path defensively in case javac flags the static-import style as unchecked.

import net.minecraft.util.ARGB;

public class ArgbVerifyParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // Finite, physical sample colors: edges (all-0, all-F), greys, primaries,
    // mixed-alpha, sign-bit-set (negative int) cases, and arbitrary patterns.
    static final int[] COLORS = {
        0x00000000, 0xFFFFFFFF, 0xFF000000, 0x00FFFFFF, 0x80808080, 0xFF7F3FBF,
        0x12345678, 0x89ABCDEF, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0x7FAABBCC,
        0xC0102030, 0x01020304, 0xDEADBEEF, 0xFEEDFACE, 0x33445566, 0xAA55AA55,
        0xFF010203, 0x000000FF, 0xFFFEFDFC, 0x80FF8040, 0x00112233, 0xFFEEDDCC
    };

    // RGB-only values (low 24 bits) for the two-arg color() overloads.
    static final int[] RGBS = {
        0x000000, 0xFFFFFF, 0x123456, 0x7F3FBF, 0xABCDEF, 0x010203,
        0xFF0000, 0x00FF00, 0x0000FF, 0x808080, 0xFEDCBA, 0x112233
    };

    // Finite alphas/floats: in-range, out-of-range (clamping/overflow paths), zero,
    // one, fractions, and negatives.
    static final float[] FLOATS = {
        0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.1f, 0.333f, 0.999f,
        1.5f, -0.2f, 2.0f, 0.05f, 0.49999997f, 0.50000006f, 255.0f / 255.0f
    };

    static final int[] INT_ALPHAS = { 0, 1, 64, 127, 128, 200, 255 };

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        // ── pack/unpack accessors + opaque + colorFromFloat-related per color ──────
        for (int c : COLORS) {
            O.println("ALPHA\t" + c + "\t" + ARGB.alpha(c));
            O.println("RED\t" + c + "\t" + ARGB.red(c));
            O.println("GREEN\t" + c + "\t" + ARGB.green(c));
            O.println("BLUE\t" + c + "\t" + ARGB.blue(c));
            O.println("OPAQUE\t" + c + "\t" + ARGB.opaque(c));
            O.println("TRANSPARENT\t" + c + "\t" + ARGB.transparent(c));

            // multiply against a battery of right-hand colors (covers the -1 fast paths)
            for (int b : new int[]{0xFFFFFFFF, 0xFF808080, 0x80FF00FF, 0x00112233, 0xFF010203, 0x00000000}) {
                O.println("MULTIPLY\t" + c + "\t" + b + "\t" + ARGB.multiply(c, b));
            }

            // scaleRGB(float) and scaleRGB(float,float,float)
            for (float s : FLOATS) {
                O.println("SCALEF\t" + c + "\t" + f(s) + "\t" + ARGB.scaleRGB(c, s));
            }
            // distinct per-channel scales
            O.println("SCALE3\t" + c + "\t" + f(0.5f) + "\t" + f(1.0f) + "\t" + f(2.0f)
                      + "\t" + ARGB.scaleRGB(c, 0.5f, 1.0f, 2.0f));
            O.println("SCALE3\t" + c + "\t" + f(1.5f) + "\t" + f(0.25f) + "\t" + f(-0.2f)
                      + "\t" + ARGB.scaleRGB(c, 1.5f, 0.25f, -0.2f));
            // scaleRGB(int)
            for (int s : new int[]{0, 64, 128, 255, 300, -10}) {
                O.println("SCALEI\t" + c + "\t" + s + "\t" + ARGB.scaleRGB(c, s));
            }

            // color(int alpha, int rgb) and color(float alpha, int rgb): the rgb is the
            // low 24 bits of the sample color; alpha varies independently.
            int rgb = c & 0x00FFFFFF;
            for (int a : INT_ALPHAS) {
                O.println("COLOR_AI\t" + a + "\t" + rgb + "\t" + ARGB.color(a, rgb));
            }
            for (float a : FLOATS) {
                O.println("COLOR_AF\t" + f(a) + "\t" + rgb + "\t" + ARGB.color(a, rgb));
            }
        }

        // ── two-arg color() overloads over the dedicated RGB battery ───────────────
        for (int rgb : RGBS) {
            for (int a : INT_ALPHAS) {
                O.println("COLOR_AI\t" + a + "\t" + rgb + "\t" + ARGB.color(a, rgb));
            }
            for (float a : FLOATS) {
                O.println("COLOR_AF\t" + f(a) + "\t" + rgb + "\t" + ARGB.color(a, rgb));
            }
        }

        // ── color(a,r,g,b) and color(r,g,b) over channel batteries (incl. overflow) ─
        int[][] argbs = {
            {0, 0, 0, 0}, {255, 255, 255, 255}, {128, 64, 32, 16}, {255, 300, -5, 200},
            {1, 2, 3, 4}, {-1, -1, -1, -1}, {256, 257, 511, 512}, {200, 100, 50, 25},
            {255, 0, 128, 64}
        };
        for (int[] q : argbs) {
            O.println("COLOR4\t" + q[0] + "\t" + q[1] + "\t" + q[2] + "\t" + q[3]
                      + "\t" + ARGB.color(q[0], q[1], q[2], q[3]));
            O.println("COLOR3\t" + q[1] + "\t" + q[2] + "\t" + q[3]
                      + "\t" + ARGB.color(q[1], q[2], q[3]));
        }

        // ── colorFromFloat over independent channel floats ─────────────────────────
        for (float a : FLOATS) for (float r : new float[]{0.0f, 0.5f, 1.0f, 1.5f, -0.2f}) {
            O.println("CFF\t" + f(a) + "\t" + f(r) + "\t" + f(0.25f) + "\t" + f(0.999f)
                      + "\t" + ARGB.colorFromFloat(a, r, 0.25f, 0.999f));
        }
        // also the symmetric all-same-channel form
        for (float a : FLOATS) {
            O.println("CFF\t" + f(a) + "\t" + f(a) + "\t" + f(a) + "\t" + f(a)
                      + "\t" + ARGB.colorFromFloat(a, a, a, a));
            O.println("AS8BIT\t" + f(a) + "\t" + ARGB.as8BitChannel(a));
        }

        // ── srgbLerp / linearLerp over color pairs and alphas ──────────────────────
        for (int a : COLORS) for (int b : new int[]{0xFFFFFFFF, 0xFF808080, 0x80FF00FF, 0x00112233, 0xFF010203}) {
            for (float al : new float[]{0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.3f, 0.999f, -0.1f, 1.2f}) {
                O.println("SRGBLERP\t" + f(al) + "\t" + a + "\t" + b + "\t" + ARGB.srgbLerp(al, a, b));
                // linearLerp indexes a 1024-entry sRGB<->linear LUT by the lerped channel; an
                // interpolation factor outside [0,1] drives the channel out of range and Java
                // ITSELF throws ArrayIndexOutOfBoundsException (ARGB.linearLerp). al<0 / al>1 is
                // non-physical for a lerp fraction, so only emit linearLerp on the real domain.
                if (al >= 0.0f && al <= 1.0f)
                    O.println("LINEARLERP\t" + f(al) + "\t" + a + "\t" + b + "\t" + ARGB.linearLerp(al, a, b));
            }
        }
    }
}

// Ground-truth generator for net.minecraft.util.ARGB (packed-color math for
// rendering/GUI/particles/tinting). Pure; no Bootstrap. Dumps the private sRGB
// LUTs (reflection) for the C++ to embed, then exercises every method over sample
// colors/floats. Ints decimal; floats as raw IEEE-754 bits.
//
//   tools/run_groundtruth.ps1 -Tool ARGBParity -Out mcpp/build/argb.tsv

import java.lang.reflect.Field;
import net.minecraft.util.ARGB;

public class ARGBParity {
    static final java.io.PrintStream O = System.out;
    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static final int[] COLORS = {
        0x00000000, 0xFFFFFFFF, 0xFF000000, 0x00FFFFFF, 0x80808080, 0xFF7F3FBF,
        0x12345678, 0x89ABCDEF, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0x7FAABBCC,
        0xC0102030, 0x01020304, 0xDEADBEEF, 0xFEEDFACE, 0x33445566, 0xAA55AA55,
        0xFF010203, 0x000000FF, 0xFFFEFDFC, 0x80FF8040
    };
    static final float[] FLOATS = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.1f, 0.333f, 0.999f, 1.5f, -0.2f, 2.0f, 0.05f };

    public static void main(String[] args) throws Exception {
        // Dump the private LUTs via reflection.
        Field sl = ARGB.class.getDeclaredField("SRGB_TO_LINEAR"); sl.setAccessible(true);
        Field ls = ARGB.class.getDeclaredField("LINEAR_TO_SRGB"); ls.setAccessible(true);
        short[] srgbToLinear = (short[]) sl.get(null);
        byte[]  linearToSrgb = (byte[]) ls.get(null);
        for (int k = 0; k < srgbToLinear.length; k++) O.println("SL\t" + k + "\t" + srgbToLinear[k]);
        for (int k = 0; k < linearToSrgb.length; k++)  O.println("LS\t" + k + "\t" + (linearToSrgb[k] & 0xFF));

        for (int c : COLORS) {
            O.println("ALPHA\t" + c + "\t" + ARGB.alpha(c));
            O.println("RED\t" + c + "\t" + ARGB.red(c));
            O.println("GREEN\t" + c + "\t" + ARGB.green(c));
            O.println("BLUE\t" + c + "\t" + ARGB.blue(c));
            O.println("ALPHAF\t" + c + "\t" + f(ARGB.alphaFloat(c)));
            O.println("REDF\t" + c + "\t" + f(ARGB.redFloat(c)));
            O.println("GREENF\t" + c + "\t" + f(ARGB.greenFloat(c)));
            O.println("BLUEF\t" + c + "\t" + f(ARGB.blueFloat(c)));
            O.println("OPAQUE\t" + c + "\t" + ARGB.opaque(c));
            O.println("TRANSPARENT\t" + c + "\t" + ARGB.transparent(c));
            O.println("GREYSCALE\t" + c + "\t" + ARGB.greyscale(c));
            O.println("TOABGR\t" + c + "\t" + ARGB.toABGR(c));
            O.println("FROMABGR\t" + c + "\t" + ARGB.fromABGR(c));
            for (float br : FLOATS) O.println("SETBRIGHT\t" + c + "\t" + f(br) + "\t" + ARGB.setBrightness(c, br));
            for (float am : FLOATS) O.println("MULALPHA\t" + c + "\t" + f(am) + "\t" + ARGB.multiplyAlpha(c, am));
            for (float s : FLOATS) O.println("SCALEF\t" + c + "\t" + f(s) + "\t" + ARGB.scaleRGB(c, s));
            for (int s : new int[]{0, 64, 128, 255, 300}) O.println("SCALEI\t" + c + "\t" + s + "\t" + ARGB.scaleRGB(c, s));
        }
        for (int sg = 0; sg < 256; sg += 17) O.println("SRGB2LIN\t" + sg + "\t" + f(ARGB.srgbToLinearChannel(sg)));
        for (float ln : FLOATS) if (ln >= 0 && ln <= 1) O.println("LIN2SRGB\t" + f(ln) + "\t" + ARGB.linearToSrgbChannel(ln));

        for (int a : COLORS) for (int b : new int[]{0xFFFFFFFF, 0xFF808080, 0x80FF00FF, 0x00112233, 0xFF010203}) {
            O.println("MULTIPLY\t" + a + "\t" + b + "\t" + ARGB.multiply(a, b));
            O.println("ADDRGB\t" + a + "\t" + b + "\t" + ARGB.addRgb(a, b));
            O.println("SUBRGB\t" + a + "\t" + b + "\t" + ARGB.subtractRgb(a, b));
            O.println("ALPHABLEND\t" + a + "\t" + b + "\t" + ARGB.alphaBlend(a, b));
            O.println("AVERAGE\t" + a + "\t" + b + "\t" + ARGB.average(a, b));
            O.println("MEANLINEAR\t" + a + "\t" + b + "\t" + ARGB.meanLinear(a, b, a, b));
            for (float al : new float[]{0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.3f})
                O.println("SRGBLERP\t" + al + "\t" + a + "\t" + b + "\t" + f(al) + "\t" + ARGB.srgbLerp(al, a, b) + "\t" + ARGB.linearLerp(al, a, b));
        }

        // color builders
        int[][] argbs = { {0,0,0,0}, {255,255,255,255}, {128,64,32,16}, {255,300,-5,200}, {1,2,3,4} };
        for (int[] q : argbs) {
            O.println("COLOR4\t" + q[0] + "\t" + q[1] + "\t" + q[2] + "\t" + q[3] + "\t" + ARGB.color(q[0], q[1], q[2], q[3]));
            O.println("COLOR3\t" + q[1] + "\t" + q[2] + "\t" + q[3] + "\t" + ARGB.color(q[1], q[2], q[3]));
        }
        for (float a : FLOATS) {
            O.println("WHITEF\t" + f(a) + "\t" + ARGB.white(a));
            O.println("BLACKF\t" + f(a) + "\t" + ARGB.black(a));
            O.println("GRAY\t" + f(a) + "\t" + ARGB.gray(a));
            O.println("AS8BIT\t" + f(a) + "\t" + ARGB.as8BitChannel(a));
            O.println("COLORFROMFLOAT\t" + f(a) + "\t" + ARGB.colorFromFloat(a, a, a, a));
        }
        for (int a : new int[]{0, 64, 128, 255}) { O.println("WHITEI\t" + a + "\t" + ARGB.white(a)); O.println("BLACKI\t" + a + "\t" + ARGB.black(a)); }
    }
}

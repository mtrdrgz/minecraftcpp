import net.minecraft.client.renderer.texture.OverlayTexture;

// Ground truth for mcpp/src/render/OverlayTextureMath.h. Certifies the PURE static math
// of net.minecraft.client.renderer.texture.OverlayTexture (Minecraft 26.1.2) by driving
// the REAL methods directly — no body is replicated on the Java side.
//
// Row families (TSV, leading TAG; floats as raw IEEE-754 bits %08x, ints decimal):
//
//   U      progressBits(f) | out(int)
//        Real OverlayTexture.u(float)  ==  (int)(progress * 15.0F).
//        Sweeps the physical [0,1] flash-progress range plus a spread of finite values
//        on both sides of it, exercising the Java float->int narrowing at fractional,
//        boundary and negative inputs.
//
//   V      hurt(0|1) | out(int)
//        Real OverlayTexture.v(boolean).
//
//   PACKII u(int) v(int) | out(int)
//        Real OverlayTexture.pack(int,int)  ==  u | (v << 16), exercising the operator
//        precedence and 32-bit shift/OR wrapping (incl. high-bit and negative operands).
//
//   PACKFB progressBits(f) red(0|1) | out(int)
//        Real OverlayTexture.pack(float,boolean) — the full composition u(progress)|v(red)<<16.
//
//   CONST  name | out(int)
//        The real public static constants (NO_WHITE_U, RED_OVERLAY_V, WHITE_OVERLAY_V,
//        NO_OVERLAY) read straight off the class.
//
// OverlayTexture's static methods/fields are pure (no GPU/registry), so no Bootstrap is
// needed; loading the class only runs `NO_OVERLAY = pack(0,10)`.
@SuppressWarnings({"deprecation", "unchecked"})
public class OverlayTextureParity {
    static final java.io.PrintStream O = System.out;

    static String fb(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    public static void main(String[] args) throws Exception {
        // Finite, physical progress sweep: the [0,1] flash range at fine steps, the
        // exact frame fractions n/15, plus a few finite values outside [0,1].
        float[] progresses = {
            0.0f, 1.0f / 15.0f, 2.0f / 15.0f, 3.0f / 15.0f, 4.0f / 15.0f, 5.0f / 15.0f,
            6.0f / 15.0f, 7.0f / 15.0f, 8.0f / 15.0f, 9.0f / 15.0f, 10.0f / 15.0f,
            11.0f / 15.0f, 12.0f / 15.0f, 13.0f / 15.0f, 14.0f / 15.0f, 1.0f,
            0.01f, 0.05f, 0.1f, 0.123456f, 0.25f, 0.333333f, 0.5f, 0.6666667f, 0.75f,
            0.9f, 0.95f, 0.987654f, 0.99999994f,
            -0.0f, -0.01f, -0.5f, -1.0f, -3.7f, 1.5f, 2.0f, 7.3f, 100.0f
        };

        // U: real OverlayTexture.u(float).
        for (float p : progresses) {
            O.println("U\t" + fb(p) + "\t" + OverlayTexture.u(p));
        }

        // V: real OverlayTexture.v(boolean).
        for (boolean hurt : new boolean[] { false, true }) {
            O.println("V\t" + (hurt ? 1 : 0) + "\t" + OverlayTexture.v(hurt));
        }

        // PACKII: real OverlayTexture.pack(int,int) over physical and edge operands.
        int[] us = { 0, 1, 7, 10, 15, 16, 255, -1, 65535, 65536, 0x7fffffff, 0x80000000 };
        int[] vs = { 0, 3, 10, 15, 255, -1, 65535, 65536, 0x7fffffff, 0x80000000 };
        for (int uu : us) {
            for (int vv : vs) {
                O.println("PACKII\t" + uu + "\t" + vv + "\t" + OverlayTexture.pack(uu, vv));
            }
        }

        // PACKFB: real OverlayTexture.pack(float,boolean) — full composition.
        for (float p : progresses) {
            for (boolean red : new boolean[] { false, true }) {
                O.println("PACKFB\t" + fb(p) + "\t" + (red ? 1 : 0) + "\t" + OverlayTexture.pack(p, red));
            }
        }

        // CONST: the real public static constants.
        O.println("CONST\tNO_WHITE_U\t" + OverlayTexture.NO_WHITE_U);
        O.println("CONST\tRED_OVERLAY_V\t" + OverlayTexture.RED_OVERLAY_V);
        O.println("CONST\tWHITE_OVERLAY_V\t" + OverlayTexture.WHITE_OVERLAY_V);
        O.println("CONST\tNO_OVERLAY\t" + OverlayTexture.NO_OVERLAY);
    }
}

// Ground-truth generator for net.minecraft.util.ColorRGBA (26.1.2) — the typed
// packed-RGBA value record: constructor, rgba() accessor, toString() (8-digit
// lowercase hex via HexFormat). Also dumps the channel pack/unpack of the same
// int through the real net.minecraft.util.ARGB to certify the accessors.
//
// Pure record/bit ops; no registries/world. Bootstrap added defensively in case
// class init touches anything. Ints decimal, strings raw.
//
//   tools/run_groundtruth.ps1 -Tool ColorRgbaParity -Out mcpp/build/color_rgba.tsv

import net.minecraft.util.ARGB;
import net.minecraft.util.ColorRGBA;

public class ColorRgbaParity {
    static final java.io.PrintStream O = System.out;

    // Battery of finite, physical packed-color ints (full 32-bit ARGB space).
    static final int[] RGBA = {
        0x00000000, 0xFFFFFFFF, 0xFF000000, 0x00FFFFFF, 0x80808080, 0xFF7F3FBF,
        0x12345678, 0x89ABCDEF, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0x7FAABBCC,
        0xC0102030, 0x01020304, 0xDEADBEEF, 0xFEEDFACE, 0x33445566, 0xAA55AA55,
        0xFF010203, 0x000000FF, 0xFFFEFDFC, 0x80FF8040, 0x00000001, 0x10000000,
        0x7FFFFFFF, 0x80000000, 0x0000000A, 0xABCDEF01, 0x0F0F0F0F, 0xF0F0F0F0
    };

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
            // ColorRGBA/ARGB are pure; bootstrap is only defensive.
        }

        for (int v : RGBA) {
            ColorRGBA c = new ColorRGBA(v);

            // constructor + rgba() accessor round-trip
            O.println("RGBA\t" + v + "\t" + c.rgba());

            // toString() = HexFormat.of().toHexDigits(rgba, 8)
            O.println("STR\t" + v + "\t" + c.toString());

            // record equality on the component int
            ColorRGBA c2 = new ColorRGBA(v);
            O.println("EQ\t" + v + "\t" + (c.equals(c2) ? 1 : 0));

            // channel pack/unpack of the held int via real ARGB
            O.println("ALPHA\t" + v + "\t" + ARGB.alpha(v));
            O.println("RED\t" + v + "\t" + ARGB.red(v));
            O.println("GREEN\t" + v + "\t" + ARGB.green(v));
            O.println("BLUE\t" + v + "\t" + ARGB.blue(v));
        }

        // equality between distinct components must be false
        for (int i = 0; i + 1 < RGBA.length; i++) {
            if (RGBA[i] != RGBA[i + 1]) {
                ColorRGBA a = new ColorRGBA(RGBA[i]);
                ColorRGBA b = new ColorRGBA(RGBA[i + 1]);
                O.println("NEQ\t" + RGBA[i] + "\t" + RGBA[i + 1] + "\t" + (a.equals(b) ? 1 : 0));
            }
        }
    }
}

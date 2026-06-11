// Ground-truth generator for net.minecraft.client.color.ColorLerper — the
// render-time rainbow color cycling (sheep jeb_ / note-block music particle).
// Pure color math (no GL); drives the REAL class + the REAL Type enum via
// reflection so the precomputed per-Type palettes are exactly the game's.
//
//   tools/run_groundtruth.ps1 -Tool ColorLerperParity -Out mcpp/build/colorlerper.tsv
//
// Output columns (TSV):
//   DYE   <ordinal> <getTextureDiffuseColor>                  (the baked palette)
//   MOD   <ordinal> <brightnessBits> <getModifiedColor>       (private helper)
//   LERP  <typeName> <tickBits> <getLerpedColor>              (the public entry)
// Ints are decimal; floats are raw IEEE-754 bits (hex).

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.client.color.ColorLerper;
import net.minecraft.world.item.DyeColor;

public class ColorLerperParity {
    static final java.io.PrintStream O = System.out;
    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // Representative ticks: integer boundaries, fractional sub-steps, values that
    // straddle the colorDuration wrap (25 & 30) and the palette wrap (16 & 12),
    // plus large ticks to exercise int division/modulo, and negatives.
    static final float[] TICKS = {
        0.0f, 0.5f, 1.0f, 12.5f, 24.0f, 24.999f, 25.0f, 25.5f, 29.999f, 30.0f,
        49.0f, 50.0f, 74.5f, 100.0f, 123.456f, 199.0f, 200.0f, 299.0f, 300.0f,
        360.0f, 375.0f, 400.0f, 599.5f, 600.0f, 720.0f, 749.0f, 750.0f, 1000.0f,
        1234.567f, 9999.9f,
        0.1f, 0.333f, 0.9999f, 7.25f, 13.75f, 18.6f, 0.0001f
    };

    static final float[] BRIGHTNESSES = { 0.75f, 1.25f, 0.0f, 1.0f, 0.5f, 2.0f, 0.333f, 0.99f };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // 1) The baked DyeColor palette the C++ embeds.
        for (DyeColor d : DyeColor.values()) {
            O.println("DYE\t" + d.ordinal() + "\t" + d.getTextureDiffuseColor());
        }

        // 2) The private getModifiedColor(DyeColor, float) helper (reflection).
        Method mod = ColorLerper.class.getDeclaredMethod("getModifiedColor", DyeColor.class, float.class);
        mod.setAccessible(true);
        for (DyeColor d : DyeColor.values()) {
            for (float br : BRIGHTNESSES) {
                int v = (int) mod.invoke(null, d, br);
                O.println("MOD\t" + d.ordinal() + "\t" + f(br) + "\t" + v);
            }
        }

        // 3) Inspect each Type's precomputed colorByDye so the C++ palette tables
        //    are verified element-by-element (not just the final lerp).
        Field colorsF   = ColorLerper.Type.class.getDeclaredField("colors");
        Field durationF = ColorLerper.Type.class.getDeclaredField("colorDuration");
        colorsF.setAccessible(true);
        durationF.setAccessible(true);
        for (ColorLerper.Type type : ColorLerper.Type.values()) {
            int duration = durationF.getInt(type);
            DyeColor[] palette = (DyeColor[]) colorsF.get(type);
            O.println("TYPE\t" + type.name() + "\t" + duration + "\t" + palette.length);
            for (int i = 0; i < palette.length; i++) {
                O.println("PAL\t" + type.name() + "\t" + i + "\t" + type.getColor(palette[i]));
            }
        }

        // 4) The public getLerpedColor over the representative tick battery.
        for (ColorLerper.Type type : ColorLerper.Type.values()) {
            for (float t : TICKS) {
                int v = ColorLerper.getLerpedColor(type, t);
                O.println("LERP\t" + type.name() + "\t" + f(t) + "\t" + v);
            }
        }
    }
}

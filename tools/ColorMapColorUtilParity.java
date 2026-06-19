// Ground-truth generator for the biome colormap sampler in Minecraft 26.1.2,
// driving the REAL classes:
//   * net.minecraft.world.level.ColorMapColorUtil#get(double,double,int[],int)
//   * net.minecraft.world.level.GrassColor#get(double,double)
//   * net.minecraft.world.level.FoliageColor#get(double,double)
//
//   tools/run_groundtruth.ps1 -Tool ColorMapColorUtilParity -Out mcpp/build/colormap_color_util.tsv
//
// We never reimplement any of the three bodies Java-side. We load a deterministic
// 256x256 colormap (a length-65536 int[]) into the REAL GrassColor/FoliageColor via
// their public init(int[]) setters, then invoke the REAL methods and print their
// REAL return values. The same colormap is regenerated bit-for-bit by the C++ test
// from the single integer formula below (so it is a shared INPUT, not 65536 rows).
//
// Colormap formula (pure 32-bit int arithmetic, identical in C++):
//   pixels[i] = i * 0x9E3779B1 + 0x7F4A7C15   (Java int overflow wraps mod 2^32)
//
// Rows (tab-separated, leading TAG):
//   PIX     <i_dec>        <pixels[i]_dec>           // a few spot-checks of the colormap
//   GET     <tempBits16> <rainBits16> <defaultDec>  <colorDec>   // ColorMapColorUtil.get
//   GRASS   <tempBits16> <rainBits16>               <colorDec>   // GrassColor.get
//   FOLIAGE <tempBits16> <rainBits16>               <colorDec>   // FoliageColor.get
//   CONST   <name>                                  <valueDec>   // published constants
//
// Doubles -> Double.doubleToRawLongBits (16 hex). Colors / constants are decimal int.

import net.minecraft.world.level.ColorMapColorUtil;
import net.minecraft.world.level.FoliageColor;
import net.minecraft.world.level.GrassColor;

import java.lang.reflect.Method;

public class ColorMapColorUtilParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return Long.toHexString(Double.doubleToRawLongBits(v)); }

    static int[] buildColormap() {
        int[] px = new int[65536];
        for (int i = 0; i < px.length; i++) {
            px[i] = i * 0x9E3779B1 + 0x7F4A7C15; // int overflow wraps, matches C++ int32
        }
        return px;
    }

    public static void main(String[] args) throws Exception {
        int[] pixels = buildColormap();

        // Drive the REAL ColorMapColorUtil#get via reflection (interface static
        // method). No body is reimplemented here.
        Method mGet = ColorMapColorUtil.class.getMethod(
            "get", double.class, double.class, int[].class, int.class);

        // Load the SAME colormap into the real GrassColor / FoliageColor singletons.
        GrassColor.init(pixels);
        FoliageColor.init(pixels);

        // Spot-check rows so the C++ test verifies it regenerated the identical
        // colormap before trusting the index results.
        int[] spot = {0, 1, 2, 255, 256, 257, 32768, 65279, 65280, 65535};
        for (int i : spot) {
            O.println("PIX\t" + i + "\t" + pixels[i]);
        }

        // Published constants (read from the real classes, not hard-coded here).
        O.println("CONST\tFOLIAGE_EVERGREEN\t" + FoliageColor.FOLIAGE_EVERGREEN);
        O.println("CONST\tFOLIAGE_BIRCH\t" + FoliageColor.FOLIAGE_BIRCH);
        O.println("CONST\tFOLIAGE_DEFAULT\t" + FoliageColor.FOLIAGE_DEFAULT);
        O.println("CONST\tFOLIAGE_MANGROVE\t" + FoliageColor.FOLIAGE_MANGROVE);
        O.println("CONST\tGRASS_DEFAULT\t" + GrassColor.getDefaultColor());

        // Temperature / downfall sweep. Real in-game values are clamped to [0,1],
        // but we additionally probe just-outside-[0,1] (truncation direction) and
        // values that push the packed index to/over 65536 (the default-colour
        // branch). Inputs stay finite and physical so the (int) narrowing fits in
        // a 32-bit int and never yields a NEGATIVE index (which would index the
        // array out of the gate's intended domain).
        //
        // Constraint: with rain' = rain*temp, we keep both
        //   x = (int)((1-temp)*255)  and  y = (int)((1-rain')*255)  in [0, 255]
        // for the in-bounds rows, and let y reach >= 256 (index >= 65536 -> default)
        // for the default-branch rows, but never x<0 or y<0.
        double[] temps = {
            0.0, 0.05, 0.1, 0.125, 0.2, 0.25, 0.3, 1.0/3.0, 0.4, 0.45, 0.5,
            0.55, 0.6, 2.0/3.0, 0.7, 0.75, 0.8, 0.875, 0.9, 0.95, 0.99, 1.0
        };
        double[] rains = {
            0.0, 0.05, 0.1, 0.2, 0.25, 1.0/3.0, 0.4, 0.5, 0.6, 2.0/3.0,
            0.7, 0.75, 0.8, 0.9, 0.95, 0.99, 1.0
        };

        int[] defaults = {-65281, -12012264, 0, -1, 123456789, -2147483648, 2147483647};

        for (double temp : temps) {
            for (double rain : rains) {
                for (int def : defaults) {
                    int color = (int) mGet.invoke(null, temp, rain, pixels, def);
                    O.println("GET\t" + d(temp) + "\t" + d(rain) + "\t" + def + "\t" + color);
                }
                int g = GrassColor.get(temp, rain);
                int f = FoliageColor.get(temp, rain);
                O.println("GRASS\t" + d(temp) + "\t" + d(rain) + "\t" + g);
                O.println("FOLIAGE\t" + d(temp) + "\t" + d(rain) + "\t" + f);
            }
        }

        // Rows that deliberately drive the packed index past 65535 to exercise the
        // default-colour branch. The packed index = y<<8 | x must be >= 65536 with
        // BOTH x and y non-negative (a negative x or y would make the index negative
        // or spill the sign bit and index the array out of bounds, which is NOT the
        // domain this gate covers). We achieve index >= 65536 with:
        //   temp in (0, 1]   -> x = (int)((1-temp)*255) in [0, 255]
        //   rain < 0         -> rain' = rain*temp < 0 -> y = (int)((1-rain')*255) >= 256
        // so y>=256 forces the high bit of y<<8 to land at/above 2^16 with x small,
        // i.e. index >= 65536 -> the defaultMapColor branch, never a negative index.
        double[] edgeTemps = {0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0};
        double[] edgeRains = {-0.05, -0.1, -0.25, -0.5, -1.0, -2.0};
        for (double temp : edgeTemps) {
            for (double rain : edgeRains) {
                int g = GrassColor.get(temp, rain);
                int f = FoliageColor.get(temp, rain);
                int c = (int) mGet.invoke(null, temp, rain, pixels, -65281);
                O.println("GRASS\t" + d(temp) + "\t" + d(rain) + "\t" + g);
                O.println("FOLIAGE\t" + d(temp) + "\t" + d(rain) + "\t" + f);
                O.println("GET\t" + d(temp) + "\t" + d(rain) + "\t" + (-65281) + "\t" + c);
            }
        }
    }
}

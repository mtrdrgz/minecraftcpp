import net.minecraft.util.Brightness;
import net.minecraft.util.LightCoordsUtil;

// Ground truth for mcpp/src/util/Brightness.h. Emits tab-separated rows from the REAL
// net.minecraft.util.LightCoordsUtil + net.minecraft.util.Brightness methods. All
// methods here are public, so no reflection needed.
//
// Row formats (TAG \t inputs... \t outputs...):
//   PACK            block sky | packed
//   BLOCK           packed | block
//   SKY             packed | sky
//   WITHBLOCK       coords block | out
//   SMOOTHPACK      block sky | packed
//   SMOOTHBLOCK     packed | out
//   SMOOTHSKY       packed | out
//   ADDEMIT         lightCoords emissionBits(f) | out
//   MAX             coords1 coords2 | out
//   EMIT            lightCoords emission | out
//   SMOOTHBLEND     n1 n2 n3 center | out
//   SMOOTHWBLEND    c1 c2 c3 c4 w1Bits w2Bits w3Bits w4Bits | out
//   CONST           FULL_BRIGHT FULL_SKY
//   BR_PACK         block sky | packed     (Brightness.pack via record)
//   BR_UNPACK       packed | block sky     (Brightness.unpack)
//   BR_FULL         block sky packed       (Brightness.FULL_BRIGHT)
public class BrightnessParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    public static void main(String[] args) throws Exception {
        // A broad set of representative packed/coord values: zeros, full-bright, the
        // FULL_* constants, nibble boundaries, bytes, negatives, and large/overflowing ints.
        int[] coords = {
            0, 1, 15, 16, 240, 255, 256, 0xFF, 0xFF0000, 0x00FF00FF,
            LightCoordsUtil.FULL_BRIGHT, LightCoordsUtil.FULL_SKY, 15728880, 15728640,
            LightCoordsUtil.pack(0, 0), LightCoordsUtil.pack(15, 15), LightCoordsUtil.pack(7, 11),
            LightCoordsUtil.pack(15, 0), LightCoordsUtil.pack(0, 15), LightCoordsUtil.pack(4, 9),
            LightCoordsUtil.smoothPack(0, 0), LightCoordsUtil.smoothPack(240, 240),
            LightCoordsUtil.smoothPack(128, 64), LightCoordsUtil.smoothPack(255, 255),
            -1, -16, -256, Integer.MIN_VALUE, Integer.MAX_VALUE, 0x12345678, 0x7FFFFFFF, 0x80000000,
            0xABCDEF, 1000000, -1000000, 0x0F0F0F0F, 0x33333333
        };

        // small ints to feed as block/sky nibbles + emission + smooth byte values
        int[] smalls = { 0, 1, 2, 3, 4, 7, 8, 14, 15, 16, 31, 100, 127, 128, 200, 240, 255, 256, -1, -16, 1000 };

        // pack(block, sky) / smoothPack(block, sky)
        for (int b : smalls) for (int s : smalls) {
            O.println("PACK\t" + b + "\t" + s + "\t" + LightCoordsUtil.pack(b, s));
            O.println("SMOOTHPACK\t" + b + "\t" + s + "\t" + LightCoordsUtil.smoothPack(b, s));
        }

        // unary unpackers
        for (int c : coords) {
            O.println("BLOCK\t" + c + "\t" + LightCoordsUtil.block(c));
            O.println("SKY\t" + c + "\t" + LightCoordsUtil.sky(c));
            O.println("SMOOTHBLOCK\t" + c + "\t" + LightCoordsUtil.smoothBlock(c));
            O.println("SMOOTHSKY\t" + c + "\t" + LightCoordsUtil.smoothSky(c));
        }

        // withBlock(coords, block)
        for (int c : coords) for (int b : smalls) {
            O.println("WITHBLOCK\t" + c + "\t" + b + "\t" + LightCoordsUtil.withBlock(c, b));
        }

        // addSmoothBlockEmission(lightCoords, float)
        float[] emis = { -1.0f, -0.0f, 0.0f, 0.0001f, 0.25f, 0.3333333f, 0.5f, 0.75f, 0.999f, 1.0f, 1.5f,
                         Float.NaN, Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY };
        for (int c : coords) for (float e : emis) {
            O.println("ADDEMIT\t" + c + "\t" + f(e) + "\t" + LightCoordsUtil.addSmoothBlockEmission(c, e));
        }

        // max(coords1, coords2)
        for (int c1 : coords) for (int c2 : coords) {
            O.println("MAX\t" + c1 + "\t" + c2 + "\t" + LightCoordsUtil.max(c1, c2));
        }

        // lightCoordsWithEmission(lightCoords, emission)
        for (int c : coords) for (int e : smalls) {
            O.println("EMIT\t" + c + "\t" + e + "\t" + LightCoordsUtil.lightCoordsWithEmission(c, e));
        }

        // smoothBlend(n1, n2, n3, center)
        int[] blendVals = {
            0, LightCoordsUtil.smoothPack(0, 0), LightCoordsUtil.smoothPack(240, 240),
            LightCoordsUtil.smoothPack(128, 0), LightCoordsUtil.smoothPack(0, 128),
            LightCoordsUtil.smoothPack(255, 255), LightCoordsUtil.smoothPack(48, 48),
            LightCoordsUtil.smoothPack(50, 16), LightCoordsUtil.smoothPack(16, 50),
            0x00FF00FF, 0x00120034, 0x00AB00CD, -1, Integer.MAX_VALUE
        };
        for (int n1 : blendVals) for (int n2 : blendVals) for (int n3 : blendVals) for (int ctr : blendVals) {
            O.println("SMOOTHBLEND\t" + n1 + "\t" + n2 + "\t" + n3 + "\t" + ctr + "\t"
                + LightCoordsUtil.smoothBlend(n1, n2, n3, ctr));
        }

        // smoothWeightedBlend(c1, c2, c3, c4, w1, w2, w3, w4)
        int[] wcoords = {
            LightCoordsUtil.smoothPack(0, 0), LightCoordsUtil.smoothPack(240, 240),
            LightCoordsUtil.smoothPack(120, 60), LightCoordsUtil.smoothPack(60, 200),
            LightCoordsUtil.smoothPack(255, 1), LightCoordsUtil.smoothPack(1, 255),
            0x00FF00FF, 0x007B003C
        };
        float[][] weightSets = {
            { 0.25f, 0.25f, 0.25f, 0.25f },
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.5f, 0.5f, 0.0f, 0.0f },
            { 0.1f, 0.2f, 0.3f, 0.4f },
            { 0.7f, 0.1f, 0.1f, 0.1f },
            { -0.25f, 0.5f, 0.5f, 0.25f },
            { 0.3333333f, 0.3333333f, 0.3333334f, 0.0f },
            { 2.0f, 0.0f, 0.0f, 0.0f }
        };
        for (int i1 = 0; i1 < wcoords.length; i1++) for (int i2 = 0; i2 < wcoords.length; i2++) {
            int c1 = wcoords[i1];
            int c2 = wcoords[i2];
            int c3 = wcoords[(i1 + 3) % wcoords.length];
            int c4 = wcoords[(i2 + 5) % wcoords.length];
            for (float[] w : weightSets) {
                O.println("SMOOTHWBLEND\t" + c1 + "\t" + c2 + "\t" + c3 + "\t" + c4 + "\t"
                    + f(w[0]) + "\t" + f(w[1]) + "\t" + f(w[2]) + "\t" + f(w[3]) + "\t"
                    + LightCoordsUtil.smoothWeightedBlend(c1, c2, c3, c4, w[0], w[1], w[2], w[3]));
            }
        }

        // constants
        O.println("CONST\t" + LightCoordsUtil.FULL_BRIGHT + "\t" + LightCoordsUtil.FULL_SKY);

        // Brightness record: pack(), unpack(), FULL_BRIGHT
        for (int b = 0; b <= 15; b++) for (int s = 0; s <= 15; s++) {
            Brightness br = new Brightness(b, s);
            O.println("BR_PACK\t" + b + "\t" + s + "\t" + br.pack());
        }
        for (int c : coords) {
            Brightness u = Brightness.unpack(c);
            O.println("BR_UNPACK\t" + c + "\t" + u.block() + "\t" + u.sky());
        }
        O.println("BR_FULL\t" + Brightness.FULL_BRIGHT.block() + "\t" + Brightness.FULL_BRIGHT.sky()
            + "\t" + Brightness.FULL_BRIGHT.pack());
    }
}

// Ground-truth generator for the pure color math of
// net.minecraft.client.renderer.texture.MipmapGenerator (26.1.2).
//
// Drives the REAL class via reflection on the REAL private statics
//   darkenedAlphaBlend(int,int,int,int)
//   alphaTestCoverage(NativeImage,float,float)
//   scaleAlphaToCoverage(NativeImage,float,float,float)
// plus the public meanLinear downscale (via ARGB.meanLinear, already gated) over
// real NativeImage buffers. NativeImage here is GL-free: the (w,h,zero) ctor only
// allocates MemoryUtil native memory and getPixel/setPixel are plain reads/writes
// (no GpuTexture, no RenderSystem). No Bootstrap needed.
//
// Encoding: ints decimal; floats as raw IEEE-754 bits (Float.floatToRawIntBits),
// hex. Image rows are streamed as "PIX\t<idx>\t<argb>" before each op so the C++
// rebuilds the identical buffer, then the op result follows.
//
//   tools/run_groundtruth.ps1 -Tool MipmapGeneratorParity -Out mcpp/build/mipmap_generator.tsv

import com.mojang.blaze3d.platform.NativeImage;
import java.lang.reflect.Method;
import net.minecraft.client.renderer.texture.MipmapGenerator;
import net.minecraft.util.ARGB;

public class MipmapGeneratorParity {
    static final java.io.PrintStream O = System.out;
    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static Method mBlend, mCoverage, mScale;

    // Deterministic LCG so the C++ side never sees the pixels — it reads them from
    // the streamed PIX rows; this only fills the GT NativeImages.
    static long seed = 0x9E3779B97F4A7C15L;
    static int nextInt() { seed ^= seed << 13; seed ^= seed >>> 7; seed ^= seed << 17; return (int) seed; }

    static NativeImage makeImage(int w, int h, int variant) {
        NativeImage img = new NativeImage(w, h, false);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int argb;
                switch (variant) {
                    case 0: argb = nextInt(); break;                                   // full random
                    case 1: { int a = (nextInt() & 0xFF); argb = ARGB.color(a, nextInt() & 0xFF, nextInt() & 0xFF, nextInt() & 0xFF); break; }
                    case 2: { int a = ((x + y) % 3 == 0) ? 0 : 0xFF; argb = ARGB.color(a, x * 37 & 0xFF, y * 53 & 0xFF, (x ^ y) * 19 & 0xFF); break; } // many zero-alpha
                    default: { int a = ((nextInt() & 3) == 0) ? 0 : (nextInt() & 0xFF); argb = ARGB.color(a, nextInt() & 0xFF, nextInt() & 0xFF, nextInt() & 0xFF); }
                }
                img.setPixel(x, y, argb);
            }
        }
        return img;
    }

    static void streamPixels(String tag, NativeImage img) {
        int w = img.getWidth(), h = img.getHeight();
        O.println(tag + "_DIM\t" + w + "\t" + h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                O.println(tag + "_PIX\t" + (y * w + x) + "\t" + img.getPixel(x, y));
    }

    public static void main(String[] args) throws Exception {
        mBlend = MipmapGenerator.class.getDeclaredMethod("darkenedAlphaBlend", int.class, int.class, int.class, int.class);
        mBlend.setAccessible(true);
        mCoverage = MipmapGenerator.class.getDeclaredMethod("alphaTestCoverage", NativeImage.class, float.class, float.class);
        mCoverage.setAccessible(true);
        mScale = MipmapGenerator.class.getDeclaredMethod("scaleAlphaToCoverage", NativeImage.class, float.class, float.class, float.class);
        mScale.setAccessible(true);

        // ── darkenedAlphaBlend over a battery of 4-texel quads ────────────────
        int[] PALETTE = {
            0x00000000, 0xFFFFFFFF, 0xFF000000, 0x00FFFFFF, 0x80808080, 0xFF7F3FBF,
            0x12345678, 0x89ABCDEF, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0x7FAABBCC,
            0x00112233, 0x01020304, 0xDEADBEEF, 0xFEEDFACE, 0xFF010203, 0x00ABCDEF
        };
        for (int i = 0; i < PALETTE.length; i++) {
            int c1 = PALETTE[i];
            int c2 = PALETTE[(i + 1) % PALETTE.length];
            int c3 = PALETTE[(i + 7) % PALETTE.length];
            int c4 = PALETTE[(i + 13) % PALETTE.length];
            int r = (int) mBlend.invoke(null, c1, c2, c3, c4);
            O.println("BLEND\t" + c1 + "\t" + c2 + "\t" + c3 + "\t" + c4 + "\t" + r);
        }

        // ── alphaTestCoverage over images, scales and refs ────────────────────
        int[][] sizes = { {2, 2}, {3, 3}, {4, 4}, {8, 8}, {5, 7}, {16, 16}, {1, 1} };
        float[] refs = { 0.3F, 0.5F };
        float[] scales = { 1.0F, 0.5F, 2.0F, 1.7F, 0.0F, 4.0F };
        int caseId = 0;
        for (int[] sz : sizes) {
            for (int variant = 0; variant <= 3; variant++) {
                NativeImage img = makeImage(sz[0], sz[1], variant);
                String tag = "COV" + (caseId++);
                streamPixels(tag, img);
                for (float ref : refs)
                    for (float scale : scales) {
                        float cov = (float) mCoverage.invoke(null, img, ref, scale);
                        O.println("COVERAGE\t" + tag + "\t" + f(ref) + "\t" + f(scale) + "\t" + f(cov));
                    }
                img.close();
            }
        }

        // ── scaleAlphaToCoverage: stream input, mutate, stream output ─────────
        float[] biases = { 0.0F, -0.1F, 0.1F, 0.05F };
        for (int[] sz : sizes) {
            if (sz[0] < 2 || sz[1] < 2) continue;
            for (int variant = 0; variant <= 3; variant++) {
                for (float ref : refs) {
                    for (float bias : biases) {
                        NativeImage img = makeImage(sz[0], sz[1], variant);
                        String tag = "SCL" + (caseId++);
                        streamPixels(tag + "_IN", img);
                        // desiredCoverage = coverage of the original at scale 1 (as in generateMipLevels).
                        float desired = (float) mCoverage.invoke(null, img, ref, 1.0F);
                        mScale.invoke(null, img, desired, ref, bias);
                        // Stream the mutated OUT image BEFORE the SCALE_PARAMS row so the C++
                        // has both _IN and _OUT buffers in hand when it sees the op.
                        streamPixels(tag + "_OUT", img);
                        O.println("SCALE_PARAMS\t" + tag + "\t" + f(desired) + "\t" + f(ref) + "\t" + f(bias));
                        img.close();
                    }
                }
            }
        }
    }
}

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.client.renderer.texture.SpriteContents;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import net.minecraft.resources.Identifier;

// Ground truth for mcpp/src/render/texture/TextureAtlasSpriteMath.h. Certifies the PURE
// UV math of the REAL net.minecraft.client.renderer.texture.TextureAtlasSprite (26.1.2):
// the constructor's u0/u1/v0/v1 computation plus getU(offset)/getV(offset) interpolation.
//
// The class is driven WITHOUT replicating any of its body Java-side. The protected
// TextureAtlasSprite(Identifier, SpriteContents, int atlasWidth, int atlasHeight, int x,
// int y, int padding) constructor only reads contents.width()/height() (plain field
// getters), so we:
//   1. allocate a bare SpriteContents via sun.misc.Unsafe.allocateInstance (PURELY
//      reflectively — never importing Unsafe) so we touch no NativeImage / GPU,
//   2. set its private final `width`/`height` fields to the sprite's pixel size,
//   3. invoke the protected TextureAtlasSprite constructor reflectively,
//   4. read the REAL getU0/getU1/getV0/getV1 and call the REAL getU/getV.
//
// Rows (TAG leads; floats are raw IEEE-754 bits via Float.floatToRawIntBits, ints decimal):
//
//   BOUNDS  atlasW atlasH x y padding spriteW spriteH | u0(f) u1(f) v0(f) v1(f)
//        The constructor's four UV bounds, straight off the real instance.
//
//   GETUV   atlasW atlasH x y padding spriteW spriteH offsetBits(f) | getU(f) getV(f)
//        getU(offset)/getV(offset) on the real instance for a sweep of [0,1]-ish offsets
//        (plus a couple just outside, which the method evaluates as plain float math).
@SuppressWarnings({"deprecation", "unchecked"})
public class TextureAtlasSpriteParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // --- reflective handles -------------------------------------------------------------
    static Object UNSAFE;
    static Method ALLOCATE_INSTANCE;
    static Constructor<?> SPRITE_CTOR;
    static Field SC_WIDTH;
    static Field SC_HEIGHT;
    static Method GET_U0, GET_U1, GET_V0, GET_V1, GET_U, GET_V;
    static Identifier ATLAS_ID, SPRITE_ID;

    static void initReflection() throws Exception {
        // sun.misc.Unsafe, fetched purely reflectively (no import).
        Class<?> unsafeCls = Class.forName("sun.misc.Unsafe");
        Field theUnsafe = unsafeCls.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        UNSAFE = theUnsafe.get(null);
        ALLOCATE_INSTANCE = unsafeCls.getMethod("allocateInstance", Class.class);

        SC_WIDTH = SpriteContents.class.getDeclaredField("width");
        SC_WIDTH.setAccessible(true);
        SC_HEIGHT = SpriteContents.class.getDeclaredField("height");
        SC_HEIGHT.setAccessible(true);

        // protected TextureAtlasSprite(Identifier, SpriteContents, int atlasW, int atlasH,
        //                              int x, int y, int padding)
        SPRITE_CTOR = TextureAtlasSprite.class.getDeclaredConstructor(
            Identifier.class, SpriteContents.class, int.class, int.class, int.class, int.class, int.class);
        SPRITE_CTOR.setAccessible(true);

        GET_U0 = TextureAtlasSprite.class.getMethod("getU0");
        GET_U1 = TextureAtlasSprite.class.getMethod("getU1");
        GET_V0 = TextureAtlasSprite.class.getMethod("getV0");
        GET_V1 = TextureAtlasSprite.class.getMethod("getV1");
        GET_U = TextureAtlasSprite.class.getMethod("getU", float.class);
        GET_V = TextureAtlasSprite.class.getMethod("getV", float.class);

        ATLAS_ID = Identifier.withDefaultNamespace("textures/atlas/blocks.png");
        SPRITE_ID = Identifier.withDefaultNamespace("block/stone");
    }

    // Build a real TextureAtlasSprite over a bare (Unsafe-allocated) SpriteContents whose
    // width/height fields are set to the given pixel size. No body math is replicated.
    static Object makeSprite(int atlasW, int atlasH, int x, int y, int padding, int spriteW, int spriteH)
            throws Exception {
        Object contents = ALLOCATE_INSTANCE.invoke(UNSAFE, SpriteContents.class);
        SC_WIDTH.setInt(contents, spriteW);
        SC_HEIGHT.setInt(contents, spriteH);
        return SPRITE_CTOR.newInstance(SPRITE_ID, contents, atlasW, atlasH, x, y, padding);
    }

    public static void main(String[] args) throws Exception {
        initReflection();

        // Atlas dimensions: powers of two as the stitcher always produces, plus a few odd
        // sizes to exercise non-clean (non-terminating-binary) divides. All finite, physical.
        int[] atlasDims = { 16, 32, 64, 128, 256, 512, 1024, 2048, 48, 96, 160 };
        // Sprite placements and pixel sizes seen in real atlases (16/32/64 px tiles, etc.).
        int[] coords = { 0, 1, 2, 3, 8, 16, 17, 32, 48, 64, 96, 128, 200, 255, 256, 400, 511, 1000 };
        int[] paddings = { 0, 1, 2 };
        int[] spriteSizes = { 1, 2, 4, 8, 16, 24, 32, 48, 64, 100, 128 };

        // Offsets for getU/getV: full [0,1] plus a couple just outside (plain float math).
        float[] offsets = {
            0.0f, 0.0625f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 0.9375f, 1.0f,
            0.0078125f, 0.3333333f, 0.6666667f, 0.123456f, 0.987654f, -0.25f, 1.25f
        };

        // Build a curated, deterministic list of sprite placements rather than a deep
        // cartesian product (which would blow far past the row budget). Each entry emits
        // 1 BOUNDS + offsets.length GETUV rows, so the configs are kept well under ~7.8k.
        // A simple counter selects disjoint slices of each axis per config so coverage stays
        // broad while the total row count stays bounded.
        java.util.ArrayList<int[]> configs = new java.util.ArrayList<>();
        int c = 0;
        for (int ai = 0; ai < atlasDims.length; ai++) {
            int atlasW = atlasDims[ai];
            // pair each width with a square atlas, the smallest atlas, and one rect atlas.
            int[] heightsForAtlas = { atlasW, atlasDims[0], atlasDims[(ai + 3) % atlasDims.length] };
            for (int atlasH : heightsForAtlas) {
                for (int x : coords) {
                    if (x >= atlasW) continue;
                    for (int y : coords) {
                        if (y >= atlasH) continue;
                        // cycle padding and sprite size deterministically across configs
                        int padding = paddings[c % paddings.length];
                        int spriteW = spriteSizes[(c / paddings.length) % spriteSizes.length];
                        // alternate a square sprite and a rectangular one
                        int spriteH = ((c & 1) == 0) ? spriteW : (spriteW == 1 ? 2 : Math.max(1, spriteW / 2));
                        configs.add(new int[]{ atlasW, atlasH, x, y, padding, spriteW, spriteH });
                        c++;
                    }
                }
            }
        }

        long rows = 0;
        for (int[] cfg : configs) {
            int atlasW = cfg[0], atlasH = cfg[1], x = cfg[2], y = cfg[3];
            int padding = cfg[4], spriteW = cfg[5], spriteH = cfg[6];
            Object sprite = makeSprite(atlasW, atlasH, x, y, padding, spriteW, spriteH);
            float u0 = (Float) GET_U0.invoke(sprite);
            float u1 = (Float) GET_U1.invoke(sprite);
            float v0 = (Float) GET_V0.invoke(sprite);
            float v1 = (Float) GET_V1.invoke(sprite);
            O.println("BOUNDS\t" + atlasW + "\t" + atlasH + "\t" + x + "\t" + y
                + "\t" + padding + "\t" + spriteW + "\t" + spriteH
                + "\t" + f(u0) + "\t" + f(u1) + "\t" + f(v0) + "\t" + f(v1));
            rows++;
            for (float off : offsets) {
                float gu = (Float) GET_U.invoke(sprite, off);
                float gv = (Float) GET_V.invoke(sprite, off);
                O.println("GETUV\t" + atlasW + "\t" + atlasH + "\t" + x + "\t" + y
                    + "\t" + padding + "\t" + spriteW + "\t" + spriteH
                    + "\t" + f(off) + "\t" + f(gu) + "\t" + f(gv));
                rows++;
            }
        }
        System.err.println("emitted rows: " + rows + " (configs: " + configs.size() + ")");
    }
}

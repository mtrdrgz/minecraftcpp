import net.minecraft.client.renderer.Lightmap;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.Mth;
import net.minecraft.world.level.dimension.BuiltinDimensionTypes;
import net.minecraft.world.level.dimension.DimensionType;

// Ground truth for mcpp/src/render/LightTextureMath.h. Certifies the PURE brightness
// curve of net.minecraft.client.renderer.Lightmap (formerly LightTexture) in 26.1.2.
//
// Two complementary row families, all values from REAL net.minecraft code:
//
//   GETBRIGHTNESS  ambientBits(f) level | outBits(f)
//        The full public Lightmap.getBrightness(DimensionType, int) composition, run
//        per BUILT-IN dimension (overworld/the_nether/the_end/overworld_caves). The
//        dimension's REAL ambientLight() float is emitted as `ambientBits` so the C++
//        side recomputes from the identical input; `out` is the real method's return.
//        This nails the whole method end-to-end against the C++ getBrightness().
//
//   CURVE          ambientBits(f) level | outBits(f)
//        Same math over a broad sweep of ambientLight floats x integer light levels,
//        replicating the method body inline (level/15F; v/(4-3v)) but routing the final
//        blend through the REAL net.minecraft.util.Mth.lerp(float,float,float) so every
//        operation is tied to real code. Covers ambient values beyond the four vanilla
//        dimensions and the full 0..15 light range plus over/under-range levels.
//
// Floats are emitted as raw IEEE-754 bits (%08x) for bit-exact exchange.
public class LightTextureParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // Inline replica of the Lightmap.getBrightness body with the final lerp delegated to
    // the REAL Mth.lerp. ambientLight is supplied directly (as the DimensionType would).
    static float curve(float ambientLight, int level) {
        float v = level / 15.0F;
        float curvedV = v / (4.0F - 3.0F * v);
        return Mth.lerp(ambientLight, curvedV, 1.0F);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        HolderLookup.Provider provider = VanillaRegistries.createLookup();
        HolderLookup.RegistryLookup<DimensionType> dimReg = provider.lookupOrThrow(Registries.DIMENSION_TYPE);

        @SuppressWarnings("unchecked")
        ResourceKey<DimensionType>[] dims = new ResourceKey[] {
            BuiltinDimensionTypes.OVERWORLD,
            BuiltinDimensionTypes.NETHER,
            BuiltinDimensionTypes.END,
            BuiltinDimensionTypes.OVERWORLD_CAVES
        };

        // Light levels: full physical 0..15 range, plus a few out-of-range integers that
        // the curve still evaluates as plain float arithmetic (no clamping in the method).
        int[] levels = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, -1, 20, 30, 100, -5 };

        // GETBRIGHTNESS: real Lightmap.getBrightness per built-in dimension.
        for (ResourceKey<DimensionType> key : dims) {
            DimensionType dt = dimReg.getOrThrow(key).value();
            float ambient = dt.ambientLight();
            for (int level : levels) {
                float out = Lightmap.getBrightness(dt, level);
                O.println("GETBRIGHTNESS\t" + f(ambient) + "\t" + level + "\t" + f(out));
            }
        }

        // CURVE: broad ambientLight sweep (finite, physical) x levels, via real Mth.lerp.
        // Vanilla ambient lights are 0.0 (overworld/end), 0.1 (nether); include those plus
        // a representative spread of valid [0,1] blend factors and a couple just outside.
        float[] ambients = {
            0.0f, 0.05f, 0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.4f, 0.5f, 0.6f,
            0.7f, 0.75f, 0.8f, 0.9f, 0.95f, 1.0f, 0.03333333f, 0.6666667f, 0.123456f, 0.987654f
        };
        for (float ambient : ambients) {
            for (int level : levels) {
                O.println("CURVE\t" + f(ambient) + "\t" + level + "\t" + f(curve(ambient, level)));
            }
        }
    }
}

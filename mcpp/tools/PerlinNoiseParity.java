import java.util.ArrayList;
import java.util.List;
import java.util.stream.IntStream;

/**
 * Ground-truth emitter for net.minecraft.world.level.levelgen.synth.PerlinNoise.
 *
 * Constructs the REAL PerlinNoise through each public factory and dumps
 * getValue(x,y,z) plus getValue(x,y,z,yScale,yFudge) and the scalar config
 * (maxValue / maxBrokenValue) tab-separated and bit-exact:
 *   doubles -> %016x of Double.doubleToRawLongBits.
 *
 * Three construction families are exercised (matching the C++ ctors):
 *   create(random, octaveStream)                  -> useNewInitialization = true,
 *                                                    octaves seeded via forkPositional().fromHashOf("octave_N")
 *   createLegacyForBlendedNoise(random, stream)   -> useNewInitialization = false, sequential LegacyRandomSource init
 *   createLegacyForLegacyNetherBiome(random,fo,a) -> useNewInitialization = false, explicit amplitudes
 *
 * Rows:
 *   PMAX     <tag> <maxValue>                              maxValue()
 *   PMBRK    <tag> <yScale> <maxBrokenValue>               maxBrokenValue(yScale)
 *   PVAL     <tag> <x> <y> <z> <value>                     getValue(x,y,z)
 *   PVALYS   <tag> <x> <y> <z> <yScale> <yFudge> <value>   getValue(x,y,z,yScale,yFudge)
 *
 * <tag> identifies the construction recipe so the C++ side rebuilds the IDENTICAL
 * PerlinNoise (same seed, same factory, same octave/amplitude set).
 *
 * The C++ side seeds mc::levelgen::LegacyRandomSource(seed) / XoroshiroRandomSource(seed)
 * identically and recomputes.
 */
@SuppressWarnings("deprecation")
public class PerlinNoiseParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // A recipe: a human/machine tag + the constructed PerlinNoise.
    static final class Recipe {
        final String tag;
        final net.minecraft.world.level.levelgen.synth.PerlinNoise noise;
        Recipe(String tag, net.minecraft.world.level.levelgen.synth.PerlinNoise noise) {
            this.tag = tag;
            this.noise = noise;
        }
    }

    static Object newLegacy(long seed) throws Exception {
        Class<?> c = Class.forName("net.minecraft.world.level.levelgen.LegacyRandomSource");
        return c.getConstructor(long.class).newInstance(seed);
    }

    static Object newXoroshiro(long seed) throws Exception {
        Class<?> c = Class.forName("net.minecraft.world.level.levelgen.XoroshiroRandomSource");
        return c.getConstructor(long.class).newInstance(seed);
    }

    static net.minecraft.world.level.levelgen.synth.PerlinNoise createNew(Object random, int[] octaves) {
        return net.minecraft.world.level.levelgen.synth.PerlinNoise.create(
            (net.minecraft.util.RandomSource) random, IntStream.of(octaves));
    }

    static net.minecraft.world.level.levelgen.synth.PerlinNoise createLegacyBlended(Object random, int[] octaves) {
        return net.minecraft.world.level.levelgen.synth.PerlinNoise.createLegacyForBlendedNoise(
            (net.minecraft.util.RandomSource) random, IntStream.of(octaves));
    }

    static net.minecraft.world.level.levelgen.synth.PerlinNoise createLegacyNether(Object random, int firstOctave, double[] amps) {
        it.unimi.dsi.fastutil.doubles.DoubleArrayList list = new it.unimi.dsi.fastutil.doubles.DoubleArrayList(amps);
        return net.minecraft.world.level.levelgen.synth.PerlinNoise.createLegacyForLegacyNetherBiome(
            (net.minecraft.util.RandomSource) random, firstOctave, list);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        List<Recipe> recipes = new ArrayList<>();

        // ---- new-initialization recipes: create(random, IntStream.of(octaves)) ----
        // tag form: NEW:<seed>:<o0,o1,...>
        // Octave sets are contiguous (negative .. 0) because PerlinNoise's ctor throws
        // "Positive octaves are temporarily disabled" unless zeroOctaveIndex == octaves-1,
        // i.e. the largest octave is 0. makeAmplitudes sorts/dedups the set.
        long[] seeds = { 0L, 1L, -1L, 42L, 12345L, -987654321L, 9876543210123L, 0x5DEECE66DL };
        int[][] octaveSets = {
            { 0 },
            { -1, 0 },
            { -3, -2, -1, 0 },
            { -7, -6, -5, -4, -3, -2, -1, 0 },
            { -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0 },
            { -5, -3, 0 },          // sparse: zero amplitudes between non-zero octaves
            { -9, -4, -1, 0 }       // sparse, larger span
        };
        for (long seed : seeds) {
            for (int[] os : octaveSets) {
                Object rng = newXoroshiro(seed);
                net.minecraft.world.level.levelgen.synth.PerlinNoise n = createNew(rng, os);
                recipes.add(new Recipe("NEW:" + seed + ":" + joinInts(os), n));
            }
        }

        // ---- legacy-blended recipes: createLegacyForBlendedNoise(random, IntStream.of(octaves)) ----
        // These use sequential LegacyRandomSource init. BlendedNoise uses -15..0 and -7..0.
        long[] legacySeeds = { 0L, 1L, -1L, 42L, 12345L, 9876543210123L };
        int[][] legacyOctaveSets = {
            { 0 },
            { -1, 0 },
            { -7, -6, -5, -4, -3, -2, -1, 0 },
            { -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0 }
        };
        for (long seed : legacySeeds) {
            for (int[] os : legacyOctaveSets) {
                Object rng = newLegacy(seed);
                net.minecraft.world.level.levelgen.synth.PerlinNoise n = createLegacyBlended(rng, os);
                recipes.add(new Recipe("LBLEND:" + seed + ":" + joinInts(os), n));
            }
        }

        // ---- legacy-nether recipes: createLegacyForLegacyNetherBiome(random, firstOctave, amplitudes) ----
        // tag form: LNETHER:<seed>:<firstOctave>:<a0,a1,...>
        // Amplitudes are arbitrary doubles; the legacy nether biome path passes them through.
        // Constraint: zeroOctaveIndex = -firstOctave must be == octaves-1 (positive octaves disabled),
        // so firstOctave = -(amplitudes.length-1).
        Object[][] netherCases = {
            { 42L, -3, new double[]{ 1.0, 1.0, 1.0, 1.0 } },
            { 12345L, -1, new double[]{ 0.5, 1.0 } },
            { -1L, -2, new double[]{ 2.0, 0.0, 1.0 } },  // zero amplitude in the middle -> skipOctave
            { 9876543210123L, 0, new double[]{ 1.0 } },
            { 7L, -5, new double[]{ 1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125 } }
        };
        for (Object[] c : netherCases) {
            long seed = (Long) c[0];
            int firstOctave = (Integer) c[1];
            double[] amps = (double[]) c[2];
            Object rng = newLegacy(seed);
            net.minecraft.world.level.levelgen.synth.PerlinNoise n = createLegacyNether(rng, firstOctave, amps);
            recipes.add(new Recipe("LNETHER:" + seed + ":" + firstOctave + ":" + joinDoubles(amps), n));
        }

        // Coordinate sweep: integers, fractions, negatives, large magnitudes (covers wrap()),
        // exact .5, near-zero. wrap() folds around 3.3554432E7, so include values near/over it.
        double[] coords = {
            0.0, 1.0, -1.0, 0.5, -0.5, 0.25, -0.25, 0.999999, -0.999999,
            1.5, -1.5, 2.3, -2.3, 7.125, -7.125, 13.7, -13.7,
            100.0, -100.0, 255.0, 256.0, 257.0, -256.0, -257.0,
            1000.3, -1000.3, 1.0E-7, -1.0E-7, 0.123456789, -0.987654321,
            64.0, -64.0, 320.5, -64.25, 1234.5678, -8765.4321,
            3.3554432E7, -3.3554432E7, 3.3554432E7 + 17.5, 1.6777216E7, -1.6777216E7
        };

        // yScale / yFudge battery for the deprecated getValue overload (incl 0 -> short-circuit).
        double[][] ysFudge = {
            {0.0, 0.0}, {1.0, 0.0}, {1.0, 0.5}, {0.5, 0.25}, {2.0, 1.0},
            {0.7, -0.3}, {0.7, 1.5}, {684.412, 0.0}, {-1.0, 0.5}, {0.333333, 0.111111}
        };

        for (Recipe r : recipes) {
            // maxValue()/maxBrokenValue() are PROTECTED accessors (not the noise eval) —
            // not certified here; the C++ test simply receives no PMAX/PMBRK rows.

            // getValue(x,y,z) over the full coordinate cube.
            for (double x : coords) {
                for (double y : coords) {
                    for (double z : coords) {
                        double v = r.noise.getValue(x, y, z);
                        O.println("PVAL\t" + r.tag + "\t" + d(x) + "\t" + d(y) + "\t" + d(z) + "\t" + d(v));
                    }
                }
            }

            // getValue(x,y,z,yScale,yFudge) over a representative coord subset x the yScale/yFudge battery.
            double[] xc = { -100.0, -2.3, 0.5, 7.125, 256.0, 3.3554432E7 };
            double[] yc = { -64.25, -1.5, 0.0, 0.25, 64.0, 320.5 };
            double[] zc = { -257.0, 0.0, 13.7, 1234.5678 };
            for (double[] yf : ysFudge) {
                for (double x : xc) {
                    for (double y : yc) {
                        for (double z : zc) {
                            double v = r.noise.getValue(x, y, z, yf[0], yf[1]);
                            O.println("PVALYS\t" + r.tag + "\t" + d(x) + "\t" + d(y) + "\t" + d(z)
                                + "\t" + d(yf[0]) + "\t" + d(yf[1]) + "\t" + d(v));
                        }
                    }
                }
            }
        }
    }

    static String joinInts(int[] a) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < a.length; i++) {
            if (i > 0) sb.append(',');
            sb.append(a[i]);
        }
        return sb.toString();
    }

    static String joinDoubles(double[] a) {
        // Emit raw long bits so the C++ side reconstructs the EXACT double.
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < a.length; i++) {
            if (i > 0) sb.append(',');
            sb.append(String.format("%016x", Double.doubleToRawLongBits(a[i])));
        }
        return sb.toString();
    }
}

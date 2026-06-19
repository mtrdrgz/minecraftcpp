import it.unimi.dsi.fastutil.doubles.DoubleArrayList;

/**
 * Ground-truth emitter for net.minecraft.world.level.levelgen.synth.NormalNoise.
 *
 * NormalNoise.create(random, NoiseParameters(firstOctave, amplitudes)) builds two
 * PerlinNoise from the SAME random (consumed in sequence) and combines them with a
 * value factor 0.16666.../expectedDeviation(octaveSpan). getValue(x,y,z) =
 * (first.getValue(x,y,z) + second.getValue(x*F,y*F,z*F)) * valueFactor, F=1.0181268882175227.
 *
 * This tool constructs the REAL NormalNoise seeded with a REAL RandomSource
 * (LegacyRandomSource or XoroshiroRandomSource) and dumps, tab-separated and
 * bit-exact (doubles -> %016x of Double.doubleToRawLongBits):
 *
 *   MAX   <cfgId> <rngKind> <seed> <maxValue>
 *   VAL   <cfgId> <rngKind> <seed> <x> <y> <z> <getValue>
 *
 * cfgId selects a NoiseParameters config from the table below; the C++ side mirrors
 * the identical table, seeds the identical RandomSource, builds the real engine
 * NormalNoise, and recomputes BIT-FOR-BIT.
 *
 * rngKind: 0 = LegacyRandomSource(seed), 1 = XoroshiroRandomSource(seed).
 */
@SuppressWarnings("deprecation")
public class NormalNoiseParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // NoiseParameters configs: { firstOctave, amplitude0, amplitude1, ... }.
    // These mirror real datapack noise params shapes (single octave, multi-octave,
    // negative firstOctave, interior zero amplitudes) without inventing values —
    // they are just the (firstOctave, amplitudes[]) inputs the public ctor accepts.
    static final double[][] CONFIGS = {
        { 0.0, 1.0 },                                  // cfg 0: single octave, like flower noise
        { -1.0, 1.0, 1.0 },                            // cfg 1
        { -3.0, 1.0, 1.0, 1.0, 1.0 },                  // cfg 2
        { -7.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 }, // cfg 3: 8 octaves
        { -2.0, 1.0, 0.0, 1.0 },                       // cfg 4: interior zero amplitude
        { -5.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0 },        // cfg 5: zero gap in the middle
        { 3.0, 1.0 },                                  // cfg 6: positive firstOctave
        { -4.0, 1.5, 0.5, 2.0, 0.25 },                 // cfg 7: non-unit amplitudes
        { 0.0, 0.0, 1.0 },                             // cfg 8: leading zero amplitude
    };

    static int firstOctave(int cfg) { return (int) CONFIGS[cfg][0]; }

    static DoubleArrayList amplitudes(int cfg) {
        double[] c = CONFIGS[cfg];
        double[] a = new double[c.length - 1];
        System.arraycopy(c, 1, a, 0, a.length);
        return new DoubleArrayList(a);
    }

    static Object newRng(int kind, long seed) throws Exception {
        String cls = kind == 0
            ? "net.minecraft.world.level.levelgen.LegacyRandomSource"
            : "net.minecraft.world.level.levelgen.XoroshiroRandomSource";
        return Class.forName(cls).getConstructor(long.class).newInstance(seed);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> nnCls = Class.forName("net.minecraft.world.level.levelgen.synth.NormalNoise");
        Class<?> npCls = Class.forName("net.minecraft.world.level.levelgen.synth.NormalNoise$NoiseParameters");
        Class<?> rsCls = Class.forName("net.minecraft.util.RandomSource");

        java.lang.reflect.Constructor<?> npCtor =
            npCls.getConstructor(int.class, it.unimi.dsi.fastutil.doubles.DoubleList.class);
        java.lang.reflect.Method create =
            nnCls.getMethod("create", rsCls, npCls);
        java.lang.reflect.Method getValue =
            nnCls.getMethod("getValue", double.class, double.class, double.class);
        java.lang.reflect.Method maxValue =
            nnCls.getMethod("maxValue");

        // Seeds: zero, small, negative, the legacy multiplier, large magnitudes.
        long[] seeds = { 0L, 1L, 42L, 2345L, -987654321L, 9876543210123L, 0x5DEECE66DL };

        // Coordinate battery: integers, fractions, negatives, exact half, large blocks
        // scaled like real worldgen (e.g. * 0.005) and raw block coords.
        double[] coords = {
            0.0, 1.0, -1.0, 0.5, -0.5, 0.25, -0.25,
            1.5, -1.5, 7.125, -7.125, 13.7, -13.7,
            64.0, -64.0, 100.0, -100.0, 256.0, -256.0,
            0.005, -0.005, 0.32, -0.16, 5.0, 1000.3, -1000.3, 12345.678
        };

        // To keep the TSV bounded but thorough we sweep the full coord cube only for a
        // representative (cfg,rng,seed) trio, then sweep all (cfg,rng,seed) over a
        // smaller probe set. Every config/seed/rng is still exercised on getValue + maxValue.
        double[] probeX = { 0.0, 1.0, -1.0, 0.5, 7.125, -13.7, 64.0, 100.0, 0.32, 1000.3 };
        double[] probeY = { 0.0, 1.0, -0.5, 13.7, -64.0, 256.0, 0.005 };
        double[] probeZ = { 0.0, -1.0, 0.25, -7.125, 100.0, -256.0, 12345.678 };

        for (int cfg = 0; cfg < CONFIGS.length; cfg++) {
            for (int rng = 0; rng <= 1; rng++) {
                for (long seed : seeds) {
                    Object rs = newRng(rng, seed);
                    Object np = npCtor.newInstance(firstOctave(cfg), amplitudes(cfg));
                    Object noise = create.invoke(null, rs, np);

                    double mv = (Double) maxValue.invoke(noise);
                    O.println("MAX\t" + cfg + "\t" + rng + "\t" + seed + "\t" + d(mv));

                    for (double x : probeX) {
                        for (double y : probeY) {
                            for (double z : probeZ) {
                                double v = (Double) getValue.invoke(noise, x, y, z);
                                O.println("VAL\t" + cfg + "\t" + rng + "\t" + seed
                                    + "\t" + d(x) + "\t" + d(y) + "\t" + d(z) + "\t" + d(v));
                            }
                        }
                    }
                }
            }
        }

        // Dense full-coord cube for one representative trio (cfg 3 = 8 octaves, legacy rng, seed 2345).
        {
            int cfg = 3, rng = 0;
            long seed = 2345L;
            Object rs = newRng(rng, seed);
            Object np = npCtor.newInstance(firstOctave(cfg), amplitudes(cfg));
            Object noise = create.invoke(null, rs, np);
            for (double x : coords) {
                for (double y : coords) {
                    for (double z : coords) {
                        double v = (Double) getValue.invoke(noise, x, y, z);
                        O.println("VAL\t" + cfg + "\t" + rng + "\t" + seed
                            + "\t" + d(x) + "\t" + d(y) + "\t" + d(z) + "\t" + d(v));
                    }
                }
            }
        }
    }
}

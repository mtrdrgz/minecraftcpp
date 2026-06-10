import java.lang.reflect.Constructor;

/**
 * Ground-truth emitter for net.minecraft.world.level.levelgen.synth.BlendedNoise
 * (the main terrain density noise: minLimit/maxLimit/mainNoise blend).
 *
 * Constructs the REAL BlendedNoise via its @VisibleForTesting public constructor
 *   BlendedNoise(RandomSource, double xzScale, double yScale, double xzFactor,
 *                double yFactor, double smearScaleMultiplier)
 * with a REAL net.minecraft XoroshiroRandomSource(seed), and dumps, tab-separated
 * and bit-exact (doubles -> %016x of Double.doubleToRawLongBits):
 *
 *   MAX     <cfgIdx> <maxValue>            BlendedNoise.maxValue()
 *   MIN     <cfgIdx> <minValue>            BlendedNoise.minValue()
 *   COMPUTE <cfgIdx> <blockX> <blockY> <blockZ> <value>   compute(SinglePointContext)
 *
 * cfgIdx selects one of the (seed, xzScale, yScale, xzFactor, yFactor, smearScaleMultiplier)
 * configs in the SEEDS/PARAMS tables below. blockX/blockY/blockZ are decimal ints (real block
 * coords). The C++ side seeds mc::levelgen::XoroshiroRandomSource(seed) identically,
 * builds the same BlendedNoise, and recomputes.
 *
 * Build via createUnseeded? No — createUnseeded fixes seed 0; we exercise multiple seeds
 * and all five scale/factor params through the public @VisibleForTesting ctor.
 */
@SuppressWarnings("deprecation")
public class BlendedNoiseParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // Each config: { seed, xzScale, yScale, xzFactor, yFactor, smearScaleMultiplier }.
    // Scales/factors are clamped by codec to [0.001,1000.0] and smear to [1.0,8.0] in
    // real data; we stay within those physical ranges. The default overworld values
    // are xzScale=yScale=1.0, xzFactor=yFactor=80.0, smear=8.0 (NoiseRouterData).
    // seed is carried separately (as a long) to avoid any double<->long rounding;
    // the params array holds the five scale/factor doubles.
    static final long[] SEEDS = {
        0L, 1L, 42L, 12345L, -987654321L, 9876543210123L, 7L, 7L, 100000L, 0x5DEECE66DL
    };
    static final double[][] PARAMS = {
        // xzScale, yScale, xzFactor, yFactor, smear
        { 1.0,  1.0,  80.0,  80.0,  8.0 }, // overworld default (NoiseRouterData)
        { 1.0,  1.0,  80.0,  80.0,  8.0 },
        { 1.0,  1.0,  80.0,  80.0,  8.0 },
        { 1.0,  1.0,  80.0,  80.0,  8.0 },
        { 1.0,  1.0,  80.0,  80.0,  8.0 },
        { 1.0,  1.0,  80.0,  80.0,  8.0 },
        { 0.25, 0.5,  40.0,  20.0,  4.0 },
        { 2.0,  3.0, 120.0,  90.0,  1.0 },
        { 0.5,  0.5,  60.0,  60.0,  6.0 },
        { 1.0,  1.0,  80.0,  80.0,  8.0 },
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> bnCls = Class.forName("net.minecraft.world.level.levelgen.synth.BlendedNoise");
        Class<?> rsCls = Class.forName("net.minecraft.util.RandomSource");
        Class<?> xoroCls = Class.forName("net.minecraft.world.level.levelgen.XoroshiroRandomSource");
        Class<?> ctxIfaceCls = Class.forName("net.minecraft.world.level.levelgen.DensityFunction$FunctionContext");
        Class<?> spCtxCls = Class.forName("net.minecraft.world.level.levelgen.DensityFunction$SinglePointContext");

        Constructor<?> bnCtor = bnCls.getConstructor(
            rsCls, double.class, double.class, double.class, double.class, double.class);
        Constructor<?> xoroCtor = xoroCls.getConstructor(long.class);
        Constructor<?> spCtor = spCtxCls.getConstructor(int.class, int.class, int.class);

        java.lang.reflect.Method computeM = bnCls.getMethod("compute", ctxIfaceCls);
        java.lang.reflect.Method maxM = bnCls.getMethod("maxValue");
        java.lang.reflect.Method minM = bnCls.getMethod("minValue");

        // Block-coordinate sweep: zeros, positives, negatives, large magnitudes, and the
        // y-range vanilla actually generates (roughly -64..320). Finite physical ints only.
        int[] xs = { 0, 1, -1, 4, 16, 64, -64, 128, -300, 1000, -2048, 30000, -30000 };
        int[] ys = { 0, 1, -1, -64, -32, 16, 64, 128, 256, 320, 384 };
        int[] zs = { 0, 1, -1, 4, 16, 64, -64, 128, -300, 1000, -2048, 30000, -30000 };

        for (int cfg = 0; cfg < SEEDS.length; cfg++) {
            long seed = SEEDS[cfg];
            double[] c = PARAMS[cfg];
            double xzScale = c[0], yScale = c[1], xzFactor = c[2], yFactor = c[3], smear = c[4];

            Object rng = xoroCtor.newInstance(seed);
            Object bn = bnCtor.newInstance(rng, xzScale, yScale, xzFactor, yFactor, smear);

            double maxV = (Double) maxM.invoke(bn);
            double minV = (Double) minM.invoke(bn);
            O.println("MAX\t" + cfg + "\t" + d(maxV));
            O.println("MIN\t" + cfg + "\t" + d(minV));

            for (int x : xs) {
                for (int y : ys) {
                    for (int z : zs) {
                        Object ctx = spCtor.newInstance(x, y, z);
                        double v = (Double) computeM.invoke(bn, ctx);
                        O.println("COMPUTE\t" + cfg + "\t" + x + "\t" + y + "\t" + z + "\t" + d(v));
                    }
                }
            }
        }
    }
}

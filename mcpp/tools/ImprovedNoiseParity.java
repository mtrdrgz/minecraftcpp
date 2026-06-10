import java.lang.reflect.Field;

/**
 * Ground-truth emitter for net.minecraft.world.level.levelgen.synth.ImprovedNoise.
 *
 * Constructs the REAL ImprovedNoise with a REAL seeded LegacyRandomSource and dumps,
 * tab-separated and bit-exact (doubles -> %016x of Double.doubleToRawLongBits):
 *   CTOR    <seed> <xo> <yo> <zo> <p0..p255 decimal>      one row per seed
 *   NOISE   <seed> <x> <y> <z> <value>                    noise(x,y,z)
 *   NOISEYS <seed> <x> <y> <z> <yScale> <yFudge> <value>  noise(x,y,z,yScale,yFudge)
 *   NWD     <seed> <x> <y> <z> <value> <dx> <dy> <dz>     noiseWithDerivative (derivativeOut seeded 0)
 *
 * The C++ side seeds mc::levelgen::LegacyRandomSource(seed) identically and recomputes.
 */
@SuppressWarnings("deprecation")
public class ImprovedNoiseParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> inCls = Class.forName("net.minecraft.world.level.levelgen.synth.ImprovedNoise");

        Field fXo = inCls.getField("xo");
        Field fYo = inCls.getField("yo");
        Field fZo = inCls.getField("zo");
        Field fP = inCls.getDeclaredField("p");
        fP.setAccessible(true);

        // Coordinate sweep: integers, fractions, negatives, large magnitudes, exact .5, near-zero.
        double[] coords = {
            0.0, 1.0, -1.0, 0.5, -0.5, 0.25, -0.25, 0.999999, -0.999999,
            1.5, -1.5, 2.3, -2.3, 7.125, -7.125, 13.7, -13.7,
            100.0, -100.0, 255.0, 256.0, 257.0, -256.0, -257.0,
            1000.3, -1000.3, 1.0E-7, -1.0E-7, 0.123456789, -0.987654321,
            64.0, -64.0, 320.5, -64.25, 1234.5678, -8765.4321
        };

        // yScale / yFudge battery for the deprecated noise overload (incl 0 -> short-circuit).
        double[][] ysFudge = {
            {0.0, 0.0}, {1.0, 0.0}, {1.0, 0.5}, {0.5, 0.25}, {2.0, 1.0},
            {0.7, -0.3}, {0.7, 1.5}, {0.125, 0.0625}, {-1.0, 0.5}, {3.0, 0.0},
            {0.7, 0.7}, {1.0, 0.999999}, {0.333333, 0.111111}
        };

        long[] seeds = { 0L, 1L, -1L, 42L, 12345L, -987654321L, 9876543210123L, 0x5DEECE66DL, Long.MIN_VALUE, Long.MAX_VALUE };

        for (long seed : seeds) {
            Object rng = newLegacy(seed);
            Object noise = inCls.getConstructor(Class.forName("net.minecraft.util.RandomSource")).newInstance(rng);

            double xo = fXo.getDouble(noise);
            double yo = fYo.getDouble(noise);
            double zo = fZo.getDouble(noise);
            byte[] p = (byte[]) fP.get(noise);

            StringBuilder ctor = new StringBuilder();
            ctor.append("CTOR\t").append(seed).append('\t').append(d(xo)).append('\t').append(d(yo)).append('\t').append(d(zo));
            for (int i = 0; i < p.length; i++) {
                ctor.append('\t').append(p[i] & 0xFF); // unsigned byte, decimal
            }
            O.println(ctor.toString());

            // noise(x,y,z)
            for (double x : coords) {
                for (double y : coords) {
                    for (double z : coords) {
                        // thin the cube to keep TSV bounded but still exhaustive across edge combos
                        double v = ((net.minecraft.world.level.levelgen.synth.ImprovedNoise) noise).noise(x, y, z);
                        O.println("NOISE\t" + seed + "\t" + d(x) + "\t" + d(y) + "\t" + d(z) + "\t" + d(v));
                    }
                }
            }

            // noise(x,y,z,yScale,yFudge) — sweep a representative coord set against the yScale/yFudge battery.
            double[] yc = { -13.7, -1.5, -0.5, 0.0, 0.25, 0.999999, 1.5, 64.0, 320.5 };
            double[] xc = { -100.0, -2.3, 0.5, 7.125, 256.0 };
            double[] zc = { -64.25, 0.0, 13.7, 257.0 };
            for (double[] yf : ysFudge) {
                for (double x : xc) {
                    for (double y : yc) {
                        for (double z : zc) {
                            double v = ((net.minecraft.world.level.levelgen.synth.ImprovedNoise) noise)
                                .noise(x, y, z, yf[0], yf[1]);
                            O.println("NOISEYS\t" + seed + "\t" + d(x) + "\t" + d(y) + "\t" + d(z)
                                + "\t" + d(yf[0]) + "\t" + d(yf[1]) + "\t" + d(v));
                        }
                    }
                }
            }

            // noiseWithDerivative — derivativeOut starts at {0,0,0}; method ACCUMULATES into it.
            for (double x : coords) {
                for (double y : coords) {
                    for (double z : coords) {
                        double[] der = new double[3];
                        double v = ((net.minecraft.world.level.levelgen.synth.ImprovedNoise) noise)
                            .noiseWithDerivative(x, y, z, der);
                        O.println("NWD\t" + seed + "\t" + d(x) + "\t" + d(y) + "\t" + d(z)
                            + "\t" + d(v) + "\t" + d(der[0]) + "\t" + d(der[1]) + "\t" + d(der[2]));
                    }
                }
            }
        }
    }

    static Object newLegacy(long seed) throws Exception {
        Class<?> c = Class.forName("net.minecraft.world.level.levelgen.LegacyRandomSource");
        return c.getConstructor(long.class).newInstance(seed);
    }
}

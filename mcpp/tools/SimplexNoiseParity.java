// Reference value generator for the C++ mc::levelgen::SimplexNoise port. Runs the
// REAL decompiled net.minecraft.world.level.levelgen.synth.SimplexNoise from the
// jar so the emitted values are exact ground truth.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/SimplexNoiseParity.java
//   java  -cp <out>;26.1.2/client.jar SimplexNoiseParity > simplex_noise.tsv
//
// The SimplexNoise ctor consumes a RandomSource (3 nextDouble for xo/yo/zo then a
// 256-entry Fisher-Yates shuffle via nextInt). We build it with a REAL seeded
// net.minecraft LegacyRandomSource(seed); the C++ side seeds its certified
// mc::levelgen::LegacyRandomSource IDENTICALLY, so the permutation table matches.
//
// Rows:
//   CTOR\t<seed>\t<xoBits>\t<yoBits>\t<zoBits>
//   N2\t<seed>\t<xBits>\t<yBits>\t<valueBits>
//   N3\t<seed>\t<xBits>\t<yBits>\t<zBits>\t<valueBits>
// All doubles emitted as raw IEEE bits (16 hex) so the C++ comparison is exact.
import java.io.PrintStream;
import java.lang.reflect.Field;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.synth.SimplexNoise;

public class SimplexNoiseParity {
    static final PrintStream O = System.out;

    static String d(double v) {
        return String.format("%016x", Double.doubleToRawLongBits(v));
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Reflect xo/yo/zo (public, but read via reflection to be robust to access).
        Field fxo = SimplexNoise.class.getField("xo");
        Field fyo = SimplexNoise.class.getField("yo");
        Field fzo = SimplexNoise.class.getField("zo");
        fxo.setAccessible(true);
        fyo.setAccessible(true);
        fzo.setAccessible(true);

        long[] seeds = { 0L, 1L, 42L, 123456789L, -987654321L, 2147483647L,
                         -1234567890123456789L, 1234567890123456789L };

        // A coordinate sweep: integers, fractionals, negatives, near-zero, and
        // larger magnitudes that exercise the i&0xFF wrap and the skew/unskew.
        double[] coords = {
            0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5, 2.25, -2.25,
            0.1, -0.1, 0.333333333333, -0.333333333333,
            3.7, -3.7, 7.0, -7.0, 10.123, -10.123,
            16.0, -16.0, 31.999, -31.999, 64.5, -64.5,
            100.25, -100.25, 255.5, -255.5, 256.0, -256.0,
            257.75, -257.75, 511.125, -511.125, 1000.0, -1000.0,
            12345.6789, -12345.6789, 1.0E-7, -1.0E-7
        };

        for (long seed : seeds) {
            SimplexNoise noise = new SimplexNoise(new LegacyRandomSource(seed));
            double xo = fxo.getDouble(noise);
            double yo = fyo.getDouble(noise);
            double zo = fzo.getDouble(noise);
            O.println("CTOR\t" + seed + "\t" + d(xo) + "\t" + d(yo) + "\t" + d(zo));

            // 2D sweep (all pairs from a representative subset to keep size sane).
            for (double x : coords) {
                for (double y : coords) {
                    double v = noise.getValue(x, y);
                    O.println("N2\t" + seed + "\t" + d(x) + "\t" + d(y) + "\t" + d(v));
                }
            }

            // 3D sweep over a coarser grid (full cube of coords would be huge).
            double[] c3 = {
                0.0, -0.0, 0.5, -0.5, 1.5, -1.5, 2.25, -2.25,
                3.7, -3.7, 10.123, -10.123, 16.0, -16.0,
                64.5, -64.5, 255.5, -255.5, 256.0, 257.75,
                1000.0, -1000.0, 0.333333333333, 12345.6789
            };
            for (double x : c3) {
                for (double y : c3) {
                    for (double z : c3) {
                        double v = noise.getValue(x, y, z);
                        O.println("N3\t" + seed + "\t" + d(x) + "\t" + d(y) + "\t" + d(z) + "\t" + d(v));
                    }
                }
            }
        }
    }
}

// Reference value generator for the C++ mc::levelgen::PerlinSimplexNoise port. Runs
// the REAL decompiled net.minecraft.world.level.levelgen.synth.PerlinSimplexNoise from
// the jar so the emitted values are exact ground truth.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/PerlinSimplexNoiseParity.java
//   java  -cp <out>;26.1.2/client.jar PerlinSimplexNoiseParity > perlin_simplex_noise.tsv
//
// PerlinSimplexNoise(RandomSource, List<Integer> octaveSet) builds one octave
// SimplexNoise per octave value. Each SimplexNoise ctor consumes the RandomSource
// (3 nextDouble for xo/yo/zo then a 256-entry Fisher-Yates shuffle), and unused
// octaves consume 262 RNG rounds. The high-freq octaves (>0) are reseeded from a
// second WorldgenRandom(LegacyRandomSource(seed-derived-from-zeroOctave)). To match
// the real construction site (Biome.java), we wrap a LegacyRandomSource in a
// WorldgenRandom exactly as vanilla does; the C++ side seeds an identically wrapped
// mc::levelgen::WorldgenRandom(LegacyRandomSource(seed)).
//
// Octave sets cover the real vanilla configurations:
//   {0}           TEMPERATURE_NOISE / BIOME_INFO_NOISE (single octave)
//   {-2,-1,0}     FROZEN_TEMPERATURE_NOISE (3 octaves, low freq)
//   plus a few mixed-frequency sets to exercise the high-freq reseed path and gaps.
//
// Rows:
//   CFG\t<cfgIdx>\t<seed>\t<octaveCSV>            (records the octave set for a cfg)
//   PV\t<cfgIdx>\t<seed>\t<octaveCSV>\t<xBits>\t<yBits>\t<useStart>\t<valueBits>
// All doubles emitted as raw IEEE bits (16 hex) so the C++ comparison is exact.
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.synth.PerlinSimplexNoise;

public class PerlinSimplexNoiseParity {
    static final PrintStream O = System.out;

    static String d(double v) {
        return String.format("%016x", Double.doubleToRawLongBits(v));
    }

    static String csv(List<Integer> octaves) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < octaves.size(); i++) {
            if (i > 0) sb.append(',');
            sb.append(octaves.get(i));
        }
        return sb.toString();
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Each config: a seed and an octave set (List<Integer> as vanilla passes).
        long[] seeds = { 1234L, 3456L, 2345L, 0L, 42L, -987654321L, 1234567890123456789L };
        List<List<Integer>> octaveSets = new ArrayList<>();
        octaveSets.add(List.of(0));               // single zero octave (vanilla)
        octaveSets.add(List.of(-2, -1, 0));       // low-freq 3 octaves (vanilla)
        octaveSets.add(List.of(0, 1, 2));         // high-freq path (>0) reseed
        octaveSets.add(List.of(-1, 0, 1));        // straddles zero, both paths
        octaveSets.add(List.of(-3, 0, 2));        // gaps -> consumeCount(262) branches
        octaveSets.add(List.of(2));               // single positive octave only

        // Coordinate sweep: integers, fractionals, negatives, near-zero, larger
        // magnitudes that exercise the per-octave input factor scaling and skew.
        double[] coords = {
            0.0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5, 2.25, -2.25,
            0.1, -0.1, 0.333333333333, -0.333333333333,
            3.7, -3.7, 7.0, -7.0, 10.123, -10.123,
            16.0, -16.0, 31.999, -31.999, 64.5, -64.5,
            100.25, -100.25, 255.5, -255.5, 256.0, -256.0,
            257.75, -257.75, 511.125, -511.125, 1000.0, -1000.0,
            12345.6789, -12345.6789, 1.0E-7, -1.0E-7
        };

        int cfgIdx = 0;
        for (long seed : seeds) {
            for (List<Integer> octaves : octaveSets) {
                String oc = csv(octaves);
                O.println("CFG\t" + cfgIdx + "\t" + seed + "\t" + oc);

                // Match the real vanilla construction site (Biome.java).
                RandomSource random = new WorldgenRandom(new LegacyRandomSource(seed));
                PerlinSimplexNoise noise = new PerlinSimplexNoise(random, octaves);

                for (double x : coords) {
                    for (double y : coords) {
                        for (int useStart = 0; useStart <= 1; useStart++) {
                            boolean us = useStart != 0;
                            double v = noise.getValue(x, y, us);
                            O.println("PV\t" + cfgIdx + "\t" + seed + "\t" + oc + "\t"
                                + d(x) + "\t" + d(y) + "\t" + useStart + "\t" + d(v));
                        }
                    }
                }
                cfgIdx++;
            }
        }
    }
}

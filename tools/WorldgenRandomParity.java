// Reference value generator for the C++ WorldgenRandom port. Runs the REAL
// decompiled net.minecraft.world.level.levelgen.WorldgenRandom from client.jar
// so the emitted sequences are exact ground truth.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/WorldgenRandomParity.java
//   java  -cp <out>:26.1.2/client.jar WorldgenRandomParity > worldgen_random_cases.tsv
//
// Floats/doubles are emitted as raw IEEE bits so the C++ comparison is exact.
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;

public class WorldgenRandomParity {
    // A fixed probe sequence exercising next(32/31/24/26/27/1) and nextLong.
    static String probe(WorldgenRandom r) {
        long l1 = r.nextLong();
        int i1 = r.nextInt();
        int i2 = r.nextInt(16);   // power of two
        int i3 = r.nextInt(100);  // non power of two
        boolean b1 = r.nextBoolean();
        int fbits = Float.floatToRawIntBits(r.nextFloat());
        long dbits = Double.doubleToRawLongBits(r.nextDouble());
        long l2 = r.nextLong();
        return l1 + "\t" + i1 + "\t" + i2 + "\t" + i3 + "\t" + (b1 ? 1 : 0) + "\t" + fbits + "\t" + dbits + "\t" + l2;
    }

    static WorldgenRandom xoro() {
        return new WorldgenRandom(new XoroshiroRandomSource(0L));
    }

    static WorldgenRandom legacy() {
        return new WorldgenRandom(new LegacyRandomSource(0L));
    }

    public static void main(String[] args) {
        long[] seeds = { 0L, 1L, 123456789L, -987654321L, 2147483647L, -1234567890123456789L };
        int[][] origins = { {0, 0}, {16, 16}, {-32, 48}, {1000, -2000}, {384, -384} };
        int[] indices = { 0, 1, 5, 37, 200 };
        int[] steps = { 0, 7, 10 };

        for (long seed : seeds) {
            for (int[] o : origins) {
                WorldgenRandom r = xoro();
                long ds = r.setDecorationSeed(seed, o[0], o[1]);
                System.out.println("DEC\t" + seed + "\t" + o[0] + "\t" + o[1] + "\t" + ds + "\t" + probe(r));

                for (int idx : indices) {
                    for (int step : steps) {
                        WorldgenRandom rf = xoro();
                        long ds2 = rf.setDecorationSeed(seed, o[0], o[1]);
                        rf.setFeatureSeed(ds2, idx, step);
                        System.out.println("FEAT\t" + seed + "\t" + o[0] + "\t" + o[1] + "\t" + idx + "\t" + step + "\t" + probe(rf));
                    }
                }

                // Legacy-wrapped (exercises the instanceof LegacyRandomSource fast path)
                // with the large-feature structure seeds.
                WorldgenRandom rl = legacy();
                rl.setLargeFeatureSeed(seed, o[0], o[1]);
                System.out.println("LARGE\t" + seed + "\t" + o[0] + "\t" + o[1] + "\t" + probe(rl));

                WorldgenRandom rs = legacy();
                rs.setLargeFeatureWithSalt(seed, o[0], o[1], 12345);
                System.out.println("SALT\t" + seed + "\t" + o[0] + "\t" + o[1] + "\t12345\t" + probe(rs));
            }
        }
    }
}

// Ground-truth generator for net.minecraft.util.LinearCongruentialGenerator —
// the positional-random LCG (next(rval, c) + MULTIPLIER/INCREMENT) used by
// BiomeManager's biome-zoom fiddle. Pure 64-bit long arithmetic; no Bootstrap
// needed. Dumps the two private constants via reflection (to verify the embedded
// C++ constants) and exercises next() over a thorough physical input battery.
// All longs printed decimal.
//
//   tools/run_groundtruth.ps1 -Tool LcgParity -Out mcpp/build/lcg.tsv
//
// (or: javac -cp <jar> LcgParity.java && java -cp <jar>;. LcgParity > lcg.tsv)

import java.lang.reflect.Field;
import java.util.ArrayList;
import net.minecraft.util.LinearCongruentialGenerator;

public class LcgParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Dump the private static final constants for the C++ to verify against.
        Field mf = LinearCongruentialGenerator.class.getDeclaredField("MULTIPLIER");
        mf.setAccessible(true);
        Field cf = LinearCongruentialGenerator.class.getDeclaredField("INCREMENT");
        cf.setAccessible(true);
        long multiplier = mf.getLong(null);
        long increment  = cf.getLong(null);
        O.println("MULTIPLIER\t" + multiplier);
        O.println("INCREMENT\t" + increment);

        // Build a finite, physical battery of seeds and salts. These cover:
        //  * the constants and the seed/c values BiomeManager actually feeds in
        //    (a 64-bit biome-zoom seed; salts that are small ints / a full seed),
        //  * small magnitudes around 0, powers of two, and large 64-bit magnitudes,
        //  * the chained-call results (next applied repeatedly) so carry/overflow
        //    propagation is checked exactly like the real fiddle pipeline.
        ArrayList<Long> seeds = new ArrayList<>();
        long[] base = {
            0L, 1L, -1L, 2L, -2L, 3L, 7L, 8L, 16L, 31L, 32L, 255L, 256L, 1023L, 1024L,
            -255L, -256L, -1024L,
            1000L, -1000L, 1000000L, -1000000L,
            0x7FFFFFFFL, -0x80000000L, 0xFFFFFFFFL,
            123456789L, -123456789L, 987654321L,
            1442695040888963407L, 6364136223846793005L, -6364136223846793005L,
            0x0123456789ABCDEFL, 0xFEDCBA9876543210L,
            Long.MAX_VALUE, Long.MIN_VALUE, Long.MAX_VALUE - 1L, Long.MIN_VALUE + 1L,
            // a few realistic world seeds (the biomeZoomSeed BiomeManager.obfuscateSeed makes)
            -4172144997902289642L, 2745086978932756360L, 7240161429170141402L
        };
        for (long b : base) seeds.add(b);

        // Salts BiomeManager passes: cornerX/Y/Z (small ints) and the full seed itself.
        long[] salts = {
            0L, 1L, -1L, 2L, -2L, 4L, -4L, 16L, -16L, 100L, -100L,
            1000000L, -1000000L,
            0x7FFFFFFFL, -0x80000000L,
            1442695040888963407L, 6364136223846793005L,
            Long.MAX_VALUE, Long.MIN_VALUE,
            -4172144997902289642L, 2745086978932756360L
        };

        // Direct single-call coverage: NEXT  seed  c  result
        for (long s : seeds) {
            for (long c : salts) {
                O.println("NEXT\t" + s + "\t" + c + "\t"
                          + LinearCongruentialGenerator.next(s, c));
            }
        }

        // Chained coverage: replays the exact BiomeManager.getFiddledDistance call
        // shape (6x next with x,y,z salts, then 2x next with the seed salt) so the
        // overflow/carry chaining is certified end-to-end.
        //   CHAIN  seed  x  y  z  v6  v7  v8   (v6 = after 6 calls, v7/v8 = the two seed-salted)
        long[][] xyz = {
            {0L, 0L, 0L}, {1L, 2L, 3L}, {-1L, -2L, -3L}, {100L, -50L, 7L},
            {0x7FFFFFFFL, -0x80000000L, 12345L}, {-987654321L, 123456789L, -42L}
        };
        for (long s : seeds) {
            for (long[] q : xyz) {
                long x = q[0], y = q[1], z = q[2];
                long v = s;
                v = LinearCongruentialGenerator.next(v, x);
                v = LinearCongruentialGenerator.next(v, y);
                v = LinearCongruentialGenerator.next(v, z);
                v = LinearCongruentialGenerator.next(v, x);
                v = LinearCongruentialGenerator.next(v, y);
                v = LinearCongruentialGenerator.next(v, z);
                long v6 = v;
                v = LinearCongruentialGenerator.next(v, s);
                long v7 = v;
                v = LinearCongruentialGenerator.next(v, s);
                long v8 = v;
                O.println("CHAIN\t" + s + "\t" + x + "\t" + y + "\t" + z
                          + "\t" + v6 + "\t" + v7 + "\t" + v8);
            }
        }
    }
}

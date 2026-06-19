// Ground-truth generator for com.mojang.math.Divisor (Minecraft 26.1.2) using
// the REAL decompiled class. Divisor is an it.unimi.dsi.fastutil IntIterator that
// splits `numerator` into `denominator` segment sizes; we drive it with hasNext()
// /nextInt() and emit the entire produced sequence per (numerator, denominator).
//
//   tools/run_groundtruth.ps1 -Tool DivisorParity -Out mcpp/build/divisor.tsv
//
// TSV row format (tab-separated), dispatched by leading TAG in the C++ test:
//   SEQ  <numerator>  <denominator>  <count>  <v0> <v1> ... <v_{count-1}>
// All values are decimal ints. <count> equals max(denominator,0) (the number of
// times hasNext() was true). Divisor is pure integer arithmetic; no registries,
// so no Bootstrap is required (guarded defensively anyway).

import com.mojang.math.Divisor;

public class DivisorParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Defensive: Divisor is pure and needs no bootstrap, but guarding is
        // harmless in case class init ever pulls something in.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // Divisor does not need the bootstrap; ignore if unavailable.
        }

        // Numerator battery: zeros, small magnitudes, signs, exact multiples and
        // off-by-one of typical denominators, larger values, and overflow-prone
        // extremes (Divisor uses plain int +/- so two's-complement wrap matters).
        int[] NUMS = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17,
            -1, -2, -3, -4, -5, -7, -8, -16, -17,
            100, 101, 127, 128, 255, 256, 999, 1000, 1024,
            -100, -1000, -1024,
            65535, 65536, -65536,
            1000000, -1000000,
            2147483647, -2147483648, 1073741824, -1073741824
        };

        // Denominator battery: the >0 path is the real one; 0 and negatives hit
        // the else-branch (quotient=mod=0) and hasNext()==false so produce empty
        // sequences. Include a wide spread of segment counts.
        int[] DENS = {
            0, -1, -2, -7, -100, -2147483648,
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17,
            32, 64, 100, 128, 255, 256, 1000
        };

        for (int num : NUMS) {
            for (int den : DENS) {
                Divisor d = new Divisor(num, den);
                StringBuilder sb = new StringBuilder();
                int count = 0;
                while (d.hasNext()) {
                    int v = d.nextInt();
                    sb.append('\t').append(v);
                    count++;
                    // Safety bound: denominator is the segment count; for the
                    // physical battery above this is always small/finite. Negative
                    // and zero denominators yield count==0 (hasNext false).
                    if (count > 2000) break;
                }
                O.println("SEQ\t" + num + "\t" + den + "\t" + count + sb.toString());
            }
        }
    }
}

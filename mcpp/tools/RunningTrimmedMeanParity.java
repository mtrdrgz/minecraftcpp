// Ground-truth generator for net.minecraft.client.renderer.RunningTrimmedMean
// (Minecraft 26.1.2) using the REAL decompiled class. RunningTrimmedMean is a
// fixed-capacity ring buffer that returns a trimmed mean (one min + one max
// dropped) of the held `long` samples, with a peculiar `count / total` fallback
// when <= 2 samples are present. We drive a fresh instance per scenario with a
// scripted sequence of registerValueAndGetMean() calls and emit each returned
// mean. Pure 64-bit integer arithmetic; no GL — but we run the standard
// bootstrap defensively anyway (guarded; RunningTrimmedMean needs none).
//
//   mcpp/tools/run_groundtruth.ps1 -Tool RunningTrimmedMeanParity -Out mcpp/build/running_trimmed_mean.tsv
//
// TSV row format (tab-separated), dispatched by leading TAG in the C++ test:
//   SEQ  <maxCount>  <n>  <in_0> <out_0>  <in_1> <out_1> ... <in_{n-1}> <out_{n-1}>
// All values decimal; inputs are int64, outputs are int64 (the returned mean).

import net.minecraft.client.renderer.RunningTrimmedMean;

public class RunningTrimmedMeanParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // RunningTrimmedMean is pure and needs no bootstrap; guard anyway in case
        // class init ever pulls something in (it does not for this class).
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — not required for RunningTrimmedMean.
        }

        // Sample batteries fed into the ring. Cover: zeros, positives, negatives,
        // mixed signs, duplicates (tie-handling for min/max removal), monotonic
        // ramps, alternating extremes, and overflow-prone Long extremes (the sum
        // uses plain long +/- so two's-complement wrap matters).
        long[][] INPUTS = {
            {},                                  // never register (output sequence empty)
            {0},
            {7},
            {-7},
            {5, 5},
            {1, 2},
            {-1, -2},
            {10, 20, 30},                        // first time count>2 branch fires
            {30, 20, 10},
            {5, 5, 5},                           // all equal: min==max==5
            {1, 2, 3, 4, 5},
            {5, 4, 3, 2, 1},
            {-5, 0, 5, 10, -10},
            {100, -100, 100, -100, 100, -100},   // alternating, sum oscillates
            {1, 1, 1, 1, 1, 1, 1, 1},
            {0, 0, 0, 0},
            {-3, -2, -1, 0, 1, 2, 3},            // symmetric around 0 → trimmed sum 0
            {2, 2, 3, 3, 4, 4},                  // duplicate min/max candidates
            {1000000, 2000000, 3000000, 4000000},
            {-1000000, 2000000, -3000000, 4000000, -5000000},
            // Long extremes: exercise wrap in `total += current` and `min+max`.
            {Long.MAX_VALUE, Long.MIN_VALUE, 0},
            {Long.MAX_VALUE, Long.MAX_VALUE, Long.MAX_VALUE},
            {Long.MIN_VALUE, Long.MIN_VALUE, Long.MIN_VALUE},
            {Long.MAX_VALUE, 1, Long.MIN_VALUE, -1, 0},
            {Long.MIN_VALUE, Long.MAX_VALUE},    // count==2 fallback with extremes
            {Long.MAX_VALUE},                    // count==1 fallback
            // Longer streams that wrap the ring multiple times.
            {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
            {12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1},
            {-100, 50, -100, 50, -100, 50, -100, 50, -100, 50},
            {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5},
        };

        // Ring capacities to exercise: 1 (degenerate, count caps at 1 → always
        // fallback branch), 2 (always fallback), 3 (first capacity hitting the
        // trimmed branch), and several larger windows that cause wrap.
        int[] CAPS = {1, 2, 3, 4, 5, 8, 16};

        for (int cap : CAPS) {
            for (long[] seq : INPUTS) {
                RunningTrimmedMean m = new RunningTrimmedMean(cap);
                StringBuilder sb = new StringBuilder();
                int n = 0;
                for (long in : seq) {
                    long out = m.registerValueAndGetMean(in);
                    sb.append('\t').append(in).append('\t').append(out);
                    n++;
                }
                O.println("SEQ\t" + cap + "\t" + n + sb.toString());
            }
        }
    }
}

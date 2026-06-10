import com.mojang.serialization.DataResult;
import net.minecraft.util.InclusiveRange;

// Ground truth for mcpp/src/util/InclusiveRange.h. Emits tab-separated rows from the
// REAL net.minecraft.util.InclusiveRange<Integer> methods. All exercised methods are
// public, so no reflection is needed.
//
// Row formats (TAG \t inputs... \t outputs...):
//   INRANGE     min max value | inRange(0/1)
//   CONTAINS    aMin aMax bMin bMax | aContainsB(0/1)
//   CTOR        min max | constructible(0/1)        (canonical ctor throws when min>max)
//   CREATE      min max | ok(0/1)                   (static create -> DataResult success?)
//   TOSTRING    min max | "[min, max]"              (only emitted for valid ranges)
public class InclusiveRangeParity {
    static final java.io.PrintStream O = System.out;

    // Try to build an InclusiveRange<Integer>; returns null if the canonical ctor throws.
    static InclusiveRange<Integer> tryMake(int min, int max) {
        try {
            return new InclusiveRange<Integer>(min, max);
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Representative finite ints: zeros, small magnitudes, byte/short/large boundaries,
        // negatives, and the 32-bit extremes. compareTo on Integer is plain signed compare.
        int[] vals = {
            Integer.MIN_VALUE, -2147483647, -1000000, -65536, -256, -100, -16, -2, -1,
            0, 1, 2, 7, 15, 16, 64, 100, 127, 128, 255, 256, 1000, 65535, 65536,
            1000000, 2147483646, Integer.MAX_VALUE
        };

        // A compact subset for the O(n^4) CONTAINS sweep so the TSV stays finite.
        int[] small = { Integer.MIN_VALUE, -1000, -16, -1, 0, 1, 16, 100, 1000, Integer.MAX_VALUE };

        // CTOR invariant + CREATE (DataResult) over all pairs.
        for (int min : vals) {
            for (int max : vals) {
                InclusiveRange<Integer> r = tryMake(min, max);
                O.println("CTOR\t" + min + "\t" + max + "\t" + (r != null ? 1 : 0));

                DataResult<InclusiveRange<Integer>> dr = InclusiveRange.create(min, max);
                int ok = dr.result().isPresent() ? 1 : 0;
                O.println("CREATE\t" + min + "\t" + max + "\t" + ok);

                if (r != null) {
                    O.println("TOSTRING\t" + min + "\t" + max + "\t" + r.toString());
                }
            }
        }

        // INRANGE: for every valid (min<=max) range, test isValueInRange over many values.
        for (int min : vals) {
            for (int max : vals) {
                if (min > max) continue;  // skip invalid (ctor would throw)
                InclusiveRange<Integer> r = new InclusiveRange<Integer>(min, max);
                for (int v : vals) {
                    int in = r.isValueInRange(v) ? 1 : 0;
                    O.println("INRANGE\t" + min + "\t" + max + "\t" + v + "\t" + in);
                }
                // Probe the exact boundaries and just-outside values too.
                int[] edge = { min, max,
                               (min == Integer.MIN_VALUE) ? min : min - 1,
                               (max == Integer.MAX_VALUE) ? max : max + 1 };
                for (int v : edge) {
                    int in = r.isValueInRange(v) ? 1 : 0;
                    O.println("INRANGE\t" + min + "\t" + max + "\t" + v + "\t" + in);
                }
            }
        }

        // CONTAINS: outer range a contains sub-range b. Only valid ranges considered.
        for (int aMin : small) {
            for (int aMax : small) {
                if (aMin > aMax) continue;
                InclusiveRange<Integer> a = new InclusiveRange<Integer>(aMin, aMax);
                for (int bMin : small) {
                    for (int bMax : small) {
                        if (bMin > bMax) continue;
                        InclusiveRange<Integer> b = new InclusiveRange<Integer>(bMin, bMax);
                        int c = a.contains(b) ? 1 : 0;
                        O.println("CONTAINS\t" + aMin + "\t" + aMax + "\t" + bMin + "\t" + bMax + "\t" + c);
                    }
                }
            }
        }
    }
}

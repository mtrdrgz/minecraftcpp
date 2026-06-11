import com.mojang.datafixers.util.Pair;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

// Ground truth for mcpp/src/util/BlockUtil.h — the REAL
// net.minecraft.util.BlockUtil.getMaxRectangleLocation(int[]) (the
// @VisibleForTesting largest-rectangle-in-a-histogram helper). It is
// package-private and returns a Pair<BlockUtil.IntBounds, Integer>; we invoke it
// and read the result purely by reflection so no compile-time access is needed.
//
// Emits one TSV row per histogram input:
//   columnsCsv \t boundsMin \t boundsMax \t height
// where columnsCsv is the comma-separated int[] ("EMPTY" for length 0). The C++
// test re-runs its 1:1 port over the same columns and compares min/max/height
// exactly (all decimal ints).
public class BlockUtilParity {
    static final java.io.PrintStream O = System.out;

    static Method GET_MAX;          // BlockUtil.getMaxRectangleLocation(int[])
    static Field IB_MIN, IB_MAX;    // BlockUtil.IntBounds.min / .max

    static {
        try {
            Class<?> bu = Class.forName("net.minecraft.util.BlockUtil");
            for (Method m : bu.getDeclaredMethods()) {
                if (m.getName().equals("getMaxRectangleLocation")) { GET_MAX = m; break; }
            }
            if (GET_MAX == null) throw new RuntimeException("getMaxRectangleLocation not found");
            GET_MAX.setAccessible(true);

            Class<?> ib = Class.forName("net.minecraft.util.BlockUtil$IntBounds");
            IB_MIN = ib.getDeclaredField("min");
            IB_MAX = ib.getDeclaredField("max");
            IB_MIN.setAccessible(true);
            IB_MAX.setAccessible(true);
        } catch (Throwable t) { throw new RuntimeException(t); }
    }

    static String csv(int[] xs) {
        if (xs.length == 0) return "EMPTY";
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < xs.length; i++) { if (i > 0) b.append(','); b.append(xs[i]); }
        return b.toString();
    }

    static void emit(int[] columns) {
        try {
            Object pair = GET_MAX.invoke(null, (Object) columns);
            Object bounds = ((Pair<?, ?>) pair).getFirst();
            int height = (Integer) ((Pair<?, ?>) pair).getSecond();
            int min = IB_MIN.getInt(bounds);
            int max = IB_MAX.getInt(bounds);
            O.println(csv(columns) + "\t" + min + "\t" + max + "\t" + height);
        } catch (Exception e) { throw new RuntimeException(e); }
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        List<int[]> cases = new ArrayList<>();

        // ---- hand-picked edge cases ----------------------------------------------
        cases.add(new int[] {});                       // empty histogram
        cases.add(new int[] {0});                       // single zero bar
        cases.add(new int[] {5});                       // single nonzero bar
        cases.add(new int[] {0, 0, 0});                 // all zero
        cases.add(new int[] {3, 3, 3});                 // flat plateau
        cases.add(new int[] {1, 2, 3, 4, 5});           // strictly increasing
        cases.add(new int[] {5, 4, 3, 2, 1});           // strictly decreasing
        cases.add(new int[] {2, 1, 5, 6, 2, 3});        // classic textbook (max area 10)
        cases.add(new int[] {6, 2, 5, 4, 5, 1, 6});     // valley/peak mix
        cases.add(new int[] {1, 0, 1, 0, 1});           // alternating with zeros
        cases.add(new int[] {0, 5, 0, 5, 0});           // gaps
        cases.add(new int[] {4, 4, 4, 0, 5, 5});        // two equal-area candidates -> first wins
        cases.add(new int[] {3, 1, 3, 2, 2});           // tie-break exercise
        cases.add(new int[] {2, 4});                     // ascending pair
        cases.add(new int[] {4, 2});                     // descending pair
        cases.add(new int[] {1, 1});                     // flat pair
        cases.add(new int[] {7, 1, 1, 1, 1, 1, 7});     // tall ends, low middle
        cases.add(new int[] {1, 2, 2, 2, 1});           // hump
        cases.add(new int[] {Integer.MAX_VALUE, 1});    // overflow-prone: huge bar then short
        cases.add(new int[] {1, Integer.MAX_VALUE});    // huge bar at the end
        cases.add(new int[] {0, 0, 1});                 // trailing single
        cases.add(new int[] {1, 0, 0});                 // leading single
        cases.add(new int[] {2, 2, 2, 2, 2, 2, 2, 2});  // long plateau
        cases.add(new int[] {9, 0});                     // big then zero
        cases.add(new int[] {0, 9});                     // zero then big

        // ---- the exact shape produced by getLargestRectangleAround: a column array
        // whose nonzero run is contiguous, mirroring the in-place "columns[i1] = ..."
        // builder. Several lengths so the sentinel/flush path is well exercised.
        cases.add(new int[] {0, 0, 3, 4, 5, 4, 3, 0, 0});
        cases.add(new int[] {1, 2, 3, 2, 1, 2, 3, 2, 1});

        // ---- randomized battery (deterministic seed) -----------------------------
        Random rnd = new Random(0xB10CC0DEL);
        for (int t = 0; t < 400; t++) {
            int len = rnd.nextInt(16);          // 0..15
            int[] c = new int[len];
            for (int i = 0; i < len; i++) c[i] = rnd.nextInt(8);   // small heights -> many ties
            cases.add(c);
        }
        // a few with larger heights (still well below overflow) to vary areas
        for (int t = 0; t < 200; t++) {
            int len = 1 + rnd.nextInt(12);
            int[] c = new int[len];
            for (int i = 0; i < len; i++) c[i] = rnd.nextInt(100000);
            cases.add(c);
        }

        for (int[] c : cases) emit(c);
    }
}

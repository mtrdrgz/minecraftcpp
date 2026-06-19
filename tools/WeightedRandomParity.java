// Reference value generator for the C++ WeightedRandom / WeightedList port.
// Runs the REAL decompiled net.minecraft.util.random.WeightedRandom and
// net.minecraft.util.random.WeightedList from client.jar so the emitted
// selection indices are exact ground truth.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/WeightedRandomParity.java
//   java  -cp <out>:26.1.2/client.jar WeightedRandomParity > weighted_random.tsv
//
// Rows are tab-separated:
//   WI    <n> <w0..w(n-1)>  <index>  <result>
//         WeightedRandom.getWeightedItem(items, index): result = selected list
//         index, or -1 for Optional.empty.
//   TOT   <n> <w0..w(n-1)>  <total>
//         WeightedRandom.getTotalWeight(items).
//   RI    <seed> <n> <w0..w(n-1)>  <result>
//         WeightedRandom.getRandomItem(LegacyRandomSource(seed), items):
//         selected list index, or -1 for Optional.empty.
//   WL    <seed> <n> <w0..w(n-1)>  <result>
//         WeightedList.of(items).getRandom(LegacyRandomSource(seed)): selected
//         list index, or -1 for empty. Exercises the Flat (<64) and Compact
//         (>=64) selector strategies plus the empty (total==0) branch.
//
// O is captured at class load so any bootstrap chatter on stdout stays out of
// the TSV.
public class WeightedRandomParity {
    static final java.io.PrintStream O = System.out;

    // Each item is a unique holder carrying its list index and weight. Using a
    // fresh object per slot guarantees reference identity is unique (Integer
    // boxing caches small values, which would collide for duplicate weights),
    // so the chosen index is recovered exactly via ==. The weightGetter returns
    // the holder's weight.
    static final class Item {
        final int index;
        final int weight;
        Item(int index, int weight) { this.index = index; this.weight = weight; }
    }

    static java.util.List<Item> items(int[] weights) {
        java.util.List<Item> list = new java.util.ArrayList<>();
        for (int i = 0; i < weights.length; i++) {
            list.add(new Item(i, weights[i]));  // unique object per slot
        }
        return list;
    }

    // Resolve an Optional<Item> back to its list index via reference identity.
    static int indexOf(java.util.List<Item> list, java.util.Optional<Item> opt) {
        if (opt.isEmpty()) return -1;
        Item sel = opt.get();
        for (int i = 0; i < list.size(); i++) {
            if (list.get(i) == sel) return i;  // identity match
        }
        return -2;  // should never happen
    }

    static String wstr(int[] w) {
        StringBuilder sb = new StringBuilder();
        sb.append(w.length);
        for (int x : w) sb.append('\t').append(x);
        return sb.toString();
    }

    @SuppressWarnings("unchecked")
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        java.util.function.ToIntFunction<Item> idGetter = (Item it) -> it.weight;

        // Reflect the real WeightedRandom static methods.
        Class<?> wr = Class.forName("net.minecraft.util.random.WeightedRandom");
        java.lang.reflect.Method mTotal = null, mWeighted = null, mRandom2 = null;
        for (java.lang.reflect.Method m : wr.getDeclaredMethods()) {
            m.setAccessible(true);
            Class<?>[] p = m.getParameterTypes();
            if (m.getName().equals("getTotalWeight") && p.length == 2) {
                mTotal = m;
            } else if (m.getName().equals("getWeightedItem") && p.length == 3) {
                mWeighted = m;
            } else if (m.getName().equals("getRandomItem") && p.length == 4) {
                mRandom2 = m;  // (random, items, totalWeight, weightGetter)
            }
        }

        // Weight batteries: include single/empty, zero weights interleaved,
        // sums straddling the Flat/Compact threshold (64), and large weights.
        int[][] batteries = new int[][] {
            {},                       // empty list
            {1},                      // single
            {5},                      // single, weight 5
            {0},                      // single zero weight (total 0)
            {1, 1, 1, 1},             // uniform
            {3, 1, 2},                // mixed small
            {10, 0, 5, 0, 1},         // interleaved zeros
            {0, 0, 7},                // leading zeros
            {7, 0, 0},                // trailing zeros
            {2, 4, 8, 16, 32},        // total 62 (Flat, just under 64)
            {2, 4, 8, 16, 33},        // total 63 (Flat, at boundary)
            {2, 4, 8, 16, 34},        // total 64 (Compact, at threshold)
            {2, 4, 8, 16, 35},        // total 65 (Compact)
            {100, 200, 300},          // larger, Compact
            {1, 1000000, 1},          // skewed Compact
            {0, 0, 0},                // all zero (total 0)
            {63},                     // single 63 (Flat)
            {64},                     // single 64 (Compact)
            {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, // total 55 Flat
        };

        // ---- TOT: getTotalWeight ----
        for (int[] w : batteries) {
            java.util.List<Item> list = items(w);
            int total = (Integer) mTotal.invoke(null, list, idGetter);
            O.println("TOT\t" + wstr(w) + "\t" + total);
        }

        // ---- WI: getWeightedItem over the full [0,total) sweep + edges ----
        for (int[] w : batteries) {
            java.util.List<Item> list = items(w);
            int total = (Integer) mTotal.invoke(null, list, idGetter);
            // Sweep every in-range index; also probe negative and >=total edges
            // (getWeightedItem just runs the subtract loop, no precondition).
            java.util.List<Integer> probes = new java.util.ArrayList<>();
            probes.add(-1);
            for (int idx = 0; idx < Math.max(total, 1); idx++) probes.add(idx);
            probes.add(total);
            probes.add(total + 1);
            for (int idx : probes) {
                java.util.Optional<Item> opt =
                    (java.util.Optional<Item>) mWeighted.invoke(null, list, idx, idGetter);
                O.println("WI\t" + wstr(w) + "\t" + idx + "\t" + indexOf(list, opt));
            }
        }

        // ---- RI: getRandomItem(random, items, totalWeight, weightGetter) ----
        // Skip the empty / total==0 lists where getRandomItem returns empty
        // WITHOUT consuming the RNG (total==0 short-circuit) but still emit them.
        long[] seeds = { 0L, 1L, 2L, 42L, 123456789L, -1L, -987654321L,
                         2147483647L, -1234567890123456789L, 8675309L };
        for (long seed : seeds) {
            for (int[] w : batteries) {
                java.util.List<Item> list = items(w);
                int total = (Integer) mTotal.invoke(null, list, idGetter);
                net.minecraft.world.level.levelgen.LegacyRandomSource rng =
                    new net.minecraft.world.level.levelgen.LegacyRandomSource(seed);
                java.util.Optional<Item> opt = (java.util.Optional<Item>)
                    mRandom2.invoke(null, rng, list, total, idGetter);
                O.println("RI\t" + seed + "\t" + wstr(w) + "\t" + indexOf(list, opt));
            }
        }

        // ---- WL: WeightedList.of(items).getRandom(random) ----
        // Builds the real WeightedList so the Flat / Compact / empty selector
        // strategy is chosen exactly as in production, then draws with a seeded
        // LegacyRandomSource. Items are Weighted<Integer> with the box as value.
        for (long seed : seeds) {
            for (int[] w : batteries) {
                java.util.List<net.minecraft.util.random.Weighted<Item>> witems =
                    new java.util.ArrayList<>();
                java.util.List<Item> holders = new java.util.ArrayList<>();
                for (int i = 0; i < w.length; i++) {
                    Item holder = new Item(i, w[i]);  // unique value per slot
                    holders.add(holder);
                    witems.add(new net.minecraft.util.random.Weighted<Item>(holder, w[i]));
                }
                net.minecraft.util.random.WeightedList<Item> wl =
                    net.minecraft.util.random.WeightedList.of(witems);
                net.minecraft.world.level.levelgen.LegacyRandomSource rng =
                    new net.minecraft.world.level.levelgen.LegacyRandomSource(seed);
                java.util.Optional<Item> opt = wl.getRandom(rng);
                int idx;
                if (opt.isEmpty()) {
                    idx = -1;
                } else {
                    Item sel = opt.get();
                    idx = -2;
                    for (int i = 0; i < holders.size(); i++) {
                        if (holders.get(i) == sel) { idx = i; break; }
                    }
                }
                O.println("WL\t" + seed + "\t" + wstr(w) + "\t" + idx);
            }
        }
    }
}

// Reference value generator for the C++ Weighted record + WeightedList.getRandom
// parity gate. Runs the REAL decompiled net.minecraft.util.random.Weighted and
// net.minecraft.util.random.WeightedList from client.jar so every emitted value
// is exact ground truth.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/WeightedParity.java
//   java  -cp <out>;26.1.2/client.jar WeightedParity > weighted_random.tsv
//
// O is captured at class load so any bootstrap chatter on stdout stays out of
// the TSV. Rows are tab-separated; only data rows are printed.
//
// TAGS:
//   WVAL  <value> <weight>  <gotValue> <gotWeight>
//         new Weighted<Integer>(value, weight); echoes value()/weight().
//         Exercises the record accessors round-trip (weight >= 0 only).
//   WMAP  <value> <weight> <addend>  <mappedValue> <mappedWeight>
//         Weighted<Integer>(value,weight).map(v -> v + addend): the value is
//         transformed, the weight preserved. Confirms map() keeps weight.
//   WNEG  <weight>  <threw>
//         new Weighted<Integer>(0, weight) with weight < 0 must throw
//         IllegalArgumentException; threw = 1 if it did, else 0.
//   WL    <seed> <n> <w0..w(n-1)>  <result>
//         WeightedList.of(Weighted<Item>...).getRandom(LegacyRandomSource(seed)):
//         selected list index, or -1 for empty. Exercises the Flat (<64),
//         Compact (>=64) selector strategies and the empty (total==0) branch.
//         This is the load-bearing selection gate (random.nextInt(totalWeight)
//         then the Flat/Compact get()).
public class WeightedParity {
    static final java.io.PrintStream O = System.out;

    // Unique holder per slot so the chosen index is recovered by reference
    // identity (Integer boxing caches small ints, which would collide).
    static final class Item {
        final int index;
        final int weight;
        Item(int index, int weight) { this.index = index; this.weight = weight; }
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

        // ---- WVAL: Weighted record accessors round-trip ----
        int[] values = { 0, 1, -1, 7, -42, 2147483647, -2147483648, 123456 };
        int[] weights = { 0, 1, 2, 5, 63, 64, 65, 100, 1000000, 2147483647 };
        for (int v : values) {
            for (int w : weights) {
                net.minecraft.util.random.Weighted<Integer> e =
                    new net.minecraft.util.random.Weighted<Integer>(Integer.valueOf(v), w);
                O.println("WVAL\t" + v + "\t" + w + "\t" + e.value() + "\t" + e.weight());
            }
        }

        // ---- WMAP: map(value -> value + addend) preserves weight ----
        int[] addends = { 0, 1, -1, 1000, -1000, 2147483647 };
        for (int v : values) {
            for (int w : new int[] { 0, 1, 5, 64 }) {
                for (int add : addends) {
                    net.minecraft.util.random.Weighted<Integer> e =
                        new net.minecraft.util.random.Weighted<Integer>(Integer.valueOf(v), w);
                    final int a = add;
                    net.minecraft.util.random.Weighted<Integer> m =
                        e.map((Integer x) -> Integer.valueOf(x + a));  // int + int wraps
                    O.println("WMAP\t" + v + "\t" + w + "\t" + add + "\t"
                              + m.value() + "\t" + m.weight());
                }
            }
        }

        // ---- WNEG: weight < 0 must throw IllegalArgumentException ----
        int[] negWeights = { -1, -2, -100, -2147483648 };
        for (int w : negWeights) {
            int threw = 0;
            try {
                new net.minecraft.util.random.Weighted<Integer>(Integer.valueOf(0), w);
            } catch (IllegalArgumentException ex) {
                threw = 1;
            }
            O.println("WNEG\t" + w + "\t" + threw);
        }

        // ---- WL: WeightedList.of(Weighted<Item>...).getRandom(random) ----
        // Builds the real WeightedList so the Flat / Compact / empty selector is
        // chosen exactly as in production, then draws with a seeded
        // LegacyRandomSource. Items are Weighted<Item> with a unique holder value.
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
        long[] seeds = { 0L, 1L, 2L, 42L, 123456789L, -1L, -987654321L,
                         2147483647L, -1234567890123456789L, 8675309L };
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

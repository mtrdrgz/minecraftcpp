// Ground-truth generator for the PURE static cost helper of
// net.minecraft.world.inventory.AnvilMenu (26.1.2):
//
//   public static int calculateIncreasedRepairCost(int baseCost)
//       return (int)Math.min(baseCost * 2L + 1L, 2147483647L);
//
// The C++ port mc::world::inventory::calculateIncreasedRepairCost (in
// world/inventory/AnvilMenuCost.h) must reproduce every emitted value bit-for-bit.
//
// We invoke the REAL method by reflection so we never replicate its body on the Java side
// (RULE #0: the ground truth must come from the real class, not a re-implementation here).
// The method is public+static, but reflecting it keeps us honest even if visibility ever
// changes, and lets us load the class without constructing a menu instance.
//
//   tools/run_groundtruth.ps1 -Tool AnvilMenuCostParity -Out mcpp/build/anvil_cost.tsv
//
// TSV row (single TAG, result is decimal int32 — the method returns int):
//   COST  <baseCost>  <result>
//
// Inputs sweep the boundaries where the long-arithmetic / clamp / long->int narrowing
// traps live: small repair costs, the exact saturation boundary (where 2*x+1 first reaches
// or exceeds Integer.MAX_VALUE), Integer.MAX_VALUE / MIN_VALUE, and negatives (clamp is a
// no-op there, so the narrowing cast just truncates the 64-bit value).

@SuppressWarnings({"deprecation", "unchecked"})
public class AnvilMenuCostParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Resolve the REAL method on the REAL class; invoke it, never re-implement it.
        java.lang.reflect.Method m = net.minecraft.world.inventory.AnvilMenu.class
            .getDeclaredMethod("calculateIncreasedRepairCost", int.class);
        m.setAccessible(true);

        java.util.LinkedHashSet<Integer> inputs = new java.util.LinkedHashSet<>();

        // Realistic anvil repair-cost progression: 0,1,3,7,15,... each = 2*prev+1.
        // This is exactly the geometric chain the game walks, and it climbs straight into
        // the Integer.MAX_VALUE saturation in ~31 steps.
        for (int v = 0, i = 0; i < 40; i++) {
            inputs.add(v);
            long next = (long) v * 2L + 1L;            // step in long so we don't wrap mid-sweep
            v = (int) Math.min(next, 2147483647L);     // same clamp the method uses, to advance
        }

        // Dense low range — every small base cost.
        for (int v = -20; v <= 80; v++) inputs.add(v);

        // Around the saturation boundary: result clamps once baseCost*2L+1L >= 2147483647L,
        // i.e. baseCost >= 1073741823. Probe a window on both sides of 1073741823.
        for (long b = 1073741818L; b <= 1073741828L; b++) inputs.add((int) b);

        // Powers of two and (2^k - 1) across the whole positive range.
        for (int k = 0; k <= 30; k++) {
            inputs.add(1 << k);
            inputs.add((1 << k) - 1);
            inputs.add((1 << k) + 1);
        }

        // Extremes and sign-flip / narrowing traps.
        int[] extremes = {
            Integer.MAX_VALUE, Integer.MIN_VALUE,
            2147483646, 2147483645, -2147483647,
            1073741822, 1073741823, 1073741824, 1073741825,
            -1073741823, -1073741824, -1073741825,
            1431655765, -1431655766, -559038737, 559038737,
            -1, -2, -3, -7, -15, -100, -1000000,
        };
        for (int v : extremes) inputs.add(v);

        for (int v : inputs) {
            int result = (Integer) m.invoke(null, v);
            O.println("COST\t" + v + "\t" + result);
        }
    }
}

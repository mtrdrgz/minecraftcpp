// Ground truth for net.minecraft.world.entity.ExperienceOrb's pure integer
// experience-decomposition helpers (Minecraft 26.1.2).
//
// Exercises the REAL class:
//   - getExperienceValue(int)  — the public static threshold ladder
//     (ExperienceOrb.java:346-368), resolved by reflection (setAccessible) and
//     invoked on the actual bytecode. The body is NEVER reimplemented here.
//
// We additionally emit a SPLIT row that records, for a battery of reward
// amounts, the orb-value sequence produced by repeatedly calling the REAL
// getExperienceValue (this mirrors awardWithDirection's decomposition loop,
// lines 191-199, which is itself just getExperienceValue applied in a loop —
// we do NOT reimplement getExperienceValue; we call it). The C++ side compares
// against ExperienceOrbValue::splitIntoOrbs.
//
// ints are emitted in decimal. Row tags (tab-separated):
//   XPVAL  <maxValue>            <getExperienceValue(maxValue)>
//   SPLIT  <amount>  <count>     <v0> <v1> ...           (the orb sequence)
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;

public class ExperienceOrbValueParity {
    static final java.io.PrintStream O = System.out;

    static Method GET;

    static int xpValue(int maxValue) throws Exception {
        return (int) GET.invoke(null, maxValue);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> cls = Class.forName("net.minecraft.world.entity.ExperienceOrb");
        GET = cls.getMethod("getExperienceValue", int.class);
        GET.setAccessible(true);

        // ── getExperienceValue battery ──────────────────────────────────────
        // The 10 ladder thresholds (the 11th branch is the < 3 fallthrough).
        int[] thresholds = {2477, 1237, 617, 307, 149, 73, 37, 17, 7, 3};

        // Use an ordered set so duplicates collapse but emission order is
        // deterministic (stable across runs / platforms).
        LinkedHashSet<Integer> inputs = new LinkedHashSet<>();

        // Each threshold and its immediate neighbours (boundary behaviour).
        for (int t : thresholds) {
            inputs.add(t - 1);
            inputs.add(t);
            inputs.add(t + 1);
        }

        // Dense sweep of the low range where every branch is reachable and the
        // gaps between thresholds are small (0..2600 covers all 11 branches and
        // every boundary).
        for (int v = -8; v <= 2600; v++) {
            inputs.add(v);
        }

        // Strided sweep of the high range (everything >= 2477 returns 2477, but
        // we confirm the saturation holds far out and at the positive edge).
        for (long v = 2600; v <= (long) Integer.MAX_VALUE; v += 97_777L) {
            inputs.add((int) v);
        }
        inputs.add(Integer.MAX_VALUE);
        inputs.add(Integer.MAX_VALUE - 1);

        // Negative range (all return 1) — strided plus the negative edge. These
        // are reachable in principle from int math feeding the helper; the
        // method just falls through to the `< 3` branch (returns 1).
        for (long v = -1; v >= (long) Integer.MIN_VALUE; v -= 131_071L) {
            inputs.add((int) v);
        }
        inputs.add(Integer.MIN_VALUE);
        inputs.add(Integer.MIN_VALUE + 1);

        for (int v : inputs) {
            O.println("XPVAL\t" + v + "\t" + xpValue(v));
        }

        // ── orb-split battery ───────────────────────────────────────────────
        // Reward amounts swept across the interesting magnitudes: small values
        // (single orb), values that straddle thresholds, and large rewards that
        // decompose into many orbs (e.g. a full XP dump). All amounts > 0 are
        // physical reward sizes. The sequence is produced by the SAME real
        // getExperienceValue, looped exactly as awardWithDirection does.
        LinkedHashSet<Integer> amounts = new LinkedHashSet<>();
        for (int a = 1; a <= 3000; a++) amounts.add(a);
        for (int t : thresholds) { amounts.add(t); amounts.add(t + 1); amounts.add(2 * t); }
        for (long a = 3000; a <= 2_000_000L; a += 9973L) amounts.add((int) a);
        amounts.add(1_000_000);
        amounts.add(7);    // exactly one threshold orb
        amounts.add(6);    // 3 + 3
        amounts.add(2);    // single 1-orb? -> 1, then 1 => [1,1]

        for (int amount : amounts) {
            List<Integer> orbs = new ArrayList<>();
            int rem = amount;
            // awardWithDirection (lines 191-199), world merge omitted (it does
            // not change the value sequence). getExperienceValue is the REAL
            // method invoked via reflection — not reimplemented.
            while (rem > 0) {
                int v = xpValue(rem);
                rem -= v;
                orbs.add(v);
            }
            StringBuilder sb = new StringBuilder();
            sb.append("SPLIT\t").append(amount).append('\t').append(orbs.size());
            for (int v : orbs) sb.append('\t').append(v);
            O.println(sb.toString());
        }
    }
}

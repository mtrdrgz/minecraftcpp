// Ground truth for net.minecraft.util.ByIdMap (Minecraft 26.1.2).
//
// We drive the REAL ByIdMap.continuous(...) and ByIdMap.sparse(...) factory
// methods (both public) and exercise the IntFunction<T> they return across a wide
// battery of int keys, capturing the looked-up value bit-for-bit.
//
// The generic T is irrelevant to the math: every branch reduces to integer index
// arithmetic on `id` and `length`. We therefore use Integer payloads. For each
// scenario the idGetter is `Integer::intValue`, so:
//   * continuous: the value array is a permutation of 0..length-1, and the lookup
//     returns the Integer whose value is the resolved index. We emit that int.
//   * sparse:     each value's id is its own int; absent ids return `_default`,
//     which we emit as a distinguished sentinel so the C++ side checks the exact
//     fallback path.
//
// Row TAGs (tab-separated; all ints decimal):
//   CONT  <scen> <length> <strategyOrd> <id> <result>
//         strategyOrd: 0=ZERO 1=WRAP 2=CLAMP (OutOfBoundsStrategy.ordinal())
//         result: the int payload ByIdMap's continuous function returned for `id`.
//   SPRS  <scen> <id> <result>
//         result: the int payload sparse(...) returned for `id`, or the default
//                 sentinel (see DEFAULT_SENTINEL) when `id` was absent.
//
// The C++ test rebuilds the identical scenarios and compares every result int
// BIT-FOR-BIT (std::bit_cast on the 32-bit value).

import java.lang.reflect.Method;
import java.util.function.IntFunction;
import java.util.function.ToIntFunction;
import net.minecraft.util.ByIdMap;

public class ByIdMapParity {
    static final java.io.PrintStream O = System.out;

    // A payload sparse() never stores, so it is unambiguous as the "absent" marker.
    static final int DEFAULT_SENTINEL = -999999;

    // OutOfBoundsStrategy enum, reached reflectively to avoid hard-coding the type
    // name in switch positions (and to confirm ordinals against the real enum).
    static Class<?> strategyClass() throws Exception {
        for (Class<?> c : ByIdMap.class.getDeclaredClasses()) {
            if (c.getSimpleName().equals("OutOfBoundsStrategy")) return c;
        }
        throw new IllegalStateException("OutOfBoundsStrategy not found");
    }

    @SuppressWarnings("unchecked")
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> stratCls = strategyClass();
        Object[] strategies = stratCls.getEnumConstants();  // [ZERO, WRAP, CLAMP]

        ToIntFunction<Integer> idGetter = Integer::intValue;

        // ----- continuous(...) scenarios -----
        // Lengths chosen to cover length==1, small, and a larger array, with
        // various permutations of the contiguous id set so build-time sorting is
        // genuinely exercised.
        contScenario("len1",   idGetter, new Integer[] { 0 }, strategies);
        contScenario("len2",   idGetter, new Integer[] { 1, 0 }, strategies);
        contScenario("len4",   idGetter, new Integer[] { 2, 0, 3, 1 }, strategies);
        contScenario("len7",   idGetter, new Integer[] { 6, 0, 4, 2, 5, 1, 3 }, strategies);
        contScenario("len16",  idGetter,
            new Integer[] { 15, 1, 9, 4, 12, 0, 7, 3, 14, 6, 10, 2, 13, 5, 11, 8 },
            strategies);

        // ----- sparse(...) scenarios -----
        // Distinct (possibly non-contiguous, possibly negative) ids; default value
        // returned for any id not present.
        sprsScenario("sp_dense", idGetter, new Integer[] { 0, 1, 2, 3, 4 });
        sprsScenario("sp_gaps",  idGetter, new Integer[] { 0, 5, 2, 9, 3, 40, 7, 1, 12, 100, 4 });
        sprsScenario("sp_neg",   idGetter, new Integer[] { -5, -1, 0, 3, 7, -100, 50 });
        sprsScenario("sp_one",   idGetter, new Integer[] { 42 });

        O.flush();
    }

    // Build the continuous function for each strategy and probe a battery of ids
    // that straddles the in-range window on both sides (and around zero).
    static void contScenario(String scen, ToIntFunction<Integer> idGetter,
                             Integer[] values, Object[] strategies) throws Exception {
        int length = values.length;
        for (int s = 0; s < strategies.length; s++) {
            Object strategy = strategies[s];
            int ord = ((Enum<?>) strategy).ordinal();
            @SuppressWarnings("unchecked")
            IntFunction<Integer> fn =
                ByIdMap.continuous(idGetter, values, (ByIdMap.OutOfBoundsStrategy) strategy);

            for (int id : probeIds(length)) {
                Integer r = fn.apply(id);
                // continuous always returns a non-null in-array Integer; for our
                // values the payload == the resolved index.
                O.println("CONT\t" + scen + "\t" + length + "\t" + ord + "\t" + id
                          + "\t" + r.intValue());
            }
        }
    }

    static void sprsScenario(String scen, ToIntFunction<Integer> idGetter,
                             Integer[] values) throws Exception {
        Integer def = Integer.valueOf(DEFAULT_SENTINEL);
        @SuppressWarnings("unchecked")
        IntFunction<Integer> fn = ByIdMap.sparse(idGetter, values, def);

        // Probe every present id, plus a sweep of nearby and far ids (hits + misses).
        java.util.LinkedHashSet<Integer> ids = new java.util.LinkedHashSet<>();
        for (Integer v : values) ids.add(v.intValue());
        for (int i = -110; i <= 110; i++) ids.add(i);
        ids.add(Integer.MIN_VALUE);
        ids.add(Integer.MAX_VALUE);
        for (int id : ids) {
            Integer r = fn.apply(id);
            O.println("SPRS\t" + scen + "\t" + id + "\t" + r.intValue());
        }
    }

    // A range of ids spanning well below 0 to well past length, designed to hit
    // every out-of-bounds branch (and the in-range path).
    static int[] probeIds(int length) {
        java.util.LinkedHashSet<Integer> set = new java.util.LinkedHashSet<>();
        // Dense in-range and a margin on each side.
        for (int i = -3 * length - 5; i <= 3 * length + 5; i++) set.add(i);
        // Extreme values to exercise floorMod/clamp at the int boundaries.
        set.add(Integer.MIN_VALUE);
        set.add(Integer.MIN_VALUE + 1);
        set.add(Integer.MAX_VALUE);
        set.add(Integer.MAX_VALUE - 1);
        set.add(-100000);
        set.add(100000);
        int[] out = new int[set.size()];
        int i = 0;
        for (int v : set) out[i++] = v;
        return out;
    }
}

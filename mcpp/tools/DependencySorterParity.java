// Ground-truth generator for net.minecraft.util.DependencySorter<Integer, Entry>.
//
// Builds a DependencySorter with Integer keys and small Entry implementations
// (required / optional dependency lists), runs the REAL orderByDependencies, and
// captures the exact BiConsumer visitation order. Emits one tab-separated row per
// case so the C++ dependency_sorter_parity gate can reconstruct the identical
// inputs and compare the visitation order byte-for-byte.
//
// Row format (all ints decimal):
//   ORDER \t <caseId> \t <entriesSpec> \t <resultOrder>
// where
//   entriesSpec = entry(';'entry)*   ; insertion order == addEntry call order
//   entry       = <id>':'reqList'|'optList
//   reqList     = (<dep>(','<dep>)*)?   ; visitRequiredDependencies emit order
//   optList     = (<dep>(','<dep>)*)?   ; visitOptionalDependencies emit order
//   resultOrder = (<id>(','<id>)*)?     ; BiConsumer accept() order
//
// Only public API is used (public ctor + addEntry + orderByDependencies + the
// public Entry interface), so no reflection is required. Bootstrap is invoked
// defensively in case a future change makes the class touch registries.

import java.util.ArrayList;
import java.util.List;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import net.minecraft.util.DependencySorter;

public class DependencySorterParity {
    static final java.io.PrintStream O = System.out;

    // A concrete Entry<Integer>: fixed required/optional dependency lists, emitted
    // in list order by the visit* callbacks (matching the Java Consumer contract).
    static final class E implements DependencySorter.Entry<Integer> {
        final int[] required;
        final int[] optional;
        E(int[] required, int[] optional) { this.required = required; this.optional = optional; }
        @Override public void visitRequiredDependencies(Consumer<Integer> output) {
            for (int d : required) output.accept(d);
        }
        @Override public void visitOptionalDependencies(Consumer<Integer> output) {
            for (int d : optional) output.accept(d);
        }
    }

    // One entry's spec, kept so we can re-emit the exact inputs into the TSV.
    static final class Spec {
        final int id; final int[] req; final int[] opt;
        Spec(int id, int[] req, int[] opt) { this.id = id; this.req = req; this.opt = opt; }
    }

    static final List<Object[]> CASES = new ArrayList<>(); // {String caseId, List<Spec>}

    static int[] a(int... v) { return v; }

    static void add(String caseId, Spec... specs) {
        List<Spec> list = new ArrayList<>();
        for (Spec s : specs) list.add(s);
        CASES.add(new Object[]{caseId, list});
    }

    static Spec s(int id, int[] req, int[] opt) { return new Spec(id, req, opt); }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
            // DependencySorter is pure (no registries); bootstrap is only defensive.
        }

        buildCases();

        for (Object[] c : CASES) {
            String caseId = (String) c[0];
            List<Spec> specs = (List<Spec>) c[1];

            DependencySorter<Integer, E> sorter = new DependencySorter<>();
            for (Spec sp : specs) {
                sorter.addEntry(sp.id, new E(sp.req, sp.opt));
            }

            final StringBuilder order = new StringBuilder();
            BiConsumer<Integer, E> out = (id, entry) -> {
                if (order.length() > 0) order.append(',');
                order.append(id.intValue());
            };
            sorter.orderByDependencies(out);

            StringBuilder spec = new StringBuilder();
            for (int i = 0; i < specs.size(); i++) {
                if (i > 0) spec.append(';');
                Spec sp = specs.get(i);
                spec.append(sp.id).append(':');
                appendList(spec, sp.req);
                spec.append('|');
                appendList(spec, sp.opt);
            }

            O.print("ORDER\t");
            O.print(caseId);
            O.print('\t');
            O.print(spec);
            O.print('\t');
            O.print(order);
            O.print('\n');
        }
        O.flush();
    }

    static void appendList(StringBuilder sb, int[] arr) {
        for (int i = 0; i < arr.length; i++) {
            if (i > 0) sb.append(',');
            sb.append(arr[i]);
        }
    }

    // ------------------------------------------------------------------
    // Finite, physical test battery. Keys are arbitrary ints (incl. negatives,
    // collisions modulo small table sizes, and >12 entries to force HashMap
    // resize) to exercise the iteration-order-dependent visitation.
    static void buildCases() {
        // 1. Single entry, no deps.
        add("single", s(0, a(), a()));

        // 2. Linear required chain 0->1->2->3 (each requires the next).
        add("chain_req",
            s(0, a(1), a()),
            s(1, a(2), a()),
            s(2, a(3), a()),
            s(3, a(), a()));

        // 3. Same chain but reversed addEntry order.
        add("chain_req_rev",
            s(3, a(), a()),
            s(2, a(3), a()),
            s(1, a(2), a()),
            s(0, a(1), a()));

        // 4. Diamond: 0 requires 1 and 2; both require 3.
        add("diamond",
            s(0, a(1, 2), a()),
            s(1, a(3), a()),
            s(2, a(3), a()),
            s(3, a(), a()));

        // 5. Optional dependencies only.
        add("optional_only",
            s(0, a(), a(1)),
            s(1, a(), a(2)),
            s(2, a(), a()));

        // 6. Mixed required + optional.
        add("mixed",
            s(0, a(1), a(2)),
            s(1, a(3), a()),
            s(2, a(), a(3)),
            s(3, a(), a()));

        // 7. Direct 2-cycle: 0 requires 1, 1 requires 0 (cycle-breaking).
        add("cycle2",
            s(0, a(1), a()),
            s(1, a(0), a()));

        // 8. 3-cycle: 0->1->2->0.
        add("cycle3",
            s(0, a(1), a()),
            s(1, a(2), a()),
            s(2, a(0), a()));

        // 9. Dependency on a key NOT in contents (recursion finds no entry).
        add("missing_dep",
            s(0, a(99), a()),
            s(1, a(0), a()));

        // (A self-dependency id->id is intentionally OMITTED: combined with any
        //  other node that depends on id it makes the REAL DependencySorter.isCyclic
        //  recurse forever (StackOverflowError) — net.minecraft never feeds one, so
        //  it is not a physical input. The C++ port reproduces the same recursion.)

        // 10. Multiple roots, shared leaf.
        add("multi_root",
            s(10, a(30), a()),
            s(20, a(30), a()),
            s(30, a(), a()),
            s(40, a(), a()));

        // 12. Negative keys (Integer.hashCode == value; spread differs).
        add("negatives",
            s(-1, a(-2), a()),
            s(-2, a(-3), a()),
            s(-3, a(), a()),
            s(-100, a(-1), a()));

        // 13. Keys that collide modulo 16 (low 4 bits equal) -> same bucket chain.
        add("collide16",
            s(1, a(), a()),
            s(17, a(), a()),
            s(33, a(), a()),
            s(49, a(1), a()));

        // 14. Keys colliding after the spread (high-bit interplay).
        add("spread_collide",
            s(0, a(), a()),
            s(65536, a(0), a()),
            s(131072, a(65536), a()));

        // 15. Many entries (>12) to force a HashMap resize (16->32) mid-build.
        {
            List<Spec> many = new ArrayList<>();
            for (int i = 0; i < 20; i++) {
                int[] req = (i + 1 < 20) ? a(i + 1) : a();
                many.add(s(i, req, a()));
            }
            add("resize20", many.toArray(new Spec[0]));
        }

        // 16. Big mixed graph with several optional cross-edges and a back-edge
        //     that would be cyclic (dropped).
        add("bigmixed",
            s(100, a(101, 102), a(103)),
            s(101, a(104), a()),
            s(102, a(104), a(105)),
            s(103, a(), a(100)),   // optional back-edge to a dependent -> cyclic, dropped
            s(104, a(), a()),
            s(105, a(104), a()));

        // 17. Wide fan-out: one node requires many.
        add("fanout",
            s(0, a(1, 2, 3, 4, 5), a()),
            s(1, a(), a()),
            s(2, a(), a()),
            s(3, a(), a()),
            s(4, a(), a()),
            s(5, a(), a()));

        // 18. Same dependency listed twice (HashSet dedups in the multimap).
        add("dup_dep",
            s(0, a(1, 1, 1), a(1)),
            s(1, a(), a()));

        // 19. Required and optional point to the same target.
        add("req_opt_same",
            s(0, a(2), a(2)),
            s(1, a(2), a()),
            s(2, a(), a()));

        // 20. Larger spread of keys incl. powers of two and odd values.
        add("assorted",
            s(7, a(3, 11), a()),
            s(3, a(2), a()),
            s(11, a(2), a(7)),    // optional back-edge -> cyclic w.r.t 7, dropped
            s(2, a(), a()),
            s(15, a(7, 3), a()),
            s(8, a(15), a()));

        // 21. Empty sorter (no entries) — output nothing.
        add("empty");

        // 22. Two independent chains interleaved in addEntry order.
        add("two_chains",
            s(0, a(1), a()),
            s(10, a(11), a()),
            s(1, a(2), a()),
            s(11, a(12), a()),
            s(2, a(), a()),
            s(12, a(), a()));

        // 23. Long single chain to deeper recursion (also forces resize at 13).
        {
            List<Spec> chain = new ArrayList<>();
            for (int i = 0; i < 15; i++) {
                int[] req = (i + 1 < 15) ? a(i + 1) : a();
                chain.add(s(i, req, a()));
            }
            add("longchain15", chain.toArray(new Spec[0]));
        }

        // 24. Cross dependencies forming a partial order with two sinks.
        add("two_sinks",
            s(0, a(2, 3), a()),
            s(1, a(2, 3), a()),
            s(2, a(), a()),
            s(3, a(), a()));

        // 25. Mutually-optional pair (each optionally depends on the other) ->
        //     one direction kept, the other dropped as cyclic.
        add("mutual_opt",
            s(0, a(), a(1)),
            s(1, a(), a(0)));
    }
}

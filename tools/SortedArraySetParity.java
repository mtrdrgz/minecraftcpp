import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.util.SortedArraySet;
import net.minecraft.util.Util;

// Ground truth for mcpp/src/util/SortedArraySet.h. Drives the REAL
// net.minecraft.util.SortedArraySet<Integer> (natural-order create() flavour) plus
// net.minecraft.util.Util.growByHalf, emitting tab-separated rows a C++ *_parity
// test recomputes and compares BIT-FOR-BIT.
//
// The set's backing-array length (contents.length == capacity) is private, so it is
// read via reflection on the `contents` field. growByHalf is package-private static,
// driven via reflection too.
//
// Row formats (TAG \t fields...):
//   GROW   cur min | growByHalf(cur,min)
//          Directly probes Util.growByHalf over a wide edge battery (overflow/clamp,
//          signed shift, narrowing).
//
//   OP     scen step opName arg | ret \t size \t capacity \t orderedCSV
//          One executed operation in a scripted scenario. `ret` is the operation's
//          observable return (bool as 0/1, addOrGet/get return the int value or the
//          sentinel -2147483648-ish encoded specially below). After the op we also
//          snapshot size, capacity, and the full ordered contents (CSV of the active
//          prefix). opNames: add/addOrGet/remove/contains/get/first/last/clear.
//
// For `get`, ret encodes presence: "<value>" when found, "ABSENT" when not (the Java
// get returns null). For first/last on a non-empty set ret is the int; we never call
// them on an empty set. addOrGet ret is the returned int. contains/add/remove ret is
// 0/1.
public class SortedArraySetParity {
    static final java.io.PrintStream O = System.out;

    static Field contentsField;
    static Method growByHalf;

    @SuppressWarnings("unchecked")
    static int capacityOf(SortedArraySet<Integer> s) throws Exception {
        Object arr = contentsField.get(s);
        return java.lang.reflect.Array.getLength(arr);
    }

    static String orderedCsv(SortedArraySet<Integer> s) {
        StringBuilder sb = new StringBuilder();
        boolean first = true;
        for (Integer v : s) {            // iteration order == sorted array order
            if (!first) sb.append(',');
            sb.append(v.intValue());
            first = false;
        }
        return sb.toString();
    }

    static void emitOp(String scen, int step, String op, String argRepr, String ret,
                       SortedArraySet<Integer> s) throws Exception {
        O.println("OP\t" + scen + "\t" + step + "\t" + op + "\t" + argRepr + "\t" + ret
                  + "\t" + s.size() + "\t" + capacityOf(s) + "\t" + orderedCsv(s));
    }

    // Run one scripted scenario. `ops` is a list of {opCode, arg}; opCode:
    //   0 add, 1 addOrGet, 2 remove, 3 contains, 4 get, 5 first, 6 last, 7 clear
    static void runScenario(String scen, int initialCapacity, int[][] ops) throws Exception {
        SortedArraySet<Integer> s = SortedArraySet.create(initialCapacity);
        // emit initial state as step 0 (a no-op "init")
        emitOp(scen, 0, "init", "-", "-", s);
        int step = 1;
        for (int[] op : ops) {
            int code = op[0];
            int arg = op.length > 1 ? op[1] : 0;
            String ret;
            switch (code) {
                case 0: ret = s.add(arg) ? "1" : "0"; emitOp(scen, step, "add", Integer.toString(arg), ret, s); break;
                case 1: ret = Integer.toString(s.addOrGet(arg)); emitOp(scen, step, "addOrGet", Integer.toString(arg), ret, s); break;
                case 2: ret = s.remove(Integer.valueOf(arg)) ? "1" : "0"; emitOp(scen, step, "remove", Integer.toString(arg), ret, s); break;
                case 3: ret = s.contains(Integer.valueOf(arg)) ? "1" : "0"; emitOp(scen, step, "contains", Integer.toString(arg), ret, s); break;
                case 4: { Integer g = s.get(arg); ret = (g == null) ? "ABSENT" : Integer.toString(g); emitOp(scen, step, "get", Integer.toString(arg), ret, s); break; }
                case 5: ret = Integer.toString(s.first()); emitOp(scen, step, "first", "-", ret, s); break;
                case 6: ret = Integer.toString(s.last()); emitOp(scen, step, "last", "-", ret, s); break;
                case 7: s.clear(); emitOp(scen, step, "clear", "-", "-", s); break;
                default: throw new IllegalStateException();
            }
            step++;
        }
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        contentsField = SortedArraySet.class.getDeclaredField("contents");
        contentsField.setAccessible(true);
        growByHalf = Util.class.getDeclaredMethod("growByHalf", int.class, int.class);
        growByHalf.setAccessible(true);

        // ---- GROW: directly exercise Util.growByHalf over an edge battery. ----
        int[] curs = {
            0, 1, 2, 3, 4, 5, 7, 9, 10, 11, 15, 16, 31, 32, 63, 100, 1000,
            1431655765, 1431655766, 1073741823, 1073741824, 1431655764,
            2147483637, 2147483638, 2147483639, 2147483640, 2147483646,
            Integer.MAX_VALUE, -1, -2, -10, -1000,
            Integer.MIN_VALUE, Integer.MIN_VALUE + 1
        };
        int[] mins = {
            0, 1, 2, 5, 10, 11, 16, 100, 1000, 2147483638, 2147483639,
            2147483640, Integer.MAX_VALUE, -1, Integer.MIN_VALUE
        };
        for (int cur : curs) {
            for (int min : mins) {
                int r = (int) growByHalf.invoke(null, cur, min);
                O.println("GROW\t" + cur + "\t" + min + "\t" + r);
            }
        }

        // ---- OP scenarios: scripted op sequences over varied initial capacities. ----

        // S1: default capacity 10, ascending+descending+dup inserts to force growth
        //     past 10 (10 -> growByHalf(10,11)=15 -> growByHalf(15,16)=22 ...).
        {
            List<int[]> ops = new ArrayList<>();
            int[] vals = {5, 1, 9, 3, 7, 2, 8, 4, 6, 0, 10, 11, 12, 13, 14, 15, 16, 17};
            for (int v : vals) ops.add(new int[]{0, v});      // add
            for (int v : vals) ops.add(new int[]{0, v});      // add again (all dup -> false)
            ops.add(new int[]{3, 7});                          // contains present
            ops.add(new int[]{3, 100});                        // contains absent
            ops.add(new int[]{4, 9});                          // get present
            ops.add(new int[]{4, -5});                         // get absent
            ops.add(new int[]{5});                             // first
            ops.add(new int[]{6});                             // last
            ops.add(new int[]{2, 0});                          // remove min
            ops.add(new int[]{2, 17});                         // remove max
            ops.add(new int[]{2, 8});                          // remove middle
            ops.add(new int[]{2, 999});                        // remove absent
            runScenario("S1", 10, ops.toArray(new int[0][]));
        }

        // S2: initialCapacity 0 -> first insert grows via growByHalf(0,1)=1, then
        //     1->growByHalf(1,2)=2, 2->3, 3->4, 4->growByHalf(4,5)=6, 6->9, 9->13...
        //     This nails the small-capacity growth ladder (NOT the dead <10 branch).
        {
            List<int[]> ops = new ArrayList<>();
            int[] vals = {100, 50, 75, 25, 60, 10, 90, 40, 80, 30, 70, 20, 95, 5, 85};
            for (int v : vals) ops.add(new int[]{1, v});      // addOrGet (returns arg, all new)
            ops.add(new int[]{1, 50});                         // addOrGet existing -> returns 50
            ops.add(new int[]{1, 95});                         // addOrGet existing
            runScenario("S2", 0, ops.toArray(new int[0][]));
        }

        // S3: extreme values incl. MIN/MAX/negatives, interleaved remove/add, clear.
        {
            List<int[]> ops = new ArrayList<>();
            int[] vals = {Integer.MIN_VALUE, Integer.MAX_VALUE, 0, -1, 1,
                          -2147483647, 2147483646, -100000, 100000, 42};
            for (int v : vals) ops.add(new int[]{0, v});
            ops.add(new int[]{5});                             // first -> MIN_VALUE
            ops.add(new int[]{6});                             // last  -> MAX_VALUE
            ops.add(new int[]{4, Integer.MIN_VALUE});
            ops.add(new int[]{4, Integer.MAX_VALUE});
            ops.add(new int[]{2, 0});
            ops.add(new int[]{2, Integer.MIN_VALUE});
            ops.add(new int[]{0, -1});                         // re-add existing -> false
            ops.add(new int[]{0, -3});                         // new
            ops.add(new int[]{7});                             // clear
            ops.add(new int[]{0, 7});                          // add after clear (size 1)
            ops.add(new int[]{0, -7});
            ops.add(new int[]{5});
            ops.add(new int[]{6});
            runScenario("S3", 3, ops.toArray(new int[0][]));
        }

        // S4: capacity 1, churn: add then remove repeatedly to test removeInternal
        //     shifting + that capacity only ever grows (never shrinks).
        {
            List<int[]> ops = new ArrayList<>();
            for (int i = 0; i < 12; i++) ops.add(new int[]{0, i * 7 - 30});   // spread, some negative
            for (int i = 11; i >= 0; i -= 2) ops.add(new int[]{2, i * 7 - 30});// remove odd-index vals
            for (int i = 0; i < 12; i++) ops.add(new int[]{0, i});            // add a fresh dense run
            ops.add(new int[]{6});
            ops.add(new int[]{5});
            runScenario("S4", 1, ops.toArray(new int[0][]));
        }

        // S5: large-ish capacity preallocated (100) so NO growth happens; verifies
        //     capacity stays 100 and pure insertion ordering across 80 sorted-random ints.
        {
            List<int[]> ops = new ArrayList<>();
            // deterministic pseudo-shuffle of 0..79 via a fixed LCG-ish permutation
            int x = 12345;
            boolean[] seen = new boolean[80];
            int placed = 0;
            while (placed < 80) {
                x = (x * 1103515245 + 12345) & 0x7fffffff;
                int v = x % 80;
                if (!seen[v]) { seen[v] = true; ops.add(new int[]{0, v}); placed++; }
            }
            ops.add(new int[]{5});
            ops.add(new int[]{6});
            ops.add(new int[]{2, 40});
            ops.add(new int[]{3, 40});
            runScenario("S5", 100, ops.toArray(new int[0][]));
        }
    }
}

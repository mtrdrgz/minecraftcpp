import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.TreeMap;

// Ground-truth emitter for net.minecraft.world.level.pathfinder.BinaryHeap (26.1.2).
//
// Drives the REAL BinaryHeap with scripted op sequences over REAL Node objects
// (Node.f set / Node.heapIdx read via reflection), then dumps, per scenario:
//   SCN  <id>  <script>  <popOrder>  <finalHeapIds>  <finalIdxMap>
// where
//   <script>       = space-separated ops the C++ side replays VERBATIM:
//                      I <nid> <costBits>   insert node nid with f=cost
//                      C <nid> <costBits>   changeCost(node, newCost)
//                      R <nid>              remove(node)
//                      P                    pop()  (popped id appended to popOrder)
//                      K                    peek() (peeked id appended to popOrder, prefixed 'k')
//   <costBits>     = %08x of Float.floatToRawIntBits(cost) — bit-exact float key
//   <popOrder>     = space-separated ids in the order P/K observed them ('k<id>' for peek)
//   <finalHeapIds> = space-separated node ids in the final heap-array order (getHeap())
//   <finalIdxMap>  = space-separated "<nid>:<heapIdx>" for every created node, by nid
//
// The Random here ONLY builds the op script (which is emitted verbatim), so it
// need not match the C++ RNG — both sides replay the identical emitted string.
// Costs are FINITE/PHYSICAL A* totals (small/large positive + a few negatives
// that are still plain finite floats; no NaN/Inf — Inf is BinaryHeap's internal
// sentinel only).
public class BinaryHeapParity {
    static final java.io.PrintStream O = System.out;

    static String fb(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> heapC = Class.forName("net.minecraft.world.level.pathfinder.BinaryHeap");
        Class<?> nodeC = Class.forName("net.minecraft.world.level.pathfinder.Node");

        Constructor<?> heapCtor = heapC.getDeclaredConstructor();
        heapCtor.setAccessible(true);
        Constructor<?> nodeCtor = nodeC.getConstructor(int.class, int.class, int.class);
        nodeCtor.setAccessible(true);

        Field fF = nodeC.getField("f");           fF.setAccessible(true);
        Field fHeapIdx = nodeC.getField("heapIdx"); fHeapIdx.setAccessible(true);

        Method mInsert = heapC.getMethod("insert", nodeC);          mInsert.setAccessible(true);
        Method mPop = heapC.getMethod("pop");                       mPop.setAccessible(true);
        Method mPeek = heapC.getMethod("peek");                     mPeek.setAccessible(true);
        Method mRemove = heapC.getMethod("remove", nodeC);          mRemove.setAccessible(true);
        Method mChange = heapC.getMethod("changeCost", nodeC, float.class); mChange.setAccessible(true);
        Method mSize = heapC.getMethod("size");                     mSize.setAccessible(true);
        Method mGetHeap = heapC.getMethod("getHeap");               mGetHeap.setAccessible(true);

        // A palette of finite, physical A*-style costs (block-distance-ish), with
        // deliberate exact ties (3.0 appears many times) to exercise the strict-'<'
        // tie ordering, plus boundary magnitudes and a few negatives that are still
        // ordinary finite floats.
        float[] costPalette = {
            0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.0f, 3.0f, 3.0f,
            4.0f, 7.0f, 10.0f, 12.5f, 16.0f, 31.0f, 64.0f, 100.0f, 127.5f,
            255.0f, 256.0f, 1000.0f, 1023.75f, 4096.0f, 30000000.0f,
            -1.0f, -3.0f, -100.0f, 0.25f, 0.125f, 1.4142135f, 1.7320508f, 2.2360680f
        };

        int scenarioId = 0;

        // ---- Deterministic hand-written scenarios (edge structure) ----------
        // 1) ascending inserts then drain
        emit(scenarioId++, scriptAscendingDrain(costPalette),
             heapCtor, nodeCtor, fF, fHeapIdx,
             mInsert, mPop, mPeek, mRemove, mChange, mGetHeap);
        // 2) descending inserts then drain
        emit(scenarioId++, scriptDescendingDrain(costPalette),
             heapCtor, nodeCtor, fF, fHeapIdx,
             mInsert, mPop, mPeek, mRemove, mChange, mGetHeap);
        // 3) all-equal costs then drain (pure tie ordering)
        emit(scenarioId++, scriptAllEqualDrain(),
             heapCtor, nodeCtor, fF, fHeapIdx,
             mInsert, mPop, mPeek, mRemove, mChange, mGetHeap);
        // 4) interleaved insert/pop
        emit(scenarioId++, scriptInterleaved(costPalette),
             heapCtor, nodeCtor, fF, fHeapIdx,
             mInsert, mPop, mPeek, mRemove, mChange, mGetHeap);
        // 5) changeCost up & down
        emit(scenarioId++, scriptChangeCost(costPalette),
             heapCtor, nodeCtor, fF, fHeapIdx,
             mInsert, mPop, mPeek, mRemove, mChange, mGetHeap);
        // 6) removes from various positions
        emit(scenarioId++, scriptRemoves(costPalette),
             heapCtor, nodeCtor, fF, fHeapIdx,
             mInsert, mPop, mPeek, mRemove, mChange, mGetHeap);
        // 7) capacity-growth: insert > 128 then drain (exercises array doubling)
        emit(scenarioId++, scriptCapacityGrowth(),
             heapCtor, nodeCtor, fF, fHeapIdx,
             mInsert, mPop, mPeek, mRemove, mChange, mGetHeap);

        // ---- Randomized scenarios (broad fuzz of the full op mix) ------------
        long[] seeds = { 1L, 2L, 7L, 42L, 1234L, 99999L, 2024L, 31337L,
                         8675309L, 123456789L, 555L, 777L };
        for (long seed : seeds) {
            for (int len : new int[]{ 20, 60, 150, 400 }) {
                emit(scenarioId++, scriptRandom(seed, len, costPalette),
                     heapCtor, nodeCtor, fF, fHeapIdx,
                     mInsert, mPop, mPeek, mRemove, mChange, mGetHeap);
            }
        }
    }

    // ---- scenario script builders (return ops as a List<String> of tokens) ----

    static List<String> scriptAscendingDrain(float[] pal) {
        List<String> s = new ArrayList<>();
        int n = pal.length;
        for (int i = 0; i < n; i++) s.add("I " + i + " " + fb(pal[i]));
        for (int i = 0; i < n; i++) { s.add("K"); s.add("P"); }
        return s;
    }

    static List<String> scriptDescendingDrain(float[] pal) {
        List<String> s = new ArrayList<>();
        int n = pal.length;
        for (int i = n - 1; i >= 0; i--) s.add("I " + i + " " + fb(pal[i]));
        for (int i = 0; i < n; i++) { s.add("K"); s.add("P"); }
        return s;
    }

    static List<String> scriptAllEqualDrain() {
        List<String> s = new ArrayList<>();
        for (int i = 0; i < 40; i++) s.add("I " + i + " " + fb(3.0f));
        for (int i = 0; i < 40; i++) s.add("P");
        return s;
    }

    static List<String> scriptInterleaved(float[] pal) {
        List<String> s = new ArrayList<>();
        int id = 0;
        for (int round = 0; round < 30; round++) {
            for (int k = 0; k < 3; k++) {
                s.add("I " + id + " " + fb(pal[id % pal.length]));
                id++;
            }
            s.add("K");
            s.add("P");
        }
        s.add("K");
        while (true) { s.add("P"); if (id-- <= 0) break; } // over-drain guarded in replay by size
        return s;
    }

    static List<String> scriptChangeCost(float[] pal) {
        List<String> s = new ArrayList<>();
        int n = 25;
        for (int i = 0; i < n; i++) s.add("I " + i + " " + fb(pal[i % pal.length]));
        // bump some up, drop others down
        for (int i = 0; i < n; i++) {
            float nc = (i % 2 == 0) ? pal[(i * 3 + 1) % pal.length] : pal[(i * 7 + 2) % pal.length];
            s.add("C " + i + " " + fb(nc));
        }
        for (int i = 0; i < n; i++) { s.add("K"); s.add("P"); }
        return s;
    }

    static List<String> scriptRemoves(float[] pal) {
        List<String> s = new ArrayList<>();
        int n = 30;
        for (int i = 0; i < n; i++) s.add("I " + i + " " + fb(pal[(i * 5 + 3) % pal.length]));
        // remove a scattered subset
        int[] rm = { 0, 7, 14, 21, 28, 3, 11, 19, 27, 1 };
        for (int r : rm) s.add("R " + r);
        for (int i = 0; i < n; i++) { s.add("K"); s.add("P"); }
        return s;
    }

    static List<String> scriptCapacityGrowth() {
        List<String> s = new ArrayList<>();
        int n = 300; // forces 128 -> 256 -> 512 doubling
        for (int i = 0; i < n; i++) {
            // pseudo-spread costs without RNG: a simple hash to floats
            float c = (float) ((i * 2654435761L & 0xffff) % 5000) / 13.0f;
            s.add("I " + i + " " + fb(c));
        }
        for (int i = 0; i < n; i++) s.add("P");
        return s;
    }

    static List<String> scriptRandom(long seed, int len, float[] pal) {
        Random rng = new Random(seed);
        List<String> s = new ArrayList<>();
        List<Integer> live = new ArrayList<>(); // ids currently in heap
        int nextId = 0;
        for (int step = 0; step < len; step++) {
            int r = rng.nextInt(100);
            if (r < 45 || live.isEmpty()) {
                int id = nextId++;
                s.add("I " + id + " " + fb(pal[rng.nextInt(pal.length)]));
                live.add(id);
            } else if (r < 70) {
                s.add("P");
                // pop removes the min; we don't know which id w/o simulating, so
                // just drop a tracked entry by re-syncing later via 'live' size.
                live.remove(live.size() - 1); // size bookkeeping only
            } else if (r < 80) {
                s.add("K");
            } else if (r < 92) {
                int idx = rng.nextInt(live.size());
                int id = live.get(idx);
                s.add("C " + id + " " + fb(pal[rng.nextInt(pal.length)]));
            } else {
                int idx = rng.nextInt(live.size());
                int id = live.remove(idx);
                s.add("R " + id);
            }
        }
        return s;
    }

    // ---- replay one scenario against the REAL heap and emit its row ----
    static void emit(int scenarioId, List<String> ops,
                     Constructor<?> heapCtor, Constructor<?> nodeCtor,
                     Field fF, Field fHeapIdx,
                     Method mInsert, Method mPop, Method mPeek, Method mRemove,
                     Method mChange, Method mGetHeap) throws Exception {
        Object heap = heapCtor.newInstance();
        Map<Integer, Object> nodes = new HashMap<>();      // id -> real Node
        Map<Object, Integer> idOf = new IdentityHashMap<>();   // Node identity -> id (all nodes are (0,0,0)-equal, so a HashMap would collapse them)
        List<String> popOrder = new ArrayList<>();
        StringBuilder script = new StringBuilder();

        for (String op : ops) {
            if (script.length() > 0) script.append(' ');
            String[] tk = op.split(" ");
            char c = tk[0].charAt(0);
            switch (c) {
                case 'I': {
                    int id = Integer.parseInt(tk[1]);
                    float cost = Float.intBitsToFloat((int) Long.parseLong(tk[2], 16));
                    Object node = nodeCtor.newInstance(0, 0, 0);
                    fF.setFloat(node, cost);
                    mInsert.invoke(heap, node);
                    nodes.put(id, node);
                    idOf.put(node, id);
                    script.append("I ").append(id).append(' ').append(tk[2]);
                    break;
                }
                case 'C': {
                    int id = Integer.parseInt(tk[1]);
                    float cost = Float.intBitsToFloat((int) Long.parseLong(tk[2], 16));
                    Object node = nodes.get(id);
                    if (node == null || fHeapIdx.getInt(node) < 0) {
                        // node not in heap; skip in BOTH GT and C++ (replay checks too)
                        script.append("C ").append(id).append(' ').append(tk[2]);
                        break;
                    }
                    mChange.invoke(heap, node, cost);
                    script.append("C ").append(id).append(' ').append(tk[2]);
                    break;
                }
                case 'R': {
                    int id = Integer.parseInt(tk[1]);
                    Object node = nodes.get(id);
                    if (node == null || fHeapIdx.getInt(node) < 0) {
                        script.append("R ").append(id);
                        break;
                    }
                    mRemove.invoke(heap, node);
                    script.append("R ").append(id);
                    break;
                }
                case 'P': {
                    Object[] h = (Object[]) mGetHeap.invoke(heap);
                    if (h.length == 0) { script.append("P"); break; }
                    Object popped = mPop.invoke(heap);
                    popOrder.add(Integer.toString(idOf.get(popped)));
                    script.append("P");
                    break;
                }
                case 'K': {
                    Object[] h = (Object[]) mGetHeap.invoke(heap);
                    if (h.length == 0) { script.append("K"); break; }
                    Object peeked = mPeek.invoke(heap);
                    popOrder.add("k" + idOf.get(peeked));
                    script.append("K");
                    break;
                }
                default: break;
            }
        }

        // final heap-array order (ids)
        Object[] finalHeap = (Object[]) mGetHeap.invoke(heap);
        StringBuilder heapIds = new StringBuilder();
        for (int i = 0; i < finalHeap.length; i++) {
            if (i > 0) heapIds.append(' ');
            heapIds.append(idOf.get(finalHeap[i]));
        }

        // final heapIdx of every created node, sorted by id
        TreeMap<Integer, Integer> idxMap = new TreeMap<>();
        for (Map.Entry<Integer, Object> e : nodes.entrySet()) {
            idxMap.put(e.getKey(), fHeapIdx.getInt(e.getValue()));
        }
        StringBuilder idxStr = new StringBuilder();
        boolean first = true;
        for (Map.Entry<Integer, Integer> e : idxMap.entrySet()) {
            if (!first) idxStr.append(' ');
            first = false;
            idxStr.append(e.getKey()).append(':').append(e.getValue());
        }

        O.println("SCN\t" + scenarioId + "\t" + script + "\t"
                  + String.join(" ", popOrder) + "\t"
                  + heapIds + "\t" + idxStr);
    }
}

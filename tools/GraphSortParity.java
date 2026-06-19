import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.function.Consumer;

// Ground truth for mcpp/src/util/Graph.h — the REAL net.minecraft.util.Graph
// (depthFirstSearch over a fixed int adjacency map). The method is a public
// static generic; we invoke it through reflection on Integer-typed collections.
//
// Determinism of the produced order depends entirely on (a) the iteration order
// of each vertex's out-edge Set and (b) the order DFS roots are seeded. To make
// that reproducible bit-for-bit in C++ we:
//   * key the edges map with a TreeMap<Integer,...> (ascending keys, same as the
//     real FeatureSorter caller and the same as C++ std::map<int,...>),
//   * give each vertex a TreeSet<Integer> of out-edges (ascending; the C++ side
//     stores the identical sorted vector).
//
// Row formats (TAG \t fields...). Graphs are serialised as:
//   nVerts ; v0>a,b,c ; v1>d ; ...           (adjacency, ascending verts/edges)
// A "graph spec" field is "V|E" where V = sorted vertex list "v0,v1,...", and
// E = ";"-separated "src>dst0,dst1,...". An empty edge list omits the ">..".
//
//   DFS    graphSpec  root        cycle(0/1)  order(csv)    discovered(csv)
//          (single Graph.depthFirstSearch call from `root` on empty visited sets)
//   SORT   graphSpec              cycle(0/1)  order(csv)
//          (FeatureSorter-style driver: iterate verts ascending, DFS each
//           undiscovered one; stop & report cycle=1 if any call returns true)
public class GraphSortParity {
    static final java.io.PrintStream O = System.out;

    // ---- reflection handle to the real Graph.depthFirstSearch ------------------
    static Method DFS;
    static {
        try {
            Class<?> g = Class.forName("net.minecraft.util.Graph");
            for (Method m : g.getDeclaredMethods()) {
                if (m.getName().equals("depthFirstSearch")) { DFS = m; break; }
            }
            DFS.setAccessible(true);
        } catch (Throwable t) { throw new RuntimeException(t); }
    }

    @SuppressWarnings("unchecked")
    static boolean callDfs(Map<Integer, Set<Integer>> edges, Set<Integer> discovered,
                           Set<Integer> currentlyVisiting, Consumer<Integer> order,
                           Integer current) {
        try {
            return (Boolean) DFS.invoke(null, edges, discovered, currentlyVisiting, order, current);
        } catch (Exception e) { throw new RuntimeException(e); }
    }

    static String csv(List<Integer> xs) {
        if (xs.isEmpty()) return "EMPTY";
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < xs.size(); i++) { if (i > 0) b.append(','); b.append(xs.get(i)); }
        return b.toString();
    }

    // serialise an adjacency map (TreeMap-ordered) into the "V|E" graph spec
    static String spec(int[] verts, int[][] adj) {
        StringBuilder v = new StringBuilder();
        for (int i = 0; i < verts.length; i++) { if (i > 0) v.append(','); v.append(verts[i]); }
        StringBuilder e = new StringBuilder();
        boolean first = true;
        for (int i = 0; i < verts.length; i++) {
            int[] outs = adj[i];
            if (outs.length == 0) continue;
            if (!first) e.append(';');
            first = false;
            e.append(verts[i]).append('>');
            for (int j = 0; j < outs.length; j++) { if (j > 0) e.append(','); e.append(outs[j]); }
        }
        return v.toString() + "|" + e.toString();
    }

    // build a TreeMap edges map (ascending keys, TreeSet ascending out-edges)
    static Map<Integer, Set<Integer>> build(int[] verts, int[][] adj) {
        Map<Integer, Set<Integer>> edges = new TreeMap<>();
        for (int i = 0; i < verts.length; i++) {
            Set<Integer> s = new TreeSet<>();
            for (int o : adj[i]) s.add(o);
            edges.put(verts[i], s);
        }
        return edges;
    }

    // single DFS call from `root` on fresh empty sets
    static void emitDfs(int[] verts, int[][] adj, int root) {
        Map<Integer, Set<Integer>> edges = build(verts, adj);
        Set<Integer> discovered = new LinkedHashSet<>();
        Set<Integer> visiting = new LinkedHashSet<>();
        List<Integer> order = new ArrayList<>();
        boolean cycle = callDfs(edges, discovered, visiting, (Consumer<Integer>) order::add, root);
        List<Integer> disc = new ArrayList<>(discovered);
        O.println("DFS\t" + spec(verts, adj) + "\t" + root + "\t" + (cycle ? 1 : 0)
                  + "\t" + csv(order) + "\t" + csv(disc));
    }

    // FeatureSorter-style full driver: iterate vertices ascending, DFS each that is
    // not yet discovered; report a cycle the moment any call returns true.
    static void emitSort(int[] verts, int[][] adj) {
        Map<Integer, Set<Integer>> edges = build(verts, adj);
        Set<Integer> discovered = new LinkedHashSet<>();
        Set<Integer> visiting = new LinkedHashSet<>();
        List<Integer> order = new ArrayList<>();
        boolean cycle = false;
        for (Integer v : edges.keySet()) {       // TreeMap keySet -> ascending
            if (!discovered.contains(v)
                && callDfs(edges, discovered, visiting, (Consumer<Integer>) order::add, v)) {
                cycle = true;
                break;
            }
        }
        O.println("SORT\t" + spec(verts, adj) + "\t" + (cycle ? 1 : 0) + "\t" + csv(order));
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ---- a battery of fixed, finite int adjacency graphs ----------------------
        // Each entry: { verts, adjacency-rows-as-int[][] } (adj[i] = out-edges of verts[i]).
        int[][] V = new int[][] {
            // 0: single vertex, no edges
            {0},
            // 1: two vertices, 0->1
            {0, 1},
            // 2: linear chain 0->1->2->3
            {0, 1, 2, 3},
            // 3: diamond 0->1,0->2,1->3,2->3
            {0, 1, 2, 3},
            // 4: self-loop on 1 (cycle)
            {0, 1, 2},
            // 5: 2-cycle 1<->2
            {0, 1, 2},
            // 6: 3-cycle 0->1->2->0
            {0, 1, 2},
            // 7: DAG with a branch, two roots
            {0, 1, 2, 3, 4},
            // 8: disconnected components, all acyclic
            {0, 1, 2, 3, 4, 5},
            // 9: wide fan-out 0->{1,2,3,4,5}
            {0, 1, 2, 3, 4, 5},
            // 10: deep chain 0..7
            {0, 1, 2, 3, 4, 5, 6, 7},
            // 11: back-edge deep in chain (cycle 4->1)
            {0, 1, 2, 3, 4},
            // 12: non-contiguous vertex ids
            {5, 10, 20, 30},
            // 13: cross edges, DAG, multiple roots converge
            {0, 1, 2, 3, 4, 5},
            // 14: tree (binary), acyclic
            {0, 1, 2, 3, 4, 5, 6},
            // 15: cycle inside a larger DAG (3->4->5->3)
            {0, 1, 2, 3, 4, 5, 6},
            // 16: empty isolated verts plus one edge
            {0, 1, 2, 3, 4},
            // 17: single edge to already-lower id (DAG, root not 0)
            {1, 2, 3},
        };
        int[][][] A = new int[][][] {
            /* 0 */ { {} },
            /* 1 */ { {1}, {} },
            /* 2 */ { {1}, {2}, {3}, {} },
            /* 3 */ { {1, 2}, {3}, {3}, {} },
            /* 4 */ { {1}, {1}, {} },
            /* 5 */ { {1}, {2}, {1} },
            /* 6 */ { {1}, {2}, {0} },
            /* 7 */ { {2}, {2}, {3, 4}, {}, {} },
            /* 8 */ { {1}, {}, {3}, {}, {5}, {} },
            /* 9 */ { {1, 2, 3, 4, 5}, {}, {}, {}, {}, {} },
            /* 10 */ { {1}, {2}, {3}, {4}, {5}, {6}, {7}, {} },
            /* 11 */ { {1}, {2}, {3}, {4}, {1} },
            /* 12 */ { {10, 20}, {30}, {30}, {} },
            /* 13 */ { {2, 3}, {3, 4}, {5}, {5}, {5}, {} },
            /* 14 */ { {1, 2}, {3, 4}, {5, 6}, {}, {}, {}, {} },
            /* 15 */ { {1}, {2}, {3}, {4}, {5}, {3}, {} },
            /* 16 */ { {}, {}, {3}, {}, {} },
            /* 17 */ { {2}, {3}, {} },
        };

        for (int g = 0; g < V.length; g++) {
            int[] verts = V[g];
            int[][] adj = A[g];
            // full topological-sort driver
            emitSort(verts, adj);
            // single DFS from every vertex as a root (fresh sets each time)
            for (int r : verts) emitDfs(verts, adj, r);
        }
    }
}

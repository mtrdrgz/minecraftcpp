// 1:1 C++ port of net.minecraft.util.Graph (Minecraft 26.1.2).
//
// Java source (26.1.2/src/net/minecraft/util/Graph.java) — the entire class is a
// single static generic method:
//
//   public static <T> boolean depthFirstSearch(
//       Map<T, Set<T>> edges, Set<T> discovered, Set<T> currentlyVisiting,
//       Consumer<T> reverseTopologicalOrder, T current) {
//      if (discovered.contains(current))        return false;
//      if (currentlyVisiting.contains(current)) return true;
//      currentlyVisiting.add(current);
//      for (T next : edges.getOrDefault(current, ImmutableSet.of()))
//          if (depthFirstSearch(edges, discovered, currentlyVisiting,
//                               reverseTopologicalOrder, next)) return true;
//      currentlyVisiting.remove(current);
//      discovered.add(current);
//      reverseTopologicalOrder.accept(current);
//      return false;
//   }
//
// It is a recursive depth-first search that:
//   * returns true the moment a back-edge into a vertex still on the recursion
//     stack (currentlyVisiting) is found — i.e. a cycle was detected;
//   * otherwise, on finishing a vertex (all out-edges explored), appends it to
//     reverseTopologicalOrder and marks it discovered.
//
// The output order is fully determined by (a) the iteration order of a vertex's
// out-edge collection and (b) the order in which the caller seeds DFS roots. The
// real caller (FeatureSorter) keeps its edges in a TreeMap / TreeSet keyed by a
// comparator, i.e. a deterministic ascending order. To stay bit-exact we make the
// adjacency a *vector* of out-edges (insertion / sorted order preserved verbatim)
// and the visited/visiting sets plain membership tests — exactly the behaviour of
// java.util.Set.contains/add/remove, which never reorder a traversal.
//
// This header is header-only and templated on the vertex type T (an int in the
// parity gate). No Minecraft dependency is needed; the algorithm is pure.

#pragma once

#include <functional>
#include <map>
#include <unordered_set>
#include <vector>

namespace mc::util::graph {

// Adjacency map: vertex -> ordered list of its out-edges (iteration order of the
// Java Set is reproduced verbatim by the vector's order). std::map keeps keys in
// ascending order, matching the TreeMap the real caller uses; getOrDefault on a
// missing key yields the empty edge list (== ImmutableSet.of()).
template <typename T>
using Edges = std::map<T, std::vector<T>>;

// 1:1 port of Graph.depthFirstSearch. `discovered` and `currentlyVisiting` are
// caller-owned mutable membership sets (java.util.Set semantics); `accept` is the
// Consumer<T> reverseTopologicalOrder.accept callback. Returns true iff a cycle
// (back-edge into the recursion stack) is encountered.
template <typename T>
bool depthFirstSearch(const Edges<T>& edges,
                      std::unordered_set<T>& discovered,
                      std::unordered_set<T>& currentlyVisiting,
                      const std::function<void(const T&)>& reverseTopologicalOrder,
                      const T& current) {
    if (discovered.find(current) != discovered.end()) {
        return false;
    }
    if (currentlyVisiting.find(current) != currentlyVisiting.end()) {
        return true;
    }
    currentlyVisiting.insert(current);

    auto it = edges.find(current);
    if (it != edges.end()) {
        for (const T& next : it->second) {
            if (depthFirstSearch(edges, discovered, currentlyVisiting,
                                 reverseTopologicalOrder, next)) {
                return true;
            }
        }
    }

    currentlyVisiting.erase(current);
    discovered.insert(current);
    reverseTopologicalOrder(current);
    return false;
}

}  // namespace mc::util::graph

// Bit-exact C++ port of net.minecraft.util.DependencySorter (Minecraft 26.1.2).
//
//   public DependencySorter<K,V> addEntry(K id, V value)
//   public void orderByDependencies(BiConsumer<K,V> output)
//
// The class is a deterministic topological sort whose OUTPUT ORDER is fully
// determined by the iteration orders of three JDK / Guava containers:
//
//   * contents            : java.util.HashMap<K,V>       (default ctor)
//   * directDependencies  : com.google.common.collect.HashMultimap<K,K>
//                           == HashMap<K, HashSet<K>> with each value HashSet
//                             built by Sets.newHashSetWithExpectedSize(2)
//                             => new HashSet<>(Maps.capacity(2)) = new HashSet<>(3)
//                             => table threshold tableSizeFor(3)=4, first put
//                                allocates a capacity-4 table (Guava 33.5.0-jre).
//   * alreadyVisited      : java.util.HashSet<K>         (default ctor)
//
// orderByDependencies (DependencySorter.java:42-48):
//   1. directDependencies = HashMultimap.create();
//   2. contents.forEach((id,v) -> v.visitRequiredDependencies(
//          dep -> addDependencyIfNotCyclic(directDependencies, id, dep)));
//   3. contents.forEach((id,v) -> v.visitOptionalDependencies(... same ...));
//   4. alreadyVisited = new HashSet<>();
//   5. contents.keySet().forEach(topId ->
//          visitDependenciesAndElement(directDependencies, alreadyVisited, topId, output));
//
// visitDependenciesAndElement (DependencySorter.java:21-29):
//   if (alreadyVisited.add(id)) {
//       directDependencies.get(id).forEach(dep -> recurse(dep));
//       V cur = contents.get(id);
//       if (cur != null) output.accept(id, cur);   // cur is always non-null here
//   }
//
// addDependencyIfNotCyclic / isCyclic (DependencySorter.java:31-40): adds (from->to)
// to directDependencies ONLY if it would not create a cycle, i.e. only if `to`
// cannot already reach `from` through the edges added so far (DFS over the live
// multimap). This is evaluated incrementally as edges are inserted, so insertion
// order (== contents HashMap order, then the per-entry visit order) matters.
//
// This port models all three containers with an exact java.util.HashMap simulation
// (h ^ (h>>>16) spread, power-of-two table, order-preserving resize, treeify) so the
// visitation order is byte-identical to the JVM. For Integer keys (the case this gate
// covers) Integer.hashCode() == the int value; comparableClassFor(Integer) != null so
// tree bins use signed Integer.compareTo as the tie-break (deterministic). All inputs
// in the gate keep bins below TREEIFY_THRESHOLD, but the treeify path is ported anyway.
//
// NOTE: this is a header-only, int-key specialization (mc::util::IntDependencySorter).
// The container simulation (JavaIntHashMap / JavaIntHashSet) follows the JDK exactly.

#ifndef MCPP_UTIL_DEPENDENCYSORTER_H
#define MCPP_UTIL_DEPENDENCYSORTER_H

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace mc::util {

// ---------------------------------------------------------------------------
// java.util.HashMap<Integer, V> iteration-order simulation.
//
// Integer keys: hashCode == value; spread == h ^ (h>>>16); table is power-of-two;
// resize splits chains lo/hi preserving order; bins treeify at TREEIFY_THRESHOLD
// (8) once capacity >= MIN_TREEIFY_CAPACITY (64). Tree order uses Integer.compareTo
// (comparableClassFor(Integer) != null), which is total, so tieBreakOrder is never
// reached. V is carried alongside the key but never affects ordering.
template <typename V>
class JavaIntHashMap {
public:
    // initialCapacity == 0 means the default-constructed HashMap() (threshold 0,
    // first put allocates a 16-slot table). A positive value mimics
    // new HashMap<>(initialCapacity): threshold = tableSizeFor(initialCapacity),
    // first put allocates a table of that size.
    explicit JavaIntHashMap(int initialCapacity = 0) {
        if (initialCapacity > 0) {
            m_threshold = tableSizeFor(initialCapacity);
        }
    }

    // HashMap.put: insert or replace. Returns true if a new key was inserted.
    bool put(std::int32_t key, const V& value) {
        if (m_tab.empty()) resizeAllocate();
        const std::uint32_t h = spread(key);
        const int n = static_cast<int>(m_tab.size());
        const int i = static_cast<int>(h & static_cast<std::uint32_t>(n - 1));
        int head = m_tab[static_cast<std::size_t>(i)];
        if (head < 0) {
            m_tab[static_cast<std::size_t>(i)] = newNode(key, h, value);
        } else if (m_isTree[static_cast<std::size_t>(i)]) {
            int found = findTreeVal(head, key, h);
            if (found >= 0) { m_a[static_cast<std::size_t>(found)].value = value; return false; }
            putTreeVal(i, key, h, value);
        } else {
            int p = head;
            for (int binCount = 0;; ++binCount) {
                if (m_a[static_cast<std::size_t>(p)].key == key) {
                    m_a[static_cast<std::size_t>(p)].value = value;
                    return false;
                }
                if (m_a[static_cast<std::size_t>(p)].next < 0) {
                    m_a[static_cast<std::size_t>(p)].next = newNode(key, h, value);
                    if (binCount >= TREEIFY_THRESHOLD - 1) treeifyBin(i);
                    break;
                }
                p = m_a[static_cast<std::size_t>(p)].next;
            }
        }
        if (++m_size > m_threshold) resize();
        return true;
    }

    // HashMap.get: returns pointer to the stored value or nullptr.
    const V* get(std::int32_t key) const {
        const int node = findNode(key);
        return node < 0 ? nullptr : &m_a[static_cast<std::size_t>(node)].value;
    }
    bool containsKey(std::int32_t key) const { return findNode(key) >= 0; }
    int size() const { return m_size; }

    // HashMap iteration order: bucket 0..n-1, each bin head-to-tail via next.
    // Calls fn(key, value) in iteration order.
    void forEach(const std::function<void(std::int32_t, const V&)>& fn) const {
        for (int b : m_tab) {
            for (int e = b; e >= 0; e = m_a[static_cast<std::size_t>(e)].next) {
                fn(m_a[static_cast<std::size_t>(e)].key, m_a[static_cast<std::size_t>(e)].value);
            }
        }
    }
    // keySet() iteration order == entrySet() order for HashMap.
    void forEachKey(const std::function<void(std::int32_t)>& fn) const {
        for (int b : m_tab) {
            for (int e = b; e >= 0; e = m_a[static_cast<std::size_t>(e)].next) {
                fn(m_a[static_cast<std::size_t>(e)].key);
            }
        }
    }

private:
    static constexpr int TREEIFY_THRESHOLD = 8;
    static constexpr int UNTREEIFY_THRESHOLD = 6;
    static constexpr int MIN_TREEIFY_CAPACITY = 64;

    struct Node {
        std::int32_t key = 0;
        std::uint32_t hash = 0;
        V value{};
        int next = -1, prev = -1;
        int parent = -1, left = -1, right = -1;
        bool red = false;
    };

    std::vector<Node> m_a;                  // node arena
    std::vector<int>  m_tab;                 // bucket heads (-1 == empty)
    std::vector<bool> m_isTree;              // per-bucket tree flag
    int m_size = 0;
    int m_threshold = 0;

    static std::uint32_t spread(std::int32_t key) {
        // HashMap.hash(key) with Integer.hashCode() == key value.
        const std::uint32_t u = static_cast<std::uint32_t>(key);
        return u ^ (u >> 16);
    }

    // HashMap.tableSizeFor.
    static int tableSizeFor(int cap) {
        std::uint32_t n = static_cast<std::uint32_t>(cap) - 1u;
        n |= n >> 1; n |= n >> 2; n |= n >> 4; n |= n >> 8; n |= n >> 16;
        if (static_cast<std::int32_t>(n) < 0) return 1;
        return (n >= 0x40000000u) ? 0x40000000 : static_cast<int>(n + 1);
    }

    int newNode(std::int32_t key, std::uint32_t h, const V& value) {
        Node node; node.key = key; node.hash = h; node.value = value;
        m_a.push_back(node);
        return static_cast<int>(m_a.size()) - 1;
    }

    // First table allocation (HashMap.resize from an empty table). If threshold>0
    // (constructed with a capacity) the table is sized to that threshold; else 16.
    void resizeAllocate() {
        int newCap, newThr;
        if (m_threshold > 0) {
            newCap = m_threshold;
            newThr = static_cast<int>(static_cast<long long>(newCap) * 3 / 4); // cap * 0.75f
        } else {
            newCap = 16;
            newThr = 12;
        }
        m_threshold = newThr;
        m_tab.assign(static_cast<std::size_t>(newCap), -1);
        m_isTree.assign(static_cast<std::size_t>(newCap), false);
    }

    int findNode(std::int32_t key) const {
        if (m_tab.empty()) return -1;
        const std::uint32_t h = spread(key);
        const int n = static_cast<int>(m_tab.size());
        const int i = static_cast<int>(h & static_cast<std::uint32_t>(n - 1));
        int head = m_tab[static_cast<std::size_t>(i)];
        if (head < 0) return -1;
        if (m_isTree[static_cast<std::size_t>(i)]) return findTreeVal(head, key, h);
        for (int e = head; e >= 0; e = m_a[static_cast<std::size_t>(e)].next) {
            if (m_a[static_cast<std::size_t>(e)].key == key) return e;
        }
        return -1;
    }

    // dir for tree bins. Integer.compareTo tie-break (comparableClassFor(Integer)!=null).
    int dirOf(std::uint32_t h, std::int32_t key, int pNode) const {
        const std::uint32_t ph = m_a[static_cast<std::size_t>(pNode)].hash;
        if (ph > h) return -1;
        if (ph < h) return 1;
        const std::int32_t pk = m_a[static_cast<std::size_t>(pNode)].key;
        // Integer.compareTo: signed compare; keys here are distinct so != 0.
        if (key < pk) return -1;
        if (key > pk) return 1;
        throw std::logic_error("JavaIntHashMap: duplicate key reached tree insert");
    }

    int findTreeVal(int from, std::int32_t key, std::uint32_t h) const {
        int p = from;
        while (p >= 0) {
            const std::uint32_t ph = m_a[static_cast<std::size_t>(p)].hash;
            if (ph > h) { p = m_a[static_cast<std::size_t>(p)].left; continue; }
            if (ph < h) { p = m_a[static_cast<std::size_t>(p)].right; continue; }
            const std::int32_t pk = m_a[static_cast<std::size_t>(p)].key;
            if (pk == key) return p;
            p = (key < pk) ? m_a[static_cast<std::size_t>(p)].left
                           : m_a[static_cast<std::size_t>(p)].right;
        }
        return -1;
    }

    int rotateLeft(int root, int p) {
        if (p < 0) return root;
        const int r = m_a[static_cast<std::size_t>(p)].right;
        if (r < 0) return root;
        const int rl = m_a[static_cast<std::size_t>(r)].left;
        m_a[static_cast<std::size_t>(p)].right = rl;
        if (rl >= 0) m_a[static_cast<std::size_t>(rl)].parent = p;
        const int pp = m_a[static_cast<std::size_t>(p)].parent;
        m_a[static_cast<std::size_t>(r)].parent = pp;
        if (pp < 0) { root = r; m_a[static_cast<std::size_t>(r)].red = false; }
        else if (m_a[static_cast<std::size_t>(pp)].left == p) m_a[static_cast<std::size_t>(pp)].left = r;
        else m_a[static_cast<std::size_t>(pp)].right = r;
        m_a[static_cast<std::size_t>(r)].left = p;
        m_a[static_cast<std::size_t>(p)].parent = r;
        return root;
    }
    int rotateRight(int root, int p) {
        if (p < 0) return root;
        const int l = m_a[static_cast<std::size_t>(p)].left;
        if (l < 0) return root;
        const int lr = m_a[static_cast<std::size_t>(l)].right;
        m_a[static_cast<std::size_t>(p)].left = lr;
        if (lr >= 0) m_a[static_cast<std::size_t>(lr)].parent = p;
        const int pp = m_a[static_cast<std::size_t>(p)].parent;
        m_a[static_cast<std::size_t>(l)].parent = pp;
        if (pp < 0) { root = l; m_a[static_cast<std::size_t>(l)].red = false; }
        else if (m_a[static_cast<std::size_t>(pp)].right == p) m_a[static_cast<std::size_t>(pp)].right = l;
        else m_a[static_cast<std::size_t>(pp)].left = l;
        m_a[static_cast<std::size_t>(l)].right = p;
        m_a[static_cast<std::size_t>(p)].parent = l;
        return root;
    }

    int balanceInsertion(int root, int x) {
        m_a[static_cast<std::size_t>(x)].red = true;
        for (int xp, xpp, xppl, xppr;;) {
            if ((xp = m_a[static_cast<std::size_t>(x)].parent) < 0) {
                m_a[static_cast<std::size_t>(x)].red = false;
                return x;
            }
            if (!m_a[static_cast<std::size_t>(xp)].red || (xpp = m_a[static_cast<std::size_t>(xp)].parent) < 0) {
                return root;
            }
            if (xp == (xppl = m_a[static_cast<std::size_t>(xpp)].left)) {
                if ((xppr = m_a[static_cast<std::size_t>(xpp)].right) >= 0 && m_a[static_cast<std::size_t>(xppr)].red) {
                    m_a[static_cast<std::size_t>(xppr)].red = false;
                    m_a[static_cast<std::size_t>(xp)].red = false;
                    m_a[static_cast<std::size_t>(xpp)].red = true;
                    x = xpp;
                } else {
                    if (x == m_a[static_cast<std::size_t>(xp)].right) {
                        root = rotateLeft(root, x = xp);
                        xp = m_a[static_cast<std::size_t>(x)].parent;
                        xpp = xp < 0 ? -1 : m_a[static_cast<std::size_t>(xp)].parent;
                    }
                    if (xp >= 0) {
                        m_a[static_cast<std::size_t>(xp)].red = false;
                        if (xpp >= 0) {
                            m_a[static_cast<std::size_t>(xpp)].red = true;
                            root = rotateRight(root, xpp);
                        }
                    }
                }
            } else {
                if (xppl >= 0 && m_a[static_cast<std::size_t>(xppl)].red) {
                    m_a[static_cast<std::size_t>(xppl)].red = false;
                    m_a[static_cast<std::size_t>(xp)].red = false;
                    m_a[static_cast<std::size_t>(xpp)].red = true;
                    x = xpp;
                } else {
                    if (x == m_a[static_cast<std::size_t>(xp)].left) {
                        root = rotateRight(root, x = xp);
                        xp = m_a[static_cast<std::size_t>(x)].parent;
                        xpp = xp < 0 ? -1 : m_a[static_cast<std::size_t>(xp)].parent;
                    }
                    if (xp >= 0) {
                        m_a[static_cast<std::size_t>(xp)].red = false;
                        if (xpp >= 0) {
                            m_a[static_cast<std::size_t>(xpp)].red = true;
                            root = rotateLeft(root, xpp);
                        }
                    }
                }
            }
        }
    }

    void moveRootToFront(int root) {
        if (root < 0) return;
        const int n = static_cast<int>(m_tab.size());
        const int index = static_cast<int>(m_a[static_cast<std::size_t>(root)].hash & static_cast<std::uint32_t>(n - 1));
        const int first = m_tab[static_cast<std::size_t>(index)];
        if (root != first) {
            m_tab[static_cast<std::size_t>(index)] = root;
            const int rp = m_a[static_cast<std::size_t>(root)].prev;
            const int rn = m_a[static_cast<std::size_t>(root)].next;
            if (rn >= 0) m_a[static_cast<std::size_t>(rn)].prev = rp;
            if (rp >= 0) m_a[static_cast<std::size_t>(rp)].next = rn;
            if (first >= 0) m_a[static_cast<std::size_t>(first)].prev = root;
            m_a[static_cast<std::size_t>(root)].next = first;
            m_a[static_cast<std::size_t>(root)].prev = -1;
        }
    }

    void treeify(int i) {
        int root = -1;
        for (int x = m_tab[static_cast<std::size_t>(i)], next; x >= 0; x = next) {
            next = m_a[static_cast<std::size_t>(x)].next;
            m_a[static_cast<std::size_t>(x)].left = m_a[static_cast<std::size_t>(x)].right = -1;
            if (root < 0) {
                m_a[static_cast<std::size_t>(x)].parent = -1;
                m_a[static_cast<std::size_t>(x)].red = false;
                root = x;
            } else {
                const std::uint32_t h = m_a[static_cast<std::size_t>(x)].hash;
                const std::int32_t k = m_a[static_cast<std::size_t>(x)].key;
                for (int p = root;;) {
                    const int dir = dirOf(h, k, p);
                    const int xp = p;
                    p = dir <= 0 ? m_a[static_cast<std::size_t>(p)].left : m_a[static_cast<std::size_t>(p)].right;
                    if (p < 0) {
                        m_a[static_cast<std::size_t>(x)].parent = xp;
                        if (dir <= 0) m_a[static_cast<std::size_t>(xp)].left = x;
                        else m_a[static_cast<std::size_t>(xp)].right = x;
                        root = balanceInsertion(root, x);
                        break;
                    }
                }
            }
        }
        m_isTree[static_cast<std::size_t>(i)] = true;
        moveRootToFront(root);
    }

    void treeifyBin(int i) {
        if (static_cast<int>(m_tab.size()) < MIN_TREEIFY_CAPACITY) {
            resize();
            return;
        }
        int prev = -1;
        for (int e = m_tab[static_cast<std::size_t>(i)]; e >= 0; e = m_a[static_cast<std::size_t>(e)].next) {
            m_a[static_cast<std::size_t>(e)].prev = prev;
            prev = e;
        }
        treeify(i);
    }

    void putTreeVal(int i, std::int32_t key, std::uint32_t h, const V& value) {
        const int x = newNode(key, h, value);
        int root = m_tab[static_cast<std::size_t>(i)];
        for (int p = root;;) {
            const int dir = dirOf(h, key, p);
            const int xp = p;
            p = dir <= 0 ? m_a[static_cast<std::size_t>(p)].left : m_a[static_cast<std::size_t>(p)].right;
            if (p < 0) {
                const int xpn = m_a[static_cast<std::size_t>(xp)].next;
                if (dir <= 0) m_a[static_cast<std::size_t>(xp)].left = x;
                else m_a[static_cast<std::size_t>(xp)].right = x;
                m_a[static_cast<std::size_t>(xp)].next = x;
                m_a[static_cast<std::size_t>(x)].parent = m_a[static_cast<std::size_t>(x)].prev = xp;
                m_a[static_cast<std::size_t>(x)].next = xpn;
                if (xpn >= 0) m_a[static_cast<std::size_t>(xpn)].prev = x;
                moveRootToFront(balanceInsertion(root, x));
                return;
            }
        }
    }

    void untreeifyChain(int head) {
        for (int q = head; q >= 0; q = m_a[static_cast<std::size_t>(q)].next) {
            m_a[static_cast<std::size_t>(q)].prev = -1;
            m_a[static_cast<std::size_t>(q)].parent = m_a[static_cast<std::size_t>(q)].left = m_a[static_cast<std::size_t>(q)].right = -1;
            m_a[static_cast<std::size_t>(q)].red = false;
        }
    }

    void resize() {
        const int oldCap = static_cast<int>(m_tab.size());
        const int newCap = oldCap << 1;
        m_threshold <<= 1;
        std::vector<int>  oldTab    = m_tab;
        std::vector<bool> oldIsTree = m_isTree;
        m_tab.assign(static_cast<std::size_t>(newCap), -1);
        m_isTree.assign(static_cast<std::size_t>(newCap), false);
        for (int j = 0; j < oldCap; ++j) {
            const int e = oldTab[static_cast<std::size_t>(j)];
            if (e < 0) continue;
            const bool tree = oldIsTree[static_cast<std::size_t>(j)];
            int loHead = -1, loTail = -1, hiHead = -1, hiTail = -1, lc = 0, hc = 0;
            for (int q = e, next; q >= 0; q = next) {
                next = m_a[static_cast<std::size_t>(q)].next;
                m_a[static_cast<std::size_t>(q)].next = -1;
                if ((m_a[static_cast<std::size_t>(q)].hash & static_cast<std::uint32_t>(oldCap)) == 0) {
                    m_a[static_cast<std::size_t>(q)].prev = loTail;
                    if (loTail < 0) loHead = q; else m_a[static_cast<std::size_t>(loTail)].next = q;
                    loTail = q; ++lc;
                } else {
                    m_a[static_cast<std::size_t>(q)].prev = hiTail;
                    if (hiTail < 0) hiHead = q; else m_a[static_cast<std::size_t>(hiTail)].next = q;
                    hiTail = q; ++hc;
                }
            }
            if (!tree) {
                if (loHead >= 0) m_tab[static_cast<std::size_t>(j)] = loHead;
                if (hiHead >= 0) m_tab[static_cast<std::size_t>(j + oldCap)] = hiHead;
            } else {
                if (loHead >= 0) {
                    if (lc <= UNTREEIFY_THRESHOLD) {
                        m_tab[static_cast<std::size_t>(j)] = loHead;
                        untreeifyChain(loHead);
                    } else {
                        m_tab[static_cast<std::size_t>(j)] = loHead;
                        m_isTree[static_cast<std::size_t>(j)] = true;
                        if (hiHead >= 0) treeify(j);
                    }
                }
                if (hiHead >= 0) {
                    if (hc <= UNTREEIFY_THRESHOLD) {
                        m_tab[static_cast<std::size_t>(j + oldCap)] = hiHead;
                        untreeifyChain(hiHead);
                    } else {
                        m_tab[static_cast<std::size_t>(j + oldCap)] = hiHead;
                        m_isTree[static_cast<std::size_t>(j + oldCap)] = true;
                        if (loHead >= 0) treeify(j + oldCap);
                    }
                }
            }
        }
    }
};

// ---------------------------------------------------------------------------
// java.util.HashSet<Integer> iteration-order simulation. HashSet wraps a HashMap
// whose values are a dummy PRESENT object; iteration order == that HashMap's key
// order. We reuse JavaIntHashMap<bool>; default ctor (capacity 0) for HashSet(),
// or a positive initialCapacity for HashSet(initialCapacity).
class JavaIntHashSet {
public:
    explicit JavaIntHashSet(int initialCapacity = 0) : m_map(initialCapacity) {}
    // HashSet.add: returns true if newly added (map.put(key, PRESENT)==null).
    bool add(std::int32_t key) { return m_map.put(key, true); }
    bool contains(std::int32_t key) const { return m_map.containsKey(key); }
    int size() const { return m_map.size(); }
    void forEach(const std::function<void(std::int32_t)>& fn) const { m_map.forEachKey(fn); }
private:
    JavaIntHashMap<bool> m_map;
};

// ---------------------------------------------------------------------------
// com.google.common.collect.HashMultimap<Integer,Integer> (Guava 33.5.0-jre):
// HashMap<Integer, HashSet<Integer>>, each value HashSet created via
// Sets.newHashSetWithExpectedSize(2) == new HashSet<>(Maps.capacity(2)) ==
// new HashSet<>(3). put(k,v) lazily creates the value set on first insert.
// get(k) returns a live wrapper whose iteration delegates to the backing HashSet,
// so the iteration order == HashSet<Integer>(3) order.
//
// The OUTER map (key -> set) is also a HashMap; but DependencySorter only ever
// reads it via get(id) which is order-independent, so outer order is irrelevant.
class IntHashMultimap {
public:
    // put(from, to); creates the value set on demand. Returns true if (from,to) was
    // not already present (matches Guava HashMultimap.put semantics, though the
    // caller ignores the return).
    bool put(std::int32_t from, std::int32_t to) {
        JavaIntHashSet* set = nullptr;
        for (auto& kv : m_entries) {
            if (kv.key == from) { set = &kv.set; break; }
        }
        if (set == nullptr) {
            m_entries.push_back(Bucket{from, JavaIntHashSet(VALUE_SET_EXPECTED_CAPACITY)});
            set = &m_entries.back().set;
        }
        return set->add(to);
    }

    // get(to): true if `from` is among directDependencies.get(to). Used by isCyclic.
    bool contains(std::int32_t key, std::int32_t value) const {
        for (const auto& kv : m_entries) {
            if (kv.key == key) return kv.set.contains(value);
        }
        return false;
    }

    // directDependencies.get(id).forEach(...) in HashSet iteration order. Returns
    // the empty set (no-op) when key is absent (Guava get() returns an empty view).
    void forEachValue(std::int32_t key, const std::function<void(std::int32_t)>& fn) const {
        for (const auto& kv : m_entries) {
            if (kv.key == key) { kv.set.forEach(fn); return; }
        }
    }

private:
    // Maps.capacity(2)=3 -> new HashSet<>(3). The JavaIntHashSet ctor takes the
    // same "initialCapacity" HashSet would (it computes tableSizeFor internally).
    static constexpr int VALUE_SET_EXPECTED_CAPACITY = 3;
    struct Bucket { std::int32_t key; JavaIntHashSet set; };
    std::vector<Bucket> m_entries;   // outer order irrelevant (only get() is used)
};

// ---------------------------------------------------------------------------
// net.minecraft.util.DependencySorter<Integer, Entry>.
//
// Entry exposes the required/optional dependency lists in the SAME order the Java
// Entry.visitRequiredDependencies / visitOptionalDependencies callbacks emit them
// (the consumer is invoked once per dependency, in list order).
struct IntDependencyEntry {
    std::vector<std::int32_t> requiredDeps;   // visitRequiredDependencies order
    std::vector<std::int32_t> optionalDeps;   // visitOptionalDependencies order
};

class IntDependencySorter {
public:
    IntDependencySorter& addEntry(std::int32_t id, const IntDependencyEntry& value) {
        // HashMap.put: insert or replace (later put on same id overwrites; first
        // insertion fixes the bucket slot, matching java.util.HashMap).
        m_contents.put(id, value);
        return *this;
    }

    // orderByDependencies: invokes output(id) in the exact JVM visitation order.
    void orderByDependencies(const std::function<void(std::int32_t, const IntDependencyEntry&)>& output) const {
        IntHashMultimap directDependencies;

        // contents.forEach(... visitRequiredDependencies ...)
        m_contents.forEach([&](std::int32_t id, const IntDependencyEntry& value) {
            for (std::int32_t dep : value.requiredDeps) {
                addDependencyIfNotCyclic(directDependencies, id, dep);
            }
        });
        // contents.forEach(... visitOptionalDependencies ...)
        m_contents.forEach([&](std::int32_t id, const IntDependencyEntry& value) {
            for (std::int32_t dep : value.optionalDeps) {
                addDependencyIfNotCyclic(directDependencies, id, dep);
            }
        });

        JavaIntHashSet alreadyVisited;
        m_contents.forEachKey([&](std::int32_t topId) {
            visitDependenciesAndElement(directDependencies, alreadyVisited, topId, output);
        });
    }

private:
    JavaIntHashMap<IntDependencyEntry> m_contents;

    void visitDependenciesAndElement(
            const IntHashMultimap& dependencies,
            JavaIntHashSet& alreadyVisited,
            std::int32_t id,
            const std::function<void(std::int32_t, const IntDependencyEntry&)>& output) const {
        if (alreadyVisited.add(id)) {
            dependencies.forEachValue(id, [&](std::int32_t dep) {
                visitDependenciesAndElement(dependencies, alreadyVisited, dep, output);
            });
            const IntDependencyEntry* current = m_contents.get(id);
            if (current != nullptr) {
                output(id, *current);
            }
        }
    }

    static bool isCyclic(const IntHashMultimap& directDependencies, std::int32_t from, std::int32_t to) {
        // dependencies = directDependencies.get(to); contains(from) ? true :
        //   dependencies.stream().anyMatch(dep -> isCyclic(d, from, dep));
        if (directDependencies.contains(to, from)) return true;
        bool any = false;
        directDependencies.forEachValue(to, [&](std::int32_t dep) {
            if (!any && isCyclic(directDependencies, from, dep)) any = true;
        });
        return any;
    }

    static void addDependencyIfNotCyclic(IntHashMultimap& directDependencies, std::int32_t from, std::int32_t to) {
        if (!isCyclic(directDependencies, from, to)) {
            directDependencies.put(from, to);
        }
    }
};

}  // namespace mc::util

#endif  // MCPP_UTIL_DEPENDENCYSORTER_H

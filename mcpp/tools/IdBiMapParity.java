// Ground truth for net.minecraft.util.CrudeIncrementalIntIdentityHashBiMap — the
// registry id<->object bimap used in network id mapping. We drive the REAL class
// (via reflection for the private constructor/methods) with deterministic
// sequences of distinct Object keys, capturing each object's
// System.identityHashCode (the exact int Java feeds into its private hash()). That
// identity-hash is emitted in every row so the C++ port can replay the IDENTICAL
// insertion sequence and reproduce probe order / slot placement / grow timing /
// assigned ids bit-for-bit (the map depends only on identity-hash + ref-equality,
// never on the stored value).
//
// Row TAGs (all ints decimal, tab-separated):
//   KEY   <scenario> <opaqueId> <identityHash>            one row per distinct key, in driven order
//   ADD   <scenario> <opaqueId> <assignedId>             result of map.add(key)
//   MAP   <scenario> <opaqueId> <explicitId>             a map.addMapping(key, explicitId) was performed
//   GETID <scenario> <opaqueId> <getId>                  map.getId(key)  (-1 == NOT_FOUND)
//   BYID  <scenario> <queryId> <opaqueIdOrMinus1>        opaqueId of map.byId(queryId), or -1 if null
//   SIZE  <scenario> <size> <nextId>                     map.size() and the private nextId field
//   CAP   <scenario> <capacity>                          keys.length (capacity), after the scenario
//
// The C++ side rebuilds each scenario from KEY/ADD/MAP rows (which fully specify
// the input sequence) and re-derives GETID/BYID/SIZE/CAP, comparing to GT.

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.util.CrudeIncrementalIntIdentityHashBiMap;

public class IdBiMapParity {
    static final java.io.PrintStream O = System.out;

    // Reflection handles into the real class.
    static Class<?> CLS;
    static Method M_create;     // static create(int)
    static Method M_add;        // add(Object) -> int
    static Method M_addMapping; // addMapping(Object,int)
    static Method M_getId;      // getId(Object) -> int
    static Method M_byId;       // byId(int) -> Object
    static Method M_size;       // size() -> int
    static Field  F_nextId;     // private int nextId
    static Field  F_keys;       // private Object[] keys

    @SuppressWarnings("unchecked")
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        CLS = CrudeIncrementalIntIdentityHashBiMap.class;
        M_create = CLS.getDeclaredMethod("create", int.class);
        M_create.setAccessible(true);
        M_add = CLS.getDeclaredMethod("add", Object.class);
        M_add.setAccessible(true);
        M_addMapping = CLS.getDeclaredMethod("addMapping", Object.class, int.class);
        M_addMapping.setAccessible(true);
        M_getId = CLS.getDeclaredMethod("getId", Object.class);
        M_getId.setAccessible(true);
        M_byId = CLS.getDeclaredMethod("byId", int.class);
        M_byId.setAccessible(true);
        M_size = CLS.getDeclaredMethod("size");
        M_size.setAccessible(true);
        F_nextId = CLS.getDeclaredField("nextId");
        F_nextId.setAccessible(true);
        F_keys = CLS.getDeclaredField("keys");
        F_keys.setAccessible(true);

        scenarioBasicAdds();
        scenarioGrowMany();
        scenarioAddMappingExplicit();
        scenarioAddMappingSparseIds();
        scenarioLookupMisses();
        scenarioClearAndReuseViaCopyOfSequence();

        O.flush();
    }

    // A driver that mirrors the map and tracks key->opaqueId so we can resolve
    // byId(...) results back to opaque ids and emit the KEY/ADD/MAP input stream.
    static final class Driver {
        final Object map;
        final java.util.IdentityHashMap<Object, Integer> opaqueOf = new java.util.IdentityHashMap<>();
        final java.util.ArrayList<Object> keysInOrder = new java.util.ArrayList<>();
        final String scen;
        int nextOpaque = 0;

        Driver(String scen, int initialCapacity) throws Exception {
            this.scen = scen;
            this.map = M_create.invoke(null, initialCapacity);
        }

        // Mint a brand-new distinct object, register its opaque id + identityHash,
        // emit the KEY row.
        Object newKey() {
            Object k = new Object();
            int op = nextOpaque++;
            opaqueOf.put(k, op);
            keysInOrder.add(k);
            O.println("KEY\t" + scen + "\t" + op + "\t" + System.identityHashCode(k));
            return k;
        }

        int opaque(Object k) { return k == null ? -1 : opaqueOf.get(k); }

        int add(Object k) throws Exception {
            int id = (Integer) M_add.invoke(map, k);
            O.println("ADD\t" + scen + "\t" + opaque(k) + "\t" + id);
            return id;
        }

        void addMapping(Object k, int id) throws Exception {
            M_addMapping.invoke(map, k, id);
            O.println("MAP\t" + scen + "\t" + opaque(k) + "\t" + id);
        }

        void getId(Object k) throws Exception {
            int v = (Integer) M_getId.invoke(map, k);
            O.println("GETID\t" + scen + "\t" + opaque(k) + "\t" + v);
        }

        void byId(int id) throws Exception {
            Object r = M_byId.invoke(map, id);
            O.println("BYID\t" + scen + "\t" + id + "\t" + (r == null ? -1 : opaque(r)));
        }

        void dumpSizeAndCap() throws Exception {
            int sz = (Integer) M_size.invoke(map);
            int nid = F_nextId.getInt(map);
            O.println("SIZE\t" + scen + "\t" + sz + "\t" + nid);
            Object[] keys = (Object[]) F_keys.get(map);
            O.println("CAP\t" + scen + "\t" + keys.length);
        }
    }

    // 1) Basic sequential adds; verify getId round-trips and byId resolves.
    static void scenarioBasicAdds() throws Exception {
        Driver d = new Driver("basic", 16);
        Object[] ks = new Object[10];
        for (int i = 0; i < ks.length; i++) { ks[i] = d.newKey(); d.add(ks[i]); }
        for (int i = 0; i < ks.length; i++) d.getId(ks[i]);
        for (int i = 0; i < ks.length + 3; i++) d.byId(i);
        d.dumpSizeAndCap();
    }

    // 2) Many adds to force one or more grows (cross the 0.8 load factor).
    static void scenarioGrowMany() throws Exception {
        Driver d = new Driver("grow", 4);
        Object[] ks = new Object[300];
        for (int i = 0; i < ks.length; i++) { ks[i] = d.newKey(); d.add(ks[i]); }
        // round-trip every key through getId after the grows
        for (int i = 0; i < ks.length; i++) d.getId(ks[i]);
        // byId across the full range plus a few past the end
        for (int i = 0; i < ks.length + 5; i++) d.byId(i);
        d.dumpSizeAndCap();
    }

    // 3) addMapping with explicit, in-order ids matching what add would assign.
    static void scenarioAddMappingExplicit() throws Exception {
        Driver d = new Driver("mapseq", 8);
        Object[] ks = new Object[20];
        for (int i = 0; i < ks.length; i++) { ks[i] = d.newKey(); d.addMapping(ks[i], i); }
        for (int i = 0; i < ks.length; i++) d.getId(ks[i]);
        for (int i = 0; i < ks.length + 2; i++) d.byId(i);
        d.dumpSizeAndCap();
    }

    // 4) addMapping with SPARSE / out-of-order ids: forces grow-to-fit-id, nextId
    //    gaps, and the Math.max(id, size+1) load-factor branch.
    static void scenarioAddMappingSparseIds() throws Exception {
        Driver d = new Driver("sparse", 8);
        // ids chosen to leave gaps and to occasionally jump high (forcing newSize<<1
        // loops in addMapping's grow path)
        int[] ids = { 0, 5, 2, 9, 3, 40, 7, 1, 12, 100, 4 };
        Object[] ks = new Object[ids.length];
        for (int i = 0; i < ids.length; i++) {
            ks[i] = d.newKey();
            d.addMapping(ks[i], ids[i]);
        }
        for (int i = 0; i < ks.length; i++) d.getId(ks[i]);
        for (int i = 0; i <= 101; i++) d.byId(i);
        d.dumpSizeAndCap();
        // interleave a couple of add() calls — they should fill the lowest free ids
        Object a = d.newKey(); d.add(a);
        Object b = d.newKey(); d.add(b);
        d.getId(a); d.getId(b);
        d.dumpSizeAndCap();
    }

    // 5) Lookups of keys that were never inserted must return NOT_FOUND (-1), and
    //    byId of out-of-range / empty ids must return null (-1 opaque).
    static void scenarioLookupMisses() throws Exception {
        Driver d = new Driver("miss", 16);
        Object[] ins = new Object[5];
        for (int i = 0; i < ins.length; i++) { ins[i] = d.newKey(); d.add(ins[i]); }
        // never-inserted keys (still minted so their identityHash is recorded)
        Object[] absent = new Object[4];
        for (int i = 0; i < absent.length; i++) absent[i] = d.newKey();
        for (Object a : absent) d.getId(a);
        // out of range byId
        d.byId(-1);
        d.byId(5);
        d.byId(1000);
        d.dumpSizeAndCap();
    }

    // 6) A longer mixed sequence (adds then targeted addMapping ids), the kind of
    //    pattern registries actually produce, to stress probe wraparound after
    //    several grows.
    static void scenarioClearAndReuseViaCopyOfSequence() throws Exception {
        Driver d = new Driver("mixed", 6);
        Object[] ks = new Object[64];
        for (int i = 0; i < ks.length; i++) {
            ks[i] = d.newKey();
            if (i % 7 == 3) d.addMapping(ks[i], 200 + i);  // occasional far-away id
            else d.add(ks[i]);
        }
        for (int i = 0; i < ks.length; i++) d.getId(ks[i]);
        for (int i = 0; i < 270; i++) d.byId(i);
        d.dumpSizeAndCap();
    }
}

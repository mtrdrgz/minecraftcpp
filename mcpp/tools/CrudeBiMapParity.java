// Ground truth for net.minecraft.util.CrudeIncrementalIntIdentityHashBiMap — the
// open-addressing growable int<->object identity bimap used in registry / network
// id mapping. We drive the REAL class (private ctor + methods reached via
// reflection.setAccessible) with deterministic sequences of distinct Object keys,
// recording each object's System.identityHashCode — the exact int Java feeds into
// its private hash(): (Mth.murmurHash3Mixer(identityHashCode) & 0x7fffffff) % len.
//
// That identity-hash plus reference equality (keys[i] == key) are the ONLY inputs
// to probe order / slot placement / grow timing / assigned ids — the stored value
// is irrelevant. So we emit, per distinct key, a KEY row carrying its opaque token
// + identityHash; the C++ port replays the identical sequence and reproduces every
// observable result bit-for-bit. (Source: CrudeIncrementalIntIdentityHashBiMap.java
// lines 60-156; create() line 34-36; hash() line 114-116.)
//
// Every scenario starts with a NEW row carrying the exact create(initialCapacity)
// argument, so the C++ test is fully data-driven (no hard-coded scenario table).
//
// Row TAGs (all ints decimal, tab-separated, STDOUT only):
//   NEW   <scenario> <initialCapacity>                  create(initialCapacity); resets the live map
//   KEY   <scenario> <opaqueId> <identityHash>          one row per distinct minted key, in driven order
//   ADD   <scenario> <opaqueId> <assignedId>            result of map.add(key)
//   MAP   <scenario> <opaqueId> <explicitId>            a map.addMapping(key, explicitId) was performed
//   GETID <scenario> <opaqueId> <getId>                 map.getId(key)   (-1 == NOT_FOUND)
//   BYID  <scenario> <queryId> <opaqueIdOrMinus1>       opaqueId of map.byId(queryId), or -1 if null
//   CONT  <scenario> <opaqueId> <0|1>                   map.contains(key)
//   CONTI <scenario> <queryId> <0|1>                    map.contains(id)
//   SIZE  <scenario> <size> <nextId>                    map.size() and the private nextId field
//   CAP   <scenario> <capacity>                         keys.length (capacity)

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.util.CrudeIncrementalIntIdentityHashBiMap;

public class CrudeBiMapParity {
    static final java.io.PrintStream O = System.out;

    static Class<?> CLS;
    static Method M_create;     // static create(int) -> map
    static Method M_add;        // add(Object) -> int
    static Method M_addMapping; // addMapping(Object,int)
    static Method M_getId;      // getId(Object) -> int
    static Method M_byId;       // byId(int) -> Object
    static Method M_size;       // size() -> int
    static Method M_containsK;  // contains(Object) -> boolean
    static Method M_containsI;  // contains(int) -> boolean
    static Field  F_nextId;     // private int nextId
    static Field  F_keys;       // private Object[] keys

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
        M_containsK = CLS.getDeclaredMethod("contains", Object.class);
        M_containsK.setAccessible(true);
        M_containsI = CLS.getDeclaredMethod("contains", int.class);
        M_containsI.setAccessible(true);
        F_nextId = CLS.getDeclaredField("nextId");
        F_nextId.setAccessible(true);
        F_keys = CLS.getDeclaredField("keys");
        F_keys.setAccessible(true);

        scenarioSeqAdds();
        scenarioForceGrows();
        scenarioMappingInOrder();
        scenarioMappingSparse();
        scenarioMissesAndContains();
        scenarioInterleaved();

        O.flush();
    }

    // Mirrors the real map and tracks key->opaqueId so byId(...) results resolve to
    // opaque ids and we emit the KEY/ADD/MAP input stream.
    static final class Driver {
        final Object map;
        final java.util.IdentityHashMap<Object, Integer> opaqueOf = new java.util.IdentityHashMap<>();
        final String scen;
        int nextOpaque = 0;

        Driver(String scen, int initialCapacity) throws Exception {
            this.scen = scen;
            this.map = M_create.invoke(null, initialCapacity);
            O.println("NEW\t" + scen + "\t" + initialCapacity);
        }

        Object newKey() {
            Object k = new Object();
            int op = nextOpaque++;
            opaqueOf.put(k, op);
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

        void containsKey(Object k) throws Exception {
            boolean b = (Boolean) M_containsK.invoke(map, k);
            O.println("CONT\t" + scen + "\t" + opaque(k) + "\t" + (b ? 1 : 0));
        }

        void containsId(int id) throws Exception {
            boolean b = (Boolean) M_containsI.invoke(map, id);
            O.println("CONTI\t" + scen + "\t" + id + "\t" + (b ? 1 : 0));
        }

        void dumpSizeAndCap() throws Exception {
            int sz = (Integer) M_size.invoke(map);
            int nid = F_nextId.getInt(map);
            O.println("SIZE\t" + scen + "\t" + sz + "\t" + nid);
            Object[] keys = (Object[]) F_keys.get(map);
            O.println("CAP\t" + scen + "\t" + keys.length);
        }
    }

    // 1) Sequential add() of distinct keys; round-trip getId and byId, with contains.
    static void scenarioSeqAdds() throws Exception {
        Driver d = new Driver("seq", 12);
        Object[] ks = new Object[14];
        for (int i = 0; i < ks.length; i++) { ks[i] = d.newKey(); d.add(ks[i]); }
        for (int i = 0; i < ks.length; i++) { d.getId(ks[i]); d.containsKey(ks[i]); }
        for (int i = 0; i < ks.length + 4; i++) { d.byId(i); d.containsId(i); }
        d.dumpSizeAndCap();
    }

    // 2) Many sequential adds to cross the 0.8 load factor repeatedly (several grows).
    static void scenarioForceGrows() throws Exception {
        Driver d = new Driver("grows", 3);
        Object[] ks = new Object[256];
        for (int i = 0; i < ks.length; i++) { ks[i] = d.newKey(); d.add(ks[i]); }
        for (int i = 0; i < ks.length; i++) d.getId(ks[i]);
        for (int i = 0; i < ks.length + 6; i++) d.byId(i);
        d.dumpSizeAndCap();
    }

    // 3) addMapping with explicit in-order ids equal to what add would assign.
    static void scenarioMappingInOrder() throws Exception {
        Driver d = new Driver("mapord", 10);
        Object[] ks = new Object[24];
        for (int i = 0; i < ks.length; i++) { ks[i] = d.newKey(); d.addMapping(ks[i], i); }
        for (int i = 0; i < ks.length; i++) d.getId(ks[i]);
        for (int i = 0; i < ks.length + 3; i++) d.byId(i);
        d.dumpSizeAndCap();
    }

    // 4) addMapping with SPARSE / out-of-order ids: exercises grow-to-fit-id (the
    //    newSize<<1 loop), nextId gaps, and Math.max(id,size+1) load-factor branch.
    static void scenarioMappingSparse() throws Exception {
        Driver d = new Driver("sparse", 7);
        int[] ids = { 0, 4, 1, 11, 2, 33, 6, 3, 17, 80, 5, 200, 8 };
        Object[] ks = new Object[ids.length];
        for (int i = 0; i < ids.length; i++) { ks[i] = d.newKey(); d.addMapping(ks[i], ids[i]); }
        for (int i = 0; i < ks.length; i++) d.getId(ks[i]);
        for (int i = 0; i <= 201; i++) d.byId(i);
        d.dumpSizeAndCap();
        // a few add() calls now fill the lowest free ids (nextId skips occupied slots)
        Object a = d.newKey(); d.add(a);
        Object b = d.newKey(); d.add(b);
        Object c = d.newKey(); d.add(c);
        d.getId(a); d.getId(b); d.getId(c);
        d.dumpSizeAndCap();
    }

    // 5) Lookup misses: never-inserted keys must return -1; byId/contains out of
    //    range must be null/false.
    static void scenarioMissesAndContains() throws Exception {
        Driver d = new Driver("miss", 16);
        Object[] ins = new Object[6];
        for (int i = 0; i < ins.length; i++) { ins[i] = d.newKey(); d.add(ins[i]); }
        Object[] absent = new Object[5];
        for (int i = 0; i < absent.length; i++) absent[i] = d.newKey(); // minted, never inserted
        for (Object a : absent) { d.getId(a); d.containsKey(a); }
        d.byId(-1);   d.containsId(-1);
        d.byId(6);    d.containsId(6);
        d.byId(1000); d.containsId(1000);
        d.dumpSizeAndCap();
    }

    // 6) Longer mixed adds + occasional far-away addMapping ids, stressing probe
    //    wraparound after multiple grows.
    static void scenarioInterleaved() throws Exception {
        Driver d = new Driver("mixed", 5);
        Object[] ks = new Object[72];
        for (int i = 0; i < ks.length; i++) {
            ks[i] = d.newKey();
            if (i % 9 == 4) d.addMapping(ks[i], 300 + i);
            else d.add(ks[i]);
        }
        for (int i = 0; i < ks.length; i++) d.getId(ks[i]);
        for (int i = 0; i < 380; i++) d.byId(i);
        d.dumpSizeAndCap();
    }
}

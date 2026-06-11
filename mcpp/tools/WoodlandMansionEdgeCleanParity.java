// Ground-truth generator for the PURE grid-cleanup pass nested in the REAL
// decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces
//     -> private static class MansionGrid
//          static  boolean isHouse(SimpleGrid grid, int x, int y)
//          (instance) boolean cleanEdges(SimpleGrid grid)
//
// Both helpers are fully self-contained integer geometry over a single SimpleGrid:
// NO world writes, NO RandomSource, NO registry/datapack, NO Direction. cleanEdges
// reads no instance state of MansionGrid (only its `grid` arg + the static
// isHouse), so we invoke it on a MansionGrid obtained WITHOUT running the ctor
// (sun.misc.Unsafe.allocateInstance) — the ctor would need a RandomSource and run
// the whole mansion algorithm, which we deliberately do NOT exercise here.
//
//   tools/run_groundtruth.ps1 -Tool WoodlandMansionEdgeCleanParity -Out mcpp/build/woodland_mansion_edge_clean.tsv
//
// All ops are an explicit deterministic script (no RandomSource) so the C++ side
// walks identical grid state. Both sides mutate ONE grid in place across passes.
//
// Line formats (all ints decimal):
//   GRID  <gid> <w> <h> <outside>                 -- (re)create a fresh SimpleGrid
//   SET   <gid> <x> <y> <value>                   -- single-cell set
//   RECT  <gid> <x0> <y0> <x1> <y1> <value>       -- rectangle set
//   HOUSE <gid> <x> <y> <result0or1>              -- probe MansionGrid.isHouse (REAL)
//   CLEAN <gid> <touched0or1>                      -- run ONE cleanEdges pass (REAL); result = its return
//   GET   <gid> <x> <y> <result>                  -- probe SimpleGrid.get after the preceding op

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;

public class WoodlandMansionEdgeCleanParity {
    static final java.io.PrintStream OUT = System.out;

    static Class<?> GRID_CLS;          // WoodlandMansionPieces$SimpleGrid
    static Class<?> MANSION_CLS;       // WoodlandMansionPieces$MansionGrid
    static Constructor<?> GRID_CTOR;
    static Method M_set3, M_set5, M_get;
    static Method M_isHouse, M_cleanEdges;
    static Object MANSION;             // unconstructed MansionGrid instance for cleanEdges

    static final Map<Integer, Object> GRIDS = new HashMap<>();

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> outer = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces");
        for (Class<?> c : outer.getDeclaredClasses()) {
            String n = c.getSimpleName();
            if (n.equals("SimpleGrid")) GRID_CLS = c;
            else if (n.equals("MansionGrid")) MANSION_CLS = c;
        }
        if (GRID_CLS == null) throw new IllegalStateException("SimpleGrid not found");
        if (MANSION_CLS == null) throw new IllegalStateException("MansionGrid not found");

        GRID_CTOR = GRID_CLS.getDeclaredConstructor(int.class, int.class, int.class);
        GRID_CTOR.setAccessible(true);
        M_set3 = GRID_CLS.getDeclaredMethod("set", int.class, int.class, int.class);
        M_set3.setAccessible(true);
        M_set5 = GRID_CLS.getDeclaredMethod("set", int.class, int.class, int.class, int.class, int.class);
        M_set5.setAccessible(true);
        M_get = GRID_CLS.getDeclaredMethod("get", int.class, int.class);
        M_get.setAccessible(true);

        M_isHouse = MANSION_CLS.getDeclaredMethod("isHouse", GRID_CLS, int.class, int.class);
        M_isHouse.setAccessible(true);
        M_cleanEdges = MANSION_CLS.getDeclaredMethod("cleanEdges", GRID_CLS);
        M_cleanEdges.setAccessible(true);

        // cleanEdges is a non-static method but touches no instance state. Build a
        // MansionGrid WITHOUT running its ctor (which needs a RandomSource and runs
        // the full mansion algorithm) via sun.misc.Unsafe.allocateInstance.
        MANSION = allocateUnconstructed(MANSION_CLS);

        int gid = 0;
        // Real mansion shape: 11x11, valueIfOutside=5 (BLOCKED, not a "house").
        gid = realMansionEntrancePattern(gid);
        // Synthetic patterns that exercise every branch + in-place mutation order.
        gid = pattern_plus(gid);          // a plus carves CLEAR pockets with 2/3 direct nbrs
        gid = pattern_ring(gid);          // hollow ring -> interior gets filled by sweep
        gid = pattern_diagonalGuard(gid); // forces the ==2 / diagonal<=1 branch both ways
        gid = pattern_edgesAndCorners(gid); // OOB sentinel participation at borders
        gid = pattern_altSentinel(gid);   // valueIfOutside in 1..4 makes OOB count as house
        gid = pattern_dense(gid);         // near-full grid, repeated passes to fixed point

        OUT.flush();
    }

    // ── pattern builders ─────────────────────────────────────────────────────

    static int realMansionEntrancePattern(int gid) throws Exception {
        // The exact carve the real MansionGrid ctor lays down before its
        // `while (cleanEdges(baseGrid)) {}` loop (recursiveCorridor uses RandomSource
        // and is intentionally NOT reproduced; we seed only the deterministic sets).
        Object g = newGrid(gid, 11, 11, 5);
        int ex = 7, ey = 4;
        rect(gid, g, ex, ey, ex + 1, ey + 1, 3);
        rect(gid, g, ex - 1, ey, ex - 1, ey + 1, 2);
        rect(gid, g, ex + 2, ey - 2, ex + 3, ey + 3, 5);
        rect(gid, g, ex + 1, ey - 2, ex + 1, ey - 1, 1);
        rect(gid, g, ex + 1, ey + 2, ex + 1, ey + 3, 1);
        set(gid, g, ex - 1, ey - 1, 1);
        set(gid, g, ex - 1, ey + 2, 1);
        rect(gid, g, 0, 0, 11, 1, 5);
        rect(gid, g, 0, 9, 11, 11, 5);
        runToFixedPoint(gid, g);
        return gid + 1;
    }

    static int pattern_plus(int gid) throws Exception {
        Object g = newGrid(gid, 7, 7, 5);
        // a plus of CORRIDOR(1) cells around the center
        set(gid, g, 3, 1, 1);
        set(gid, g, 3, 2, 1);
        set(gid, g, 3, 4, 1);
        set(gid, g, 3, 5, 1);
        set(gid, g, 1, 3, 1);
        set(gid, g, 2, 3, 1);
        set(gid, g, 4, 3, 1);
        set(gid, g, 5, 3, 1);
        runToFixedPoint(gid, g);
        return gid + 1;
    }

    static int pattern_ring(int gid) throws Exception {
        Object g = newGrid(gid, 9, 9, 5);
        // hollow square ring of ROOM(2)
        rect(gid, g, 2, 2, 6, 2, 2);
        rect(gid, g, 2, 6, 6, 6, 2);
        rect(gid, g, 2, 2, 2, 6, 2);
        rect(gid, g, 6, 2, 6, 6, 2);
        runToFixedPoint(gid, g);
        return gid + 1;
    }

    static int pattern_diagonalGuard(int gid) throws Exception {
        Object g = newGrid(gid, 8, 8, 5);
        // L-shapes / corners giving CLEAR cells exactly 2 direct neighbours with
        // 0,1, or 2 diagonals -> exercises the diagonalNeighbors<=1 guard both ways.
        set(gid, g, 1, 1, 1);
        set(gid, g, 2, 1, 1);
        set(gid, g, 1, 2, 1);   // (2,2) clear has 2 direct, 1 diagonal -> filled
        set(gid, g, 4, 4, 2);
        set(gid, g, 5, 4, 2);
        set(gid, g, 4, 5, 2);
        set(gid, g, 5, 5, 2);   // (between) corner cells: 2 direct, 2 diagonal -> NOT filled
        set(gid, g, 6, 1, 3);
        set(gid, g, 6, 2, 4);   // mix of START_ROOM/TEST_ROOM as house values
        runToFixedPoint(gid, g);
        return gid + 1;
    }

    static int pattern_edgesAndCorners(int gid) throws Exception {
        Object g = newGrid(gid, 5, 5, 5);
        // houses pushed against the border so OOB sentinel (5, not a house) matters
        set(gid, g, 0, 1, 1);
        set(gid, g, 1, 0, 1);
        set(gid, g, 0, 0, 0);   // corner CLEAR: 2 in-grid direct nbrs, OOB diagonals
        set(gid, g, 4, 3, 2);
        set(gid, g, 3, 4, 2);
        runToFixedPoint(gid, g);
        return gid + 1;
    }

    static int pattern_altSentinel(int gid) throws Exception {
        // valueIfOutside in 1..4 => OOB reads ARE "house"; border CLEAR cells then
        // see phantom neighbours. Locks down that isHouse is sentinel-driven via get.
        Object g = newGrid(gid, 4, 4, 2);
        set(gid, g, 1, 1, 0);
        set(gid, g, 2, 2, 0);
        set(gid, g, 2, 1, 1);
        runToFixedPoint(gid, g);
        return gid + 1;
    }

    static int pattern_dense(int gid) throws Exception {
        Object g = newGrid(gid, 6, 6, 5);
        // checkerboard-ish of houses so the in-place sweep cascades across passes
        for (int y = 0; y < 6; y++)
            for (int x = 0; x < 6; x++)
                if (((x + y) & 1) == 0 && !(x == 3 && y == 3)) set(gid, g, x, y, 2);
        runToFixedPoint(gid, g);
        return gid + 1;
    }

    // ── helpers ──────────────────────────────────────────────────────────────

    static Object newGrid(int gid, int w, int h, int outside) throws Exception {
        Object g = GRID_CTOR.newInstance(w, h, outside);
        GRIDS.put(gid, g);
        OUT.println("GRID\t" + gid + "\t" + w + "\t" + h + "\t" + outside);
        return g;
    }

    static void set(int gid, Object g, int x, int y, int v) throws Exception {
        M_set3.invoke(g, x, y, v);
        OUT.println("SET\t" + gid + "\t" + x + "\t" + y + "\t" + v);
    }

    static void rect(int gid, Object g, int x0, int y0, int x1, int y1, int v) throws Exception {
        M_set5.invoke(g, x0, y0, x1, y1, v);
        OUT.println("RECT\t" + gid + "\t" + x0 + "\t" + y0 + "\t" + x1 + "\t" + y1 + "\t" + v);
    }

    // Run cleanEdges to a fixed point (as the real ctor does), emitting after EACH
    // pass: the CLEAN return value, a full grid GET dump (extended OOB ring), and
    // HOUSE probes over the same ring. Both sides see identical mutating state.
    static void runToFixedPoint(int gid, Object g) throws Exception {
        // Probe the freshly-seeded grid before any cleaning.
        dump(gid, g);
        for (int pass = 0; pass < 64; pass++) {
            boolean touched = (Boolean) M_cleanEdges.invoke(MANSION, g);
            OUT.println("CLEAN\t" + gid + "\t" + (touched ? 1 : 0));
            dump(gid, g);
            if (!touched) break;
        }
    }

    static void dump(int gid, Object g) throws Exception {
        int w = getInt(g, "width");
        int h = getInt(g, "height");
        for (int y = -1; y <= h; y++) {
            for (int x = -1; x <= w; x++) {
                int got = (Integer) M_get.invoke(g, x, y);
                OUT.println("GET\t" + gid + "\t" + x + "\t" + y + "\t" + got);
                boolean house = (Boolean) M_isHouse.invoke(null, g, x, y);
                OUT.println("HOUSE\t" + gid + "\t" + x + "\t" + y + "\t" + (house ? 1 : 0));
            }
        }
    }

    static int getInt(Object g, String field) throws Exception {
        Field f = GRID_CLS.getDeclaredField(field);
        f.setAccessible(true);
        return f.getInt(g);
    }

    // sun.misc.Unsafe.allocateInstance(cls): construct without running any ctor.
    static Object allocateUnconstructed(Class<?> cls) throws Exception {
        Field theUnsafe = Class.forName("sun.misc.Unsafe").getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        Object unsafe = theUnsafe.get(null);
        Method alloc = unsafe.getClass().getMethod("allocateInstance", Class.class);
        return alloc.invoke(unsafe, cls);
    }
}

// Ground-truth generator for the PURE 2D-grid helper nested in the REAL
// decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces
//     -> private static class SimpleGrid
//
// SimpleGrid is fully self-contained integer geometry: a width*height int grid
// with a `valueIfOutside` sentinel. NO world writes, NO RandomSource, NO
// registry/datapack, NO Direction. We exercise it via reflection (the class is
// private static; its enclosing <clinit> touches Blocks, so we Bootstrap first).
//
//   tools/run_groundtruth.ps1 -Tool WoodlandMansionGridParity -Out mcpp/build/woodland_mansion_grid.tsv
//
// Protocol: each emitted line is a self-describing OP record OR a probe RESULT.
// The C++ side replays the SAME op script against its own SimpleGrid and compares
// every probe. We keep ops as an explicit, deterministic script (no RandomSource)
// so both sides walk identical state.
//
// Line formats (all ints decimal):
//   GRID  <gridId> <width> <height> <valueIfOutside>          -- (re)create a fresh grid
//   SET   <gridId> <x> <y> <value>                            -- single-cell set
//   RECT  <gridId> <x0> <y0> <x1> <y1> <value>               -- rectangle set
//   SETIF <gridId> <x> <y> <ifValue> <value>                 -- conditional set
//   GET   <gridId> <x> <y> <result>                          -- probe get()  (result from REAL class)
//   EDGE  <gridId> <x> <y> <ifValue> <result0or1>            -- probe edgesTo() (result from REAL class)

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;

public class WoodlandMansionGridParity {
    static final java.io.PrintStream OUT = System.out;

    static Class<?> GRID_CLS;
    static Constructor<?> CTOR;
    static Method M_set3, M_set5, M_get, M_setif, M_edgesTo;

    static final Map<Integer, Object> GRIDS = new HashMap<>();

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Locate WoodlandMansionPieces$SimpleGrid (private static nested class).
        GRID_CLS = null;
        for (Class<?> c : Class.forName(
                "net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces")
                .getDeclaredClasses()) {
            if (c.getSimpleName().equals("SimpleGrid")) { GRID_CLS = c; break; }
        }
        if (GRID_CLS == null) throw new IllegalStateException("SimpleGrid not found");

        CTOR = GRID_CLS.getDeclaredConstructor(int.class, int.class, int.class);
        CTOR.setAccessible(true);
        M_set3 = GRID_CLS.getDeclaredMethod("set", int.class, int.class, int.class);
        M_set3.setAccessible(true);
        M_set5 = GRID_CLS.getDeclaredMethod("set", int.class, int.class, int.class, int.class, int.class);
        M_set5.setAccessible(true);
        M_get = GRID_CLS.getDeclaredMethod("get", int.class, int.class);
        M_get.setAccessible(true);
        M_setif = GRID_CLS.getDeclaredMethod("setif", int.class, int.class, int.class, int.class);
        M_setif.setAccessible(true);
        M_edgesTo = GRID_CLS.getDeclaredMethod("edgesTo", int.class, int.class, int.class);
        M_edgesTo.setAccessible(true);

        // Grid shapes: the real mansion uses 11x11 (valueIfOutside=5); we add edge
        // shapes (1x1, 1xN, Nx1, asymmetric) and varied sentinels (incl. negatives).
        int[][] shapes = {
            {11, 11, 5},   // real WoodlandMansionPieces usage
            {1, 1, 0},
            {1, 1, 5},
            {1, 7, 9},     // single column
            {7, 1, -1},    // single row
            {3, 5, 0},
            {5, 3, 7},
            {8, 8, -7},
            {4, 6, 13},
        };

        int gid = 0;
        for (int[] s : shapes) {
            buildAndExercise(gid++, s[0], s[1], s[2]);
        }

        OUT.flush();
    }

    // Create one grid, run a deterministic op script, probe heavily after each phase.
    static void buildAndExercise(int gid, int w, int h, int outside) throws Exception {
        Object g = CTOR.newInstance(w, h, outside);
        GRIDS.put(gid, g);
        OUT.println("GRID\t" + gid + "\t" + w + "\t" + h + "\t" + outside);

        // Phase 0: probe the fresh (all-zero) grid, including OOB ring.
        probeAll(gid, g);

        // Phase 1: single-cell sets at corners, center, edges, and OOB (dropped).
        int xMax = w - 1, yMax = h - 1;
        int[][] sets = {
            {0, 0, 1}, {xMax, yMax, 2}, {0, yMax, 3}, {xMax, 0, 4},
            {w / 2, h / 2, 8}, {-1, 0, 99}, {0, -1, 99}, {w, 0, 99}, {0, h, 99},
            {-5, -5, 99}, {w + 3, h + 3, 99},
        };
        for (int[] s : sets) {
            M_set3.invoke(g, s[0], s[1], s[2]);
            OUT.println("SET\t" + gid + "\t" + s[0] + "\t" + s[1] + "\t" + s[2]);
        }
        probeAll(gid, g);

        // Phase 2: rectangle sets — forward, reversed (no-op), partial OOB, full grid.
        int[][] rects = {
            {0, 0, xMax, yMax, 6},                 // fill whole grid
            {1, 1, Math.max(1, xMax - 1), Math.max(1, yMax - 1), 11}, // inner block
            {2, 2, 0, 0, 77},                      // reversed range -> no-op
            {-2, -2, 1, 1, 22},                    // partial OOB (clipped by set's guard)
            {xMax - 1, yMax - 1, w + 2, h + 2, 33},// partial OOB high side
            {0, h / 2, xMax, h / 2, 44},           // single horizontal line
        };
        for (int[] r : rects) {
            M_set5.invoke(g, r[0], r[1], r[2], r[3], r[4]);
            OUT.println("RECT\t" + gid + "\t" + r[0] + "\t" + r[1] + "\t" + r[2] + "\t" + r[3] + "\t" + r[4]);
        }
        probeAll(gid, g);

        // Phase 3: setif — conditional writes that hinge on get() (incl. OOB targets).
        int[][] ifs = {
            {0, 0, 6, 100},   // matches current?
            {0, 0, 6, 101},
            {1, 1, 11, 200},
            {w / 2, h / 2, 44, 300},
            {-1, -1, outside, 400}, // OOB get == outside; set() drops the write
            {xMax, yMax, 6, 500},
        };
        for (int[] f : ifs) {
            M_setif.invoke(g, f[0], f[1], f[2], f[3]);
            OUT.println("SETIF\t" + gid + "\t" + f[0] + "\t" + f[1] + "\t" + f[2] + "\t" + f[3]);
        }
        probeAll(gid, g);
    }

    // Emit GET for every cell in the OOB-extended ring [-1..w] x [-1..h], plus
    // EDGE probes for a representative set of `ifValue`s at every such position.
    static void probeAll(int gid, Object g) throws Exception {
        int w = getInt(g, "width");
        int h = getInt(g, "height");
        int outside = getInt(g, "valueIfOutside");
        int[] ifVals = {0, 6, 11, 44, outside};
        for (int y = -1; y <= h; y++) {
            for (int x = -1; x <= w; x++) {
                int got = (Integer) M_get.invoke(g, x, y);
                OUT.println("GET\t" + gid + "\t" + x + "\t" + y + "\t" + got);
                for (int iv : ifVals) {
                    boolean e = (Boolean) M_edgesTo.invoke(g, x, y, iv);
                    OUT.println("EDGE\t" + gid + "\t" + x + "\t" + y + "\t" + iv + "\t" + (e ? 1 : 0));
                }
            }
        }
    }

    static int getInt(Object g, String field) throws Exception {
        java.lang.reflect.Field f = GRID_CLS.getDeclaredField(field);
        f.setAccessible(true);
        return f.getInt(g);
    }
}

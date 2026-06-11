// Ground-truth generator for the PURE room-type "fitter" predicates + their
// claim-marking nested in the REAL decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces
//     -> private interface MonumentRoomFitter
//          boolean fits(RoomDefinition)
//          OceanMonumentPiece create(Direction, RoomDefinition, RandomSource)
//     -> seven implementing private static classes (FitDoubleXY/YZ/Z/X/Y,
//        FitSimpleTop, FitSimpleRoom).
//
// All of `fits` and the claim-marking in `create` are self-contained boolean/flag
// graph logic over RoomDefinition.hasOpening[]/connections[]/claimed: NO world
// writes, NO BlockState, NO registry/datapack, and NO RandomSource (the RandomSource
// create() receives flows ONLY into the constructed piece's bounding box). We
// Bootstrap because OceanMonumentPieces' <clinit> touches Blocks. We drive the REAL
// RoomDefinition + the REAL fitter classes via reflection (all private nested), build
// the SAME deterministic room graph the C++ header builds, then:
//   * exercise fits() for every fitter on every non-special grid room in a battery of
//     SCRIPTED graph states (fully-open, with neighbours claimed, with openings
//     cleared) — hitting both branches of every predicate, the two-hop traps, and the
//     FitSimpleTop all-closed case; and
//   * invoke the REAL create() for representative (fitter,room) pairs on FRESH graphs
//     and snapshot the resulting claimed[] flags (the source-swap quirk included).
//
//   tools/run_groundtruth.ps1 -Tool OceanMonumentRoomFitterParity -Out mcpp/build/ocean_monument_room_fitter.tsv
//
// Line formats (ints decimal; first tab-field is the row tag):
//   FITS   <scenarioId> <fitterIdx> <nodeId> <0|1>          -- fitter.fits(room)
//   FIRST  <scenarioId> <nodeId> <fitterIdx>                -- first matching fitter idx (probe order)
//   CLAIM  <createId> <fitterIdx> <defNodeId> <claimedMask> -- claimed bitset over all nodes after create()
// fitterIdx is the PROBE-ORDER index: 0=DoubleXY 1=DoubleYZ 2=DoubleZ 3=DoubleX
//                                     4=DoubleY  5=SimpleTop 6=Simple.

import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;

public class OceanMonumentRoomFitterParity {
    static final java.io.PrintStream OUT = System.out;

    static Class<?> ROOM_CLS;
    static Constructor<?> ROOM_CTOR;
    static Method M_setConnection;
    static Method M_updateOpenings;
    static Field F_index, F_connections, F_hasOpening, F_claimed, F_isSource;

    // The seven fitter classes, in PROBE ORDER (NOT class declaration order):
    //   XY, YZ, Z, X, Y, SimpleTop, Simple  (OceanMonumentPieces.java:190-196).
    static final String[] FITTER_NAMES = {
        "FitDoubleXYRoom", "FitDoubleYZRoom", "FitDoubleZRoom", "FitDoubleXRoom",
        "FitDoubleYRoom", "FitSimpleTopRoom", "FitSimpleRoom"
    };
    static final Object[] FITTERS = new Object[7];
    static final Method[] M_fits = new Method[7];
    static final Method[] M_create = new Method[7];

    static int getRoomIndex(int x, int y, int z) { return y * 25 + z * 5 + x; }

    static final int GRIDROOM_SOURCE_INDEX        = getRoomIndex(2, 0, 0);
    static final int GRIDROOM_TOP_CONNECT_INDEX   = getRoomIndex(2, 2, 0);
    static final int GRIDROOM_LEFTWING_CONNECT_INDEX  = getRoomIndex(0, 1, 0);
    static final int GRIDROOM_RIGHTWING_CONNECT_INDEX = getRoomIndex(4, 1, 0);

    // Node registry, in creation order, so connections[] serialize as stable node ids
    // and align exactly with the C++ buildDeterministicRoomGraph node numbering.
    static List<Object> NODES;
    static int nodeIdOf(Object o) {
        for (int i = 0; i < NODES.size(); i++) if (NODES.get(i) == o) return i;
        return -1;
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> outer = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces");
        Class<?> fitterIface = null;
        for (Class<?> c : outer.getDeclaredClasses()) {
            String sn = c.getSimpleName();
            if (sn.equals("RoomDefinition")) ROOM_CLS = c;
            if (sn.equals("MonumentRoomFitter")) fitterIface = c;
        }
        if (ROOM_CLS == null) throw new IllegalStateException("RoomDefinition not found");
        if (fitterIface == null) throw new IllegalStateException("MonumentRoomFitter not found");

        ROOM_CTOR = ROOM_CLS.getDeclaredConstructor(int.class);
        ROOM_CTOR.setAccessible(true);
        M_setConnection = ROOM_CLS.getDeclaredMethod("setConnection", Direction.class, ROOM_CLS);
        M_setConnection.setAccessible(true);
        M_updateOpenings = ROOM_CLS.getDeclaredMethod("updateOpenings");
        M_updateOpenings.setAccessible(true);
        F_index       = field("index");
        F_connections = field("connections");
        F_hasOpening  = field("hasOpening");
        F_claimed     = field("claimed");
        F_isSource    = field("isSource");

        // Instantiate the seven fitters via their no-arg ctors and bind fits/create.
        for (int i = 0; i < FITTER_NAMES.length; i++) {
            Class<?> fc = null;
            for (Class<?> c : outer.getDeclaredClasses())
                if (c.getSimpleName().equals(FITTER_NAMES[i])) fc = c;
            if (fc == null) throw new IllegalStateException("fitter not found: " + FITTER_NAMES[i]);
            Constructor<?> ctor = fc.getDeclaredConstructor();
            ctor.setAccessible(true);
            FITTERS[i] = ctor.newInstance();
            M_fits[i] = fc.getDeclaredMethod("fits", ROOM_CLS);
            M_fits[i].setAccessible(true);
            M_create[i] = fc.getDeclaredMethod("create", Direction.class, ROOM_CLS, RandomSource.class);
            M_create[i].setAccessible(true);
        }

        // ── fits() battery over several scripted graph states ──
        // Scenario 0: the pristine deterministic graph (all valid neighbours open,
        // nothing claimed except the three special wing/roof rooms).
        Object[] grid0 = buildGraph();
        emitFits(0, grid0);
        emitFirst(0, grid0);

        // Scenario 1: claim a swath of UP/EAST/NORTH neighbours to drive the
        // `!connections[..].claimed` branches and the two-hop fitters to false.
        Object[] grid1 = buildGraph();
        claimGridLayer(grid1, 1);          // claim the whole middle (y=1) layer
        emitFits(1, grid1);
        emitFirst(1, grid1);

        // Scenario 2: clear EVERY opening on a handful of rooms so FitSimpleTopRoom
        // (all of W/E/N/S/UP closed) starts matching, and DoubleZ's NORTH guard fails.
        Object[] grid2 = buildGraph();
        for (int x = 0; x < 5; x++) {
            int p = getRoomIndex(x, 0, 0);
            boolean[] open = (boolean[]) F_hasOpening.get(grid2[p]);
            for (int i = 0; i < 6; i++) open[i] = false;
        }
        emitFits(2, grid2);
        emitFirst(2, grid2);

        // Scenario 3: claim only the EAST neighbour of a row to flip DoubleX/DoubleXY
        // false while leaving DoubleZ/DoubleY true (mixed predicate outcomes).
        Object[] grid3 = buildGraph();
        for (int x = 0; x < 4; x++) {
            Object def = grid3[getRoomIndex(x, 0, 1)];
            Object[] conns = connArr(def);
            int east = Direction.EAST.get3DDataValue();
            if (conns[east] != null) F_claimed.setBoolean(conns[east], true);
        }
        emitFits(3, grid3);
        emitFirst(3, grid3);

        // Scenario 4: clear ONLY the four horizontal openings (W/E/N/S) on a set of
        // rooms while LEAVING the UP opening intact — this is the state where
        // FitSimpleTopRoom.fits hinges on its UP clause (must be FALSE because UP is
        // still open), and FitDoubleYRoom can still match. Makes the UP-clause of
        // FitSimpleTop the deciding factor.
        Object[] grid4 = buildGraph();
        for (int x = 1; x < 4; x++) {
            Object def = grid4[getRoomIndex(x, 0, 1)];
            boolean[] open = (boolean[]) F_hasOpening.get(def);
            open[Direction.WEST.get3DDataValue()]  = false;
            open[Direction.EAST.get3DDataValue()]  = false;
            open[Direction.NORTH.get3DDataValue()] = false;
            open[Direction.SOUTH.get3DDataValue()] = false;
            // UP intentionally left open.
        }
        emitFits(4, grid4);
        emitFirst(4, grid4);

        // Scenario 5: same horizontals-cleared rooms as scenario 4 but ALSO claim
        // their UP neighbour, so FitDoubleY.fits is FALSE and FitSimpleTop.fits is
        // STILL false (UP opening present) — the room falls through to FitSimpleRoom.
        // This makes FitSimpleTop's UP clause distinguish from scenario 6's all-closed.
        Object[] grid5 = buildGraph();
        for (int x = 1; x < 4; x++) {
            Object def = grid5[getRoomIndex(x, 0, 1)];
            boolean[] open = (boolean[]) F_hasOpening.get(def);
            open[Direction.WEST.get3DDataValue()]  = false;
            open[Direction.EAST.get3DDataValue()]  = false;
            open[Direction.NORTH.get3DDataValue()] = false;
            open[Direction.SOUTH.get3DDataValue()] = false;
            Object[] conns = connArr(def);
            int up = Direction.UP.get3DDataValue();
            if (conns[up] != null) F_claimed.setBoolean(conns[up], true);
        }
        emitFits(5, grid5);
        emitFirst(5, grid5);

        // Scenario 6: ALL six openings cleared on the same rooms (so FitSimpleTop.fits
        // is finally TRUE) — the positive counterpart to scenarios 4/5.
        Object[] grid6 = buildGraph();
        for (int x = 1; x < 4; x++) {
            Object def = grid6[getRoomIndex(x, 0, 1)];
            boolean[] open = (boolean[]) F_hasOpening.get(def);
            for (int i = 0; i < 6; i++) open[i] = false;
        }
        emitFits(6, grid6);
        emitFirst(6, grid6);

        // ── create() claim-effect battery ──
        // For each representative (fitter, room) we build a FRESH graph, invoke the
        // REAL create(), and snapshot the claimed[] flags of all nodes.
        int createId = 0;
        // DoubleXY on a y=0 room that has EAST+UP free and EAST.UP free.
        createId = emitCreate(createId, 0, getRoomIndex(0, 0, 0));
        // DoubleYZ on a y=0 room with NORTH+UP free and NORTH.UP free.
        createId = emitCreate(createId, 1, getRoomIndex(1, 0, 1));
        // DoubleZ "happy path": room with a free NORTH neighbour.
        createId = emitCreate(createId, 2, getRoomIndex(2, 0, 1));
        // DoubleZ "source-swap": claim this room's NORTH neighbour first so create()
        // falls back to the SOUTH neighbour as the source.
        {
            Object[] grid = buildGraph();
            int p = getRoomIndex(2, 0, 1);
            Object def = grid[p];
            Object[] conns = connArr(def);
            int north = Direction.NORTH.get3DDataValue();
            if (conns[north] != null) F_claimed.setBoolean(conns[north], true);
            createId = emitCreateOn(createId, 2, def, grid);
        }
        // DoubleX on a room with a free EAST neighbour.
        createId = emitCreate(createId, 3, getRoomIndex(0, 0, 0));
        // DoubleY on a room with a free UP neighbour.
        createId = emitCreate(createId, 4, getRoomIndex(1, 0, 0));
        // SimpleTop on a fully-closed room.
        {
            Object[] grid = buildGraph();
            int p = getRoomIndex(0, 0, 0);
            boolean[] open = (boolean[]) F_hasOpening.get(grid[p]);
            for (int i = 0; i < 6; i++) open[i] = false;
            createId = emitCreateOn(createId, 5, grid[p], grid);
        }
        // Simple (catch-all) on an arbitrary room.
        createId = emitCreate(createId, 6, getRoomIndex(3, 0, 0));

        OUT.flush();
    }

    // Build the deterministic room graph exactly as MonumentBuilding.generateRoomGraph
    // does up to OceanMonumentPieces.java:310 (before Util.shuffle / the prune loop).
    // Resets NODES so node ids are stable & match the C++ build for each fresh graph.
    static Object[] buildGraph() throws Exception {
        NODES = new ArrayList<>();
        Object[] roomGrid = new Object[75];
        for (int x = 0; x < 5; x++)
            for (int z = 0; z < 4; z++) { int pos = getRoomIndex(x, 0, z); roomGrid[pos] = newRoom(pos); }
        for (int x = 0; x < 5; x++)
            for (int z = 0; z < 4; z++) { int pos = getRoomIndex(x, 1, z); roomGrid[pos] = newRoom(pos); }
        for (int x = 1; x < 4; x++)
            for (int z = 0; z < 2; z++) { int pos = getRoomIndex(x, 2, z); roomGrid[pos] = newRoom(pos); }

        Object sourceRoom = roomGrid[GRIDROOM_SOURCE_INDEX];

        for (int x = 0; x < 5; x++)
            for (int z = 0; z < 5; z++)
                for (int y = 0; y < 3; y++) {
                    int pos = getRoomIndex(x, y, z);
                    if (roomGrid[pos] == null) continue;
                    for (Direction direction : Direction.values()) {
                        int nx = x + direction.getStepX();
                        int ny = y + direction.getStepY();
                        int nz = z + direction.getStepZ();
                        if (nx >= 0 && nx < 5 && nz >= 0 && nz < 5 && ny >= 0 && ny < 3) {
                            int npos = getRoomIndex(nx, ny, nz);
                            if (roomGrid[npos] != null) {
                                if (nz == z) M_setConnection.invoke(roomGrid[pos], direction, roomGrid[npos]);
                                else         M_setConnection.invoke(roomGrid[pos], direction.getOpposite(), roomGrid[npos]);
                            }
                        }
                    }
                }

        Object roofRoom  = newRoom(1003);
        Object leftWing  = newRoom(1001);
        Object rightWing = newRoom(1002);
        M_setConnection.invoke(roomGrid[GRIDROOM_TOP_CONNECT_INDEX], Direction.UP, roofRoom);
        M_setConnection.invoke(roomGrid[GRIDROOM_LEFTWING_CONNECT_INDEX], Direction.SOUTH, leftWing);
        M_setConnection.invoke(roomGrid[GRIDROOM_RIGHTWING_CONNECT_INDEX], Direction.SOUTH, rightWing);
        F_claimed.setBoolean(roofRoom, true);
        F_claimed.setBoolean(leftWing, true);
        F_claimed.setBoolean(rightWing, true);
        F_isSource.setBoolean(sourceRoom, true);

        for (Object def : roomGrid)
            if (def != null) M_updateOpenings.invoke(def);
        M_updateOpenings.invoke(roofRoom);   // leftWing/rightWing intentionally NOT updated
        return roomGrid;
    }

    static Object newRoom(int index) throws Exception {
        Object r = ROOM_CTOR.newInstance(index);
        NODES.add(r);
        return r;
    }

    // Claim every room on a given y-layer (and reflect on connections' claimed flag).
    static void claimGridLayer(Object[] grid, int y) throws Exception {
        for (int x = 0; x < 5; x++)
            for (int z = 0; z < 5; z++) {
                Object r = grid[getRoomIndex(x, y, z)];
                if (r != null) F_claimed.setBoolean(r, true);
            }
    }

    static void emitFits(int scenarioId, Object[] grid) throws Exception {
        for (int id = 0; id < NODES.size(); id++) {
            Object n = NODES.get(id);
            if (F_index.getInt(n) >= 75) continue;          // skip special rooms
            for (int fi = 0; fi < 7; fi++) {
                boolean r = (Boolean) M_fits[fi].invoke(FITTERS[fi], n);
                OUT.println("FITS\t" + scenarioId + "\t" + fi + "\t" + id + "\t" + (r ? 1 : 0));
            }
        }
    }

    static void emitFirst(int scenarioId, Object[] grid) throws Exception {
        for (int id = 0; id < NODES.size(); id++) {
            Object n = NODES.get(id);
            if (F_index.getInt(n) >= 75) continue;
            int first = 6;                                   // FitSimpleRoom is always-true
            for (int fi = 0; fi < 7; fi++) {
                if ((Boolean) M_fits[fi].invoke(FITTERS[fi], n)) { first = fi; break; }
            }
            OUT.println("FIRST\t" + scenarioId + "\t" + id + "\t" + first);
        }
    }

    static int emitCreate(int createId, int fitterIdx, int gridPos) throws Exception {
        Object[] grid = buildGraph();
        return emitCreateOn(createId, fitterIdx, grid[gridPos], grid);
    }

    static int emitCreateOn(int createId, int fitterIdx, Object def, Object[] grid) throws Exception {
        // Invoke the REAL create(). The RandomSource flows only into the piece's box.
        M_create[fitterIdx].invoke(FITTERS[fitterIdx], Direction.NORTH, def, RandomSource.create(1234L));
        // Snapshot claimed[] as a bitset over node ids (LSB = node 0). There are 78
        // nodes (75 grid slots minus the empty cells, plus 3 special) — well under 128,
        // so we emit two longs as a hex-free decimal pair to stay exact.
        long lo = 0L, hi = 0L;
        for (int id = 0; id < NODES.size(); id++) {
            boolean c = F_claimed.getBoolean(NODES.get(id));
            if (c) {
                if (id < 64) lo |= (1L << id);
                else hi |= (1L << (id - 64));
            }
        }
        OUT.println("CLAIM\t" + createId + "\t" + fitterIdx + "\t" + nodeIdOf(def)
            + "\t" + lo + "\t" + hi);
        return createId + 1;
    }

    static Object[] connArr(Object node) throws Exception {
        Object arr = F_connections.get(node);
        int len = Array.getLength(arr);
        Object[] out = new Object[len];
        for (int i = 0; i < len; i++) out[i] = Array.get(arr, i);
        return out;
    }

    static Field field(String name) throws Exception {
        Field f = ROOM_CLS.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }
}

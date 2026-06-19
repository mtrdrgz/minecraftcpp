// Ground-truth generator for the PURE room-connectivity graph nested in the REAL
// decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces
//     -> private static class RoomDefinition
//          RoomDefinition(int)             setConnection(Direction, RoomDefinition)
//          updateOpenings()                findSource(int)
//          isSpecial()                     countOpenings()
//     plus the DETERMINISTIC prefix of MonumentBuilding.generateRoomGraph that
//     wires the 5x3x5 room grid + 3 special wing/roof rooms together.
//
// All of this is self-contained int/boolean graph logic: NO world writes, NO
// BlockState, NO registry/datapack, and (the part exercised here) NO RandomSource.
// We Bootstrap because OceanMonumentPieces' <clinit> touches Blocks. We drive the
// REAL RoomDefinition via reflection (it is a private nested class with private
// fields/methods), replicate the deterministic grid build, then SCRIPT a fixed set
// of open/close operations (no RNG) and emit each graph-helper result.
//
//   tools/run_groundtruth.ps1 -Tool OceanMonumentRoomGraphParity -Out mcpp/build/ocean_monument_room_graph.tsv
//
// Line formats (all ints decimal; the first tab-field is the row tag):
//   INDEX  <nodeId> <index>                       -- RoomDefinition.index
//   SPEC   <nodeId> <0|1>                          -- isSpecial()
//   CONN   <nodeId> <c0> <c1> <c2> <c3> <c4> <c5>  -- connections[] as node ids (-1 null)
//   OPEN0  <nodeId> <h0..h5> <count>               -- hasOpening[]+countOpenings() after build
//   FS0    <nodeId> <0|1>                           -- findSource(scan) from each node after build
//   OPEN1  <nodeId> <h0..h5> <count>               -- after the scripted prune sequence
//   FS1    <nodeId> <0|1>                           -- findSource from each node after prune
//   PRUNE  <step> <nodeId> <f> <accepted>          -- one scripted close attempt result

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import net.minecraft.core.Direction;

public class OceanMonumentRoomGraphParity {
    static final java.io.PrintStream OUT = System.out;

    static Class<?> ROOM_CLS;
    static Constructor<?> ROOM_CTOR;
    static Method M_setConnection;
    static Method M_updateOpenings;
    static Method M_findSource;
    static Method M_isSpecial;
    static Method M_countOpenings;
    static Field F_index, F_connections, F_hasOpening, F_claimed, F_isSource, F_scanIndex;

    // getRoomIndex(x,y,z) == y*25 + z*5 + x.
    static int getRoomIndex(int x, int y, int z) { return y * 25 + z * 5 + x; }

    static final int GRIDROOM_SOURCE_INDEX        = getRoomIndex(2, 0, 0);
    static final int GRIDROOM_TOP_CONNECT_INDEX   = getRoomIndex(2, 2, 0);
    static final int GRIDROOM_LEFTWING_CONNECT_INDEX  = getRoomIndex(0, 1, 0);
    static final int GRIDROOM_RIGHTWING_CONNECT_INDEX = getRoomIndex(4, 1, 0);

    // Node registry: every created RoomDefinition gets a stable nodeId in creation
    // order, so we can serialize connections[] as node ids. roomGrid[] holds the
    // node objects exactly like the real generateRoomGraph.
    static final List<Object> NODES = new ArrayList<>();
    static int nodeIdOf(Object o) {
        for (int i = 0; i < NODES.size(); i++) if (NODES.get(i) == o) return i;
        return -1;
    }
    static Object newRoom(int index) throws Exception {
        Object r = ROOM_CTOR.newInstance(index);
        NODES.add(r);
        return r;
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> outer = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces");
        for (Class<?> c : outer.getDeclaredClasses())
            if (c.getSimpleName().equals("RoomDefinition")) ROOM_CLS = c;
        if (ROOM_CLS == null) throw new IllegalStateException("RoomDefinition not found");

        ROOM_CTOR = ROOM_CLS.getDeclaredConstructor(int.class);
        ROOM_CTOR.setAccessible(true);
        M_setConnection = ROOM_CLS.getDeclaredMethod("setConnection", Direction.class, ROOM_CLS);
        M_setConnection.setAccessible(true);
        M_updateOpenings = ROOM_CLS.getDeclaredMethod("updateOpenings");
        M_updateOpenings.setAccessible(true);
        M_findSource = ROOM_CLS.getDeclaredMethod("findSource", int.class);
        M_findSource.setAccessible(true);
        M_isSpecial = ROOM_CLS.getDeclaredMethod("isSpecial");
        M_isSpecial.setAccessible(true);
        M_countOpenings = ROOM_CLS.getDeclaredMethod("countOpenings");
        M_countOpenings.setAccessible(true);

        F_index       = field("index");
        F_connections = field("connections");
        F_hasOpening  = field("hasOpening");
        F_claimed     = field("claimed");
        F_isSource    = field("isSource");
        F_scanIndex   = field("scanIndex");

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
                                if (nz == z) setConnection(roomGrid[pos], direction, roomGrid[npos]);
                                else         setConnection(roomGrid[pos], direction.getOpposite(), roomGrid[npos]);
                            }
                        }
                    }
                }

        Object roofRoom  = newRoom(1003);
        Object leftWing  = newRoom(1001);
        Object rightWing = newRoom(1002);
        setConnection(roomGrid[GRIDROOM_TOP_CONNECT_INDEX], Direction.UP, roofRoom);
        setConnection(roomGrid[GRIDROOM_LEFTWING_CONNECT_INDEX], Direction.SOUTH, leftWing);
        setConnection(roomGrid[GRIDROOM_RIGHTWING_CONNECT_INDEX], Direction.SOUTH, rightWing);
        F_claimed.setBoolean(roofRoom, true);
        F_claimed.setBoolean(leftWing, true);
        F_claimed.setBoolean(rightWing, true);
        F_isSource.setBoolean(sourceRoom, true);

        // updateOpenings on every grid room (in roomGrid[] order, nulls skipped),
        // then roofRoom — exactly OceanMonumentPieces.java:303-310. leftWing /
        // rightWing intentionally NOT updated.
        for (Object def : roomGrid)
            if (def != null) M_updateOpenings.invoke(def);
        M_updateOpenings.invoke(roofRoom);

        // ── emit static structure (index, isSpecial, connections) ──
        for (int id = 0; id < NODES.size(); id++) {
            Object n = NODES.get(id);
            OUT.println("INDEX\t" + id + "\t" + F_index.getInt(n));
            OUT.println("SPEC\t" + id + "\t" + ((Boolean) M_isSpecial.invoke(n) ? 1 : 0));
            int[] conn = (int[]) connIds(n);
            StringBuilder sb = new StringBuilder("CONN\t").append(id);
            for (int v : conn) sb.append('\t').append(v);
            OUT.println(sb);
        }

        // ── emit openings + findSource after the deterministic build ──
        emitOpenings("OPEN0");
        emitFindSource("FS0", 1);

        // ── SCRIPTED prune sequence (no RNG): try to close a fixed list of
        // (gridIndex, faceDir) openings, replicating the real prune body exactly
        // (close both sides, run findSource on both endpoints with increasing
        // scanIndex, revert if either fails). We choose a representative set that
        // hits accepted AND rejected closes. ──
        int[][] closes = {
            {getRoomIndex(0, 0, 0), 5},  // EAST
            {getRoomIndex(1, 0, 0), 5},  // EAST
            {getRoomIndex(2, 0, 0), 3},  // SOUTH
            {getRoomIndex(0, 0, 0), 3},  // SOUTH
            {getRoomIndex(3, 0, 1), 1},  // UP
            {getRoomIndex(2, 1, 0), 2},  // NORTH
            {getRoomIndex(4, 0, 0), 0},  // DOWN (no connection -> no-op-ish)
            {getRoomIndex(1, 1, 1), 4},  // WEST
            {getRoomIndex(2, 0, 1), 1},  // UP
            {getRoomIndex(0, 1, 0), 3},  // SOUTH (to leftWing connect)
            {getRoomIndex(3, 2, 0), 0},  // DOWN
            {getRoomIndex(1, 0, 2), 5},  // EAST
        };
        int scan = 2;
        int step = 0;
        for (int[] cl : closes) {
            Object def = roomGrid[cl[0]];
            int f = cl[1];
            int accepted = 0;
            boolean[] open = (boolean[]) F_hasOpening.get(def);
            if (open[f]) {
                Object[] conns = connArr(def);
                int of = Direction.from3DDataValue(f).getOpposite().get3DDataValue();
                boolean[] otherOpen = (boolean[]) F_hasOpening.get(conns[f]);
                open[f] = false;
                otherOpen[of] = false;
                boolean a = (Boolean) M_findSource.invoke(def, scan++);
                boolean b = (Boolean) M_findSource.invoke(conns[f], scan++);
                if (a && b) {
                    accepted = 1;
                } else {
                    open[f] = true;
                    otherOpen[of] = true;
                }
            }
            OUT.println("PRUNE\t" + step + "\t" + nodeIdOf(def) + "\t" + f + "\t" + accepted);
            step++;
        }

        emitOpenings("OPEN1");
        emitFindSource("FS1", scan);

        OUT.flush();
    }

    static void emitOpenings(String tag) throws Exception {
        for (int id = 0; id < NODES.size(); id++) {
            Object n = NODES.get(id);
            boolean[] open = (boolean[]) F_hasOpening.get(n);
            StringBuilder sb = new StringBuilder(tag).append('\t').append(id);
            for (boolean b : open) sb.append('\t').append(b ? 1 : 0);
            sb.append('\t').append((Integer) M_countOpenings.invoke(n));
            OUT.println(sb);
        }
    }

    static void emitFindSource(String tag, int scanStart) throws Exception {
        int scan = scanStart;
        for (int id = 0; id < NODES.size(); id++) {
            Object n = NODES.get(id);
            boolean r = (Boolean) M_findSource.invoke(n, scan++);
            OUT.println(tag + "\t" + id + "\t" + (r ? 1 : 0));
        }
    }

    static void setConnection(Object self, Direction dir, Object def) throws Exception {
        M_setConnection.invoke(self, dir, def);
    }

    static Object[] connArr(Object node) throws Exception {
        Object arr = F_connections.get(node);
        int len = java.lang.reflect.Array.getLength(arr);
        Object[] out = new Object[len];
        for (int i = 0; i < len; i++) out[i] = java.lang.reflect.Array.get(arr, i);
        return out;
    }

    static int[] connIds(Object node) throws Exception {
        Object[] arr = connArr(node);
        int[] out = new int[arr.length];
        for (int i = 0; i < arr.length; i++) out[i] = arr[i] == null ? -1 : nodeIdOf(arr[i]);
        return out;
    }

    static Field field(String name) throws Exception {
        Field f = ROOM_CLS.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }
}

// Ground-truth generator for the PURE per-room geometry nested in the REAL
// decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces
//     -> abstract static class OceanMonumentPiece
//          static int  getRoomIndex(int, int, int)
//          static BoundingBox makeBoundingBox(Direction, RoomDefinition,
//                                int roomWidth, int roomHeight, int roomDepth)
//     -> private static class RoomDefinition   (only its int `index` is read)
//
// Both helpers are self-contained integer geometry: NO world writes, NO
// RandomSource, NO registry/datapack. We exercise them via reflection (both
// nested classes / methods are private/protected; the OceanMonumentPieces
// <clinit> touches Blocks.WATER, so we Bootstrap first).
//
//   tools/run_groundtruth.ps1 -Tool OceanMonumentRoomParity -Out mcpp/build/ocean_monument_room.tsv
//
// Line formats (all ints decimal):
//   IDX  <roomX> <roomY> <roomZ> <index>
//        -- getRoomIndex(roomX,roomY,roomZ) == index (from the REAL class)
//   BOX  <dirOrd> <index> <roomW> <roomH> <roomD> <minX> <minY> <minZ> <maxX> <maxY> <maxZ>
//        -- makeBoundingBox(Direction.from3DData?, RoomDefinition(index), w,h,d)
//           result fields (from the REAL class). dirOrd is Direction enum ordinal.

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

import net.minecraft.core.Direction;
import net.minecraft.world.level.levelgen.structure.BoundingBox;

public class OceanMonumentRoomParity {
    static final java.io.PrintStream OUT = System.out;

    static Class<?> PIECE_CLS;   // OceanMonumentPieces$OceanMonumentPiece
    static Class<?> ROOM_CLS;    // OceanMonumentPieces$RoomDefinition
    static Method M_getRoomIndex;
    static Method M_makeBox;
    static Constructor<?> ROOM_CTOR;
    static Field BB_minX, BB_minY, BB_minZ, BB_maxX, BB_maxY, BB_maxZ;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> outer = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces");
        for (Class<?> c : outer.getDeclaredClasses()) {
            String n = c.getSimpleName();
            if (n.equals("OceanMonumentPiece")) PIECE_CLS = c;
            else if (n.equals("RoomDefinition")) ROOM_CLS = c;
        }
        if (PIECE_CLS == null) throw new IllegalStateException("OceanMonumentPiece not found");
        if (ROOM_CLS == null) throw new IllegalStateException("RoomDefinition not found");

        M_getRoomIndex = PIECE_CLS.getDeclaredMethod("getRoomIndex", int.class, int.class, int.class);
        M_getRoomIndex.setAccessible(true);
        M_makeBox = PIECE_CLS.getDeclaredMethod(
            "makeBoundingBox", Direction.class, ROOM_CLS, int.class, int.class, int.class);
        M_makeBox.setAccessible(true);
        ROOM_CTOR = ROOM_CLS.getDeclaredConstructor(int.class);
        ROOM_CTOR.setAccessible(true);

        BB_minX = field(BoundingBox.class, "minX");
        BB_minY = field(BoundingBox.class, "minY");
        BB_minZ = field(BoundingBox.class, "minZ");
        BB_maxX = field(BoundingBox.class, "maxX");
        BB_maxY = field(BoundingBox.class, "maxY");
        BB_maxZ = field(BoundingBox.class, "maxZ");

        // ── getRoomIndex over the full valid grid plus a strided edge ring ──
        // The real grid is 5 x 3 x 5 (roomX 0..4, roomY 0..2, roomZ 0..4); cover
        // it exhaustively, then add a few out-of-range coords (the helper is pure
        // arithmetic and is, in practice, only fed in-range, but we probe wider).
        for (int ry = 0; ry < 3; ry++)
            for (int rz = 0; rz < 5; rz++)
                for (int rx = 0; rx < 5; rx++)
                    emitIdx(rx, ry, rz);
        int[] extra = {-1, 5, 7};
        for (int v : extra) {
            emitIdx(v, 0, 0);
            emitIdx(0, v, 0);
            emitIdx(0, 0, v);
            emitIdx(v, v, v);
        }

        // ── makeBoundingBox over every orientation, every real grid index, and
        // a representative set of room footprints. Indices: all 75 grid slots
        // (0..74) so the % / / decomposition is fully exercised, plus the three
        // special wing indices (1001/1002/1003 == LEFT/RIGHT/PENTHOUSE) whose
        // decomposition lands out-of-grid but is still computed by the real code.
        Direction[] dirs = Direction.values(); // DOWN,UP,NORTH,SOUTH,WEST,EAST
        int[] specials = {1001, 1002, 1003};
        // Footprints actually used by monument rooms span 1..2 in each grid axis;
        // include 1..3 to stress the width/depth swap and the move offsets.
        int[][] foot = {
            {1, 1, 1}, {2, 1, 1}, {1, 1, 2}, {1, 2, 1},
            {2, 1, 2}, {2, 2, 2}, {3, 1, 1}, {1, 1, 3}, {2, 3, 1},
        };

        for (Direction d : dirs) {
            for (int idx = 0; idx < 75; idx++)
                for (int[] f : foot)
                    emitBox(d, idx, f[0], f[1], f[2]);
            for (int idx : specials)
                for (int[] f : foot)
                    emitBox(d, idx, f[0], f[1], f[2]);
        }

        OUT.flush();
    }

    static void emitIdx(int rx, int ry, int rz) throws Exception {
        int v = (Integer) M_getRoomIndex.invoke(null, rx, ry, rz);
        OUT.println("IDX\t" + rx + "\t" + ry + "\t" + rz + "\t" + v);
    }

    static void emitBox(Direction d, int index, int w, int h, int dp) throws Exception {
        Object room = ROOM_CTOR.newInstance(index);
        Object box = M_makeBox.invoke(null, d, room, w, h, dp);
        OUT.println("BOX\t" + d.ordinal() + "\t" + index + "\t" + w + "\t" + h + "\t" + dp
            + "\t" + BB_minX.getInt(box) + "\t" + BB_minY.getInt(box) + "\t" + BB_minZ.getInt(box)
            + "\t" + BB_maxX.getInt(box) + "\t" + BB_maxY.getInt(box) + "\t" + BB_maxZ.getInt(box));
    }

    static Field field(Class<?> cls, String name) throws Exception {
        Field f = cls.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }
}

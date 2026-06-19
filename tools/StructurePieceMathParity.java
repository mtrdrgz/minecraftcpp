// Ground-truth generator for the PURE orientation/offset math in the REAL
// decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.StructurePiece
//
// Covered (all pure integer geometry; no world writes, no RandomSource):
//   StructurePiece.makeBoundingBox(x,y,z,Direction,w,h,d)   [protected static]
//   StructurePiece.setOrientation(Direction)  -> getMirror()/getRotation()
//   StructurePiece.getWorldX(x,z) / getWorldY(y) / getWorldZ(x,z)  [protected]
//   StructurePiece.getLocatorPosition()  (== BoundingBox.getCenter())
//   StructurePiece.isCloseToChunk(ChunkPos, distance)
//
// StructurePiece is abstract and its <clinit> builds a Block ImmutableSet, so we
// Bootstrap and define a trivial concrete subclass. The protected members are
// reached via reflection (setAccessible).
//
//   tools/run_groundtruth.ps1 -Tool StructurePieceMathParity -Out mcpp/build/structure_piece_math.tsv
//
// Enum ordinals exchanged as ints (match the C++ enums):
//   Mirror:   NONE=0, LEFT_RIGHT=1, FRONT_BACK=2
//   Rotation: NONE=0, CLOCKWISE_90=1, CLOCKWISE_180=2, COUNTERCLOCKWISE_90=3
//   Direction:DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5
//
// Output columns are decimal ints throughout (this math is pure integer).

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;

public class StructurePieceMathParity {
    static final java.io.PrintStream OUT = System.out;

    static final Direction[] DIRS = {
        Direction.DOWN, Direction.UP, Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST
    };

    // Minimal concrete StructurePiece — only construction + the inherited pure
    // geometry is exercised; postProcess/addAdditionalSaveData are never called.
    static final class TestPiece extends StructurePiece {
        TestPiece(StructurePieceType type, BoundingBox bb) { super(type, 0, bb); }
        @Override protected void addAdditionalSaveData(StructurePieceSerializationContext c, CompoundTag t) {}
        @Override public void postProcess(net.minecraft.world.level.WorldGenLevel l,
            net.minecraft.world.level.StructureManager s, ChunkGenerator g,
            net.minecraft.util.RandomSource r, BoundingBox cb, ChunkPos cp, BlockPos rp) {}
    }

    static Method M_makeBB, M_getWorldX, M_getWorldY, M_getWorldZ;
    static StructurePieceType ANY_TYPE;

    static TestPiece piece(BoundingBox bb) { return new TestPiece(ANY_TYPE, bb); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Some valid StructurePieceType for the ctor (value is irrelevant to the math).
        ANY_TYPE = StructurePieceType.MINE_SHAFT_ROOM;

        M_makeBB = StructurePiece.class.getDeclaredMethod(
            "makeBoundingBox", int.class, int.class, int.class, Direction.class, int.class, int.class, int.class);
        M_makeBB.setAccessible(true);
        M_getWorldX = StructurePiece.class.getDeclaredMethod("getWorldX", int.class, int.class);
        M_getWorldX.setAccessible(true);
        M_getWorldY = StructurePiece.class.getDeclaredMethod("getWorldY", int.class);
        M_getWorldY.setAccessible(true);
        M_getWorldZ = StructurePiece.class.getDeclaredMethod("getWorldZ", int.class, int.class);
        M_getWorldZ.setAccessible(true);

        int[] coords  = { Integer.MIN_VALUE, -2147483647, -1000, -16, -3, -1, 0, 1, 7, 16, 1000, 2147483646, Integer.MAX_VALUE };
        int[] dims    = { 1, 2, 3, 5, 7, 16, 32 };
        int[] origins = { -2048, -300, -33, -16, -7, -1, 0, 1, 7, 16, 300, 2048 };

        // ── makeBoundingBox: axis branch + int truncation/overflow ───────────
        for (int x : origins)
            for (int y : new int[]{ -64, 0, 63, 128 })
                for (int z : origins)
                    for (Direction d : DIRS)
                        for (int w : dims)
                            for (int h : dims)
                                for (int dp : dims) {
                                    BoundingBox bb = (BoundingBox) M_makeBB.invoke(null, x, y, z, d, w, h, dp);
                                    OUT.println("MAKEBB\t" + x + "\t" + y + "\t" + z + "\t" + d.ordinal()
                                        + "\t" + w + "\t" + h + "\t" + dp + "\t"
                                        + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ() + "\t"
                                        + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ());
                                }

        // ── setOrientation -> (mirror, rotation) for the 6 facings + null ────
        OUT.println("SETORIENT\t-1\t" + ordOfMirror(orientedMirror(null)) + "\t" + ordOfRotation(orientedRotation(null)));
        for (Direction d : DIRS) {
            TestPiece p = piece(new BoundingBox(0, 0, 0, 1, 1, 1));
            p.setOrientation(d);
            OUT.println("SETORIENT\t" + d.ordinal() + "\t" + p.getMirror().ordinal() + "\t" + p.getRotation().ordinal());
        }

        // ── getWorldX / getWorldY / getWorldZ across orientations & boxes ────
        // BoundingBoxes deliberately span negatives, zero, and large magnitudes.
        BoundingBox[] boxes = {
            new BoundingBox(0, 0, 0, 15, 15, 15),
            new BoundingBox(-7, -3, -11, 5, 9, 20),
            new BoundingBox(-100, 60, -200, -50, 120, -120),
            new BoundingBox(1000000, 0, 1000000, 1000050, 30, 1000050),
            new BoundingBox(-2147483640, -64, -2147483640, -2147483600, 320, -2147483600),
        };
        int[] locals = { -50, -16, -7, -1, 0, 1, 7, 16, 50, 1000 };

        // null-orientation passthrough (returns inputs verbatim).
        for (int x : locals)
            for (int z : locals) {
                TestPiece p = piece(boxes[0]);
                p.setOrientation(null);
                int wx = (Integer) M_getWorldX.invoke(p, x, z);
                int wy = (Integer) M_getWorldY.invoke(p, x);
                int wz = (Integer) M_getWorldZ.invoke(p, x, z);
                OUT.println("WORLD\t-1\t0\t" + x + "\t" + z + "\t" + wx + "\t" + wy + "\t" + wz);
            }

        for (int bi = 0; bi < boxes.length; bi++) {
            for (Direction d : DIRS) {
                for (int x : locals)
                    for (int z : locals) {
                        TestPiece p = piece(boxes[bi]);
                        p.setOrientation(d);
                        int wx = (Integer) M_getWorldX.invoke(p, x, z);
                        int wy = (Integer) M_getWorldY.invoke(p, x);
                        int wz = (Integer) M_getWorldZ.invoke(p, x, z);
                        OUT.println("WORLD\t" + d.ordinal() + "\t" + bi + "\t" + x + "\t" + z
                            + "\t" + wx + "\t" + wy + "\t" + wz);
                    }
            }
        }

        // ── getLocatorPosition == BoundingBox.getCenter() : (max-min+1)/2 ────
        for (BoundingBox bb : boxes) {
            TestPiece p = piece(bb);
            BlockPos c = p.getLocatorPosition();
            OUT.println("LOCATOR\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ()
                + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ()
                + "\t" + c.getX() + "\t" + c.getY() + "\t" + c.getZ());
        }
        // Extra getCenter int-division edge cases (even/odd spans, negatives).
        int[][] spans = {
            {0,0,0, 0,0,0}, {0,0,0, 1,1,1}, {0,0,0, 2,2,2}, {0,0,0, 3,3,3},
            {-5,-5,-5, -5,-5,-5}, {-5,-5,-5, -4,-4,-4}, {-5,-5,-5, -3,-3,-3},
            {-1,-1,-1, 0,0,0}, {-2,-2,-2, 1,1,1}, {-3,-7,-11, 4,8,12},
            {7,7,7, 22,22,22}, {-10,-10,-10, 9,9,9},
        };
        for (int[] s : spans) {
            BlockPos c = piece(new BoundingBox(s[0],s[1],s[2],s[3],s[4],s[5])).getLocatorPosition();
            OUT.println("LOCATOR\t" + s[0] + "\t" + s[1] + "\t" + s[2] + "\t" + s[3] + "\t" + s[4] + "\t" + s[5]
                + "\t" + c.getX() + "\t" + c.getY() + "\t" + c.getZ());
        }

        // ── isCloseToChunk(ChunkPos, distance) : << 4 + intersects ──────────
        int[] chunks = { -100, -16, -4, -1, 0, 1, 4, 16, 100, 134217727 };
        int[] dists  = { 0, 1, 4, 8, 16, 64 };
        BoundingBox[] cbBoxes = {
            new BoundingBox(0, 0, 0, 15, 15, 15),
            new BoundingBox(-8, 0, -8, 8, 32, 8),
            new BoundingBox(48, 0, 48, 80, 16, 80),
            new BoundingBox(-2000, 0, -2000, -1900, 16, -1900),
            new BoundingBox(16, 0, 16, 31, 64, 31),
        };
        for (BoundingBox bb : cbBoxes)
            for (int cx : chunks)
                for (int cz : chunks)
                    for (int dist : dists) {
                        TestPiece p = piece(bb);
                        boolean close = p.isCloseToChunk(new ChunkPos(cx, cz), dist);
                        OUT.println("CLOSE\t" + bb.minX() + "\t" + bb.minZ() + "\t" + bb.maxX() + "\t" + bb.maxZ()
                            + "\t" + cx + "\t" + cz + "\t" + dist + "\t" + (close ? 1 : 0));
                    }

        OUT.flush();
    }

    // Helpers to read mirror/rotation of a piece built with null orientation.
    static net.minecraft.world.level.block.Mirror orientedMirror(Direction d) {
        TestPiece p = piece(new BoundingBox(0, 0, 0, 1, 1, 1));
        p.setOrientation(d);
        return p.getMirror();
    }
    static net.minecraft.world.level.block.Rotation orientedRotation(Direction d) {
        TestPiece p = piece(new BoundingBox(0, 0, 0, 1, 1, 1));
        p.setOrientation(d);
        return p.getRotation();
    }
    static int ordOfMirror(net.minecraft.world.level.block.Mirror m) { return m.ordinal(); }
    static int ordOfRotation(net.minecraft.world.level.block.Rotation r) { return r.ordinal(); }
}

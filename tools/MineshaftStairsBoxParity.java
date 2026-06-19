// Ground-truth generator for the PURE static bounding-box probe in the REAL
// decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.MineshaftPieces
//     .MineShaftStairs.findStairs(StructurePieceAccessor, RandomSource,
//                                 int footX, int footY, int footZ, Direction)
//
// findStairs is PURE control flow over integer geometry: it NEVER reads the
// RandomSource, performs NO world writes and NO registry/datapack access, and its
// only contact with mutable state is a single accessor.findCollisionPiece(box)
// lookup whose RESULT IT USES ONLY FOR NULLNESS (`!= null ? null : box`). We drive
// the REAL method directly (MineShaftStairs + findStairs are public) through an
// inline StructurePieceAccessor that returns a REAL MineShaftStairs (or null) -- so
// the ground truth is the genuine vanilla algorithm, and the only modelled input
// is the boolean "is a piece already there".
//
//   tools/run_groundtruth.ps1 -Tool MineshaftStairsBoxParity -Out mcpp/build/mineshaft_stairs_box.tsv
//
// Line format (all decimal ints; a box is six ints minX..maxZ):
//   FSB <collisionPresent> <footX footY footZ dirOrd> | <hasResult> <r6>
//     collisionPresent 1 -> accessor returns a non-null piece; 0 -> returns null.
//     hasResult        1 -> the next six ints are the returned box; 0 -> findStairs
//                      returned null (the six result ints are 0 0 0 0 0 0).

import java.lang.reflect.Field;

import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.StructurePieceAccessor;
import net.minecraft.world.level.levelgen.structure.structures.MineshaftPieces;
import net.minecraft.world.level.levelgen.structure.structures.MineshaftStructure;

public class MineshaftStairsBoxParity {
    static final java.io.PrintStream OUT = System.out;
    static Field F_minX, F_minY, F_minZ, F_maxX, F_maxY, F_maxZ;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        F_minX = field("minX"); F_minY = field("minY"); F_minZ = field("minZ");
        F_maxX = field("maxX"); F_maxY = field("maxY"); F_maxZ = field("maxZ");

        // A throwaway random -- findStairs never reads it, but the signature wants
        // one. Seed is irrelevant precisely because the result is independent of it.
        RandomSource random = RandomSource.create(0L);

        // The four horizontals findStairs is ever called with (Direction.NORTH is
        // the `default ->` branch of the switch). The verticals never occur in
        // generation; we still exercise NORTH (= default) thoroughly.
        Direction[] dirs = { Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST };

        // Foot positions: origin, off-origin, negatives, chunk-ish + large offsets
        // (large ones exercise the wrapping move()).
        int[][] feet = {
            {0, 0, 0}, {1, 2, 3}, {-1, -2, -3}, {16, 64, 16}, {-16, 64, -16},
            {7, 10, -7}, {100, 0, -100}, {-5, 5, 5}, {3, 3, 3}, {-32, 70, 48},
            {2000000000, 50, 2000000000}, {-2000000000, 12, -2000000000},
        };

        // For each (dir, foot): emit BOTH the no-collision (returns box) and the
        // collision-present (returns null) cases -- the two branches of the ternary.
        for (Direction dir : dirs) {
            for (int[] f : feet) {
                emit(false, f[0], f[1], f[2], dir, random); // accessor -> null  => box
                emit(true,  f[0], f[1], f[2], dir, random); // accessor -> piece => null
            }
        }

        OUT.flush();
    }

    // Drive the REAL findStairs with an accessor returning a real MineShaftStairs
    // when collisionPresent, else null. The collision piece's own box is arbitrary
    // because findStairs only checks the piece for nullness.
    static void emit(boolean collisionPresent, int footX, int footY, int footZ,
                     Direction dir, RandomSource random) throws Exception {
        final StructurePiece collisionPiece = collisionPresent
            ? new MineshaftPieces.MineShaftStairs(
                  0, new BoundingBox(0, 0, 0, 1, 1, 1), Direction.NORTH, MineshaftStructure.Type.NORMAL)
            : null;

        StructurePieceAccessor accessor = new StructurePieceAccessor() {
            @Override public void addPiece(StructurePiece piece) { /* unused */ }
            @Override public StructurePiece findCollisionPiece(BoundingBox box) { return collisionPiece; }
        };

        BoundingBox result = MineshaftPieces.MineShaftStairs.findStairs(
            accessor, random, footX, footY, footZ, dir);

        StringBuilder sb = new StringBuilder("FSB\t");
        sb.append(collisionPresent ? "1" : "0").append('\t')
          .append(footX).append('\t').append(footY).append('\t').append(footZ).append('\t')
          .append(dir.ordinal()).append('\t');
        if (result == null) {
            sb.append("0\t0\t0\t0\t0\t0\t0");
        } else {
            sb.append("1\t").append(dump(result));
        }
        OUT.println(sb.toString());
    }

    static String dump(BoundingBox b) throws Exception {
        return F_minX.getInt(b) + "\t" + F_minY.getInt(b) + "\t" + F_minZ.getInt(b) + "\t"
             + F_maxX.getInt(b) + "\t" + F_maxY.getInt(b) + "\t" + F_maxZ.getInt(b);
    }

    static Field field(String name) throws Exception {
        Field f = BoundingBox.class.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }
}

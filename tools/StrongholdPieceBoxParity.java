// Ground-truth generator for the PURE static bounding-box probe in the REAL
// decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces
//     .FillerCorridor.findPieceBox(StructurePieceAccessor, RandomSource,
//                                  int footX, int footY, int footZ, Direction)
//
// findPieceBox is PURE control flow over integer geometry: it NEVER reads the
// RandomSource, performs NO world writes and NO registry/datapack access, and its
// only contact with mutable state is a single accessor.findCollisionPiece(box)
// lookup whose returned piece's (immutable) BoundingBox it then reads. We drive
// the REAL method directly (FillerCorridor + findPieceBox are public) through an
// inline StructurePieceAccessor that returns a REAL FillerCorridor carrying a
// chosen collision box (or null) -- so the ground truth is the genuine vanilla
// algorithm, and the only modelled input is the collision piece's box.
//
//   tools/run_groundtruth.ps1 -Tool StrongholdPieceBoxParity -Out mcpp/build/stronghold_piece_box.tsv
//
// Line format (all decimal ints; a box is six ints minX..maxZ):
//   FPB <hasCollision> <c6-or-six-zeros> <footX footY footZ dirOrd> | <hasResult> <r6>
//     hasCollision 1 -> the next six ints are the collision piece's box;
//                  0 -> the six ints are 0 0 0 0 0 0 (ignored; accessor returns null).
//     hasResult    1 -> the next six ints are the returned box; 0 -> findPieceBox
//                  returned null (the six result ints are 0 0 0 0 0 0).

import java.lang.reflect.Field;

import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.StructurePieceAccessor;
import net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces;

public class StrongholdPieceBoxParity {
    static final java.io.PrintStream OUT = System.out;
    static Field F_minX, F_minY, F_minZ, F_maxX, F_maxY, F_maxZ;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        F_minX = field("minX"); F_minY = field("minY"); F_minZ = field("minZ");
        F_maxX = field("maxX"); F_maxY = field("maxY"); F_maxZ = field("maxZ");

        // A throwaway random — findPieceBox never reads it, but the signature wants
        // one. Seed is irrelevant precisely because the result is independent of it.
        RandomSource random = RandomSource.create(0L);

        // The four horizontals findPieceBox is ever called with (Direction.NORTH is
        // the `default ->` branch of orientBox too).
        Direction[] dirs = { Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST };

        // Foot positions: origin, off-origin, negatives, chunk-ish offsets.
        int[][] feet = {
            {0, 0, 0}, {1, 2, 3}, {-1, -2, -3}, {16, 64, 16}, {-16, 64, -16},
            {7, 10, -7}, {100, 0, -100}, {-5, 5, 5}, {3, 3, 3}, {-32, 70, 48},
        };

        // ── Case A: NO collision piece (accessor returns null) -> always null. ──
        for (Direction dir : dirs)
            for (int[] f : feet)
                emit(null, f[0], f[1], f[2], dir, random);

        // ── Case B: a collision piece is present. We sweep its box so that the
        // minY-gate fires and misses, and so the descending depth loop returns at
        // depth=2, depth=1, and falls through to null. The depth-4 probe box for a
        // given (foot,dir) is orientBox(foot,-1,-1,0,5,5,4,dir); we synthesise the
        // collision box relative to THAT so we hit every branch deterministically.
        for (Direction dir : dirs) {
            for (int[] f : feet) {
                BoundingBox probe4 = BoundingBox.orientBox(f[0], f[1], f[2], -1, -1, 0, 5, 5, 4, dir);
                int p4MinX = F_minX.getInt(probe4), p4MinY = F_minY.getInt(probe4), p4MinZ = F_minZ.getInt(probe4);
                int p4MaxX = F_maxX.getInt(probe4), p4MaxZ = F_maxZ.getInt(probe4);

                // The depth-1, -2, -3 boxes (to position the collision box exactly on
                // each loop boundary).
                BoundingBox d1 = BoundingBox.orientBox(f[0], f[1], f[2], -1, -1, 0, 5, 5, 1, dir);
                BoundingBox d2 = BoundingBox.orientBox(f[0], f[1], f[2], -1, -1, 0, 5, 5, 2, dir);
                BoundingBox d3 = BoundingBox.orientBox(f[0], f[1], f[2], -1, -1, 0, 5, 5, 3, dir);

                // (B1) collision box == the full depth-4 probe box, same minY:
                //   intersects every candidate -> loop falls through -> null.
                emit(box(probe4), f[0], f[1], f[2], dir, random);

                // (B2) collision box minY DIFFERS from probe.minY -> gate fails -> null.
                emit(new BoundingBox(p4MinX, p4MinY + 1, p4MinZ, p4MaxX, F_maxY.getInt(probe4) + 1, p4MaxZ),
                     f[0], f[1], f[2], dir, random);

                // (B3) collision box == the depth-3 box (same minY): the depth=2
                //   candidate no longer intersects only if the box is short enough.
                //   Using the depth-2 box as collision: depth=2 candidate intersects
                //   (equal box), depth=1 candidate intersects -> falls through -> null.
                emit(box(d2), f[0], f[1], f[2], dir, random);

                // (B4) collision box == the depth-1 box (same minY): depth=2 candidate
                //   may not intersect the (shorter) depth-1 collision box -> returns
                //   orientBox(...,depth+1=3). Exercises the early-return + the +1.
                emit(box(d1), f[0], f[1], f[2], dir, random);

                // (B5) a far-away collision box that shares minY but never intersects
                //   any candidate -> depth=2 candidate misses immediately -> returns
                //   orientBox(...,3).
                emit(new BoundingBox(p4MinX + 1000, p4MinY, p4MinZ + 1000,
                                     p4MaxX + 1000, F_maxY.getInt(probe4), p4MaxZ + 1000),
                     f[0], f[1], f[2], dir, random);

                // (B6) collision box == depth-3 box -> depth=2 candidate intersects,
                //   depth=1 candidate intersects -> null (already null path, but with
                //   a distinct geometry from B1).
                emit(box(d3), f[0], f[1], f[2], dir, random);
            }
        }

        OUT.flush();
    }

    // Drive the REAL findPieceBox with an accessor returning a real FillerCorridor
    // carrying `collisionBox`, or null when collisionBox == null.
    static void emit(BoundingBox collisionBox, int footX, int footY, int footZ,
                     Direction dir, RandomSource random) throws Exception {
        final StructurePiece collisionPiece =
            collisionBox == null ? null : new StrongholdPieces.FillerCorridor(0, collisionBox, Direction.NORTH);

        StructurePieceAccessor accessor = new StructurePieceAccessor() {
            @Override public void addPiece(StructurePiece piece) { /* unused */ }
            @Override public StructurePiece findCollisionPiece(BoundingBox box) { return collisionPiece; }
        };

        BoundingBox result = StrongholdPieces.FillerCorridor.findPieceBox(
            accessor, random, footX, footY, footZ, dir);

        StringBuilder sb = new StringBuilder("FPB\t");
        if (collisionBox == null) {
            sb.append("0\t0\t0\t0\t0\t0\t0");
        } else {
            sb.append("1\t").append(dumpInputBox(collisionPiece.getBoundingBox()));
        }
        sb.append('\t').append(footX).append('\t').append(footY).append('\t').append(footZ)
          .append('\t').append(dir.ordinal()).append('\t');
        if (result == null) {
            sb.append("0\t0\t0\t0\t0\t0\t0");
        } else {
            sb.append("1\t").append(dump(result));
        }
        OUT.println(sb.toString());
    }

    static BoundingBox box(BoundingBox b) throws Exception {
        // Copy via the field values so we hand findPieceBox a fresh, equivalent box.
        return new BoundingBox(F_minX.getInt(b), F_minY.getInt(b), F_minZ.getInt(b),
                               F_maxX.getInt(b), F_maxY.getInt(b), F_maxZ.getInt(b));
    }

    static String dumpInputBox(BoundingBox b) throws Exception { return dump(b); }

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

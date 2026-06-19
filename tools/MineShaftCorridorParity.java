// Ground-truth generator for the RNG-driven corridor-sizing geometry, driving
// the REAL decompiled 26.1.2 class:
//
//   net.minecraft.world.level.levelgen.structure.structures
//       .MineshaftPieces.MineShaftCorridor.findCorridorSize(
//           StructurePieceAccessor, RandomSource, int footX, int footY, int footZ,
//           Direction)                                  [MineshaftPieces.java:144-168]
//   net.minecraft.util.RandomSource (LegacyRandomSource via create(seed))
//
// findCorridorSize draws exactly one RandomSource.nextInt(3) -> corridorLength in
// [2,4], builds a candidate BoundingBox per direction (blockLength=corridorLength*5,
// spanning blockLength-1 along the axis and 2 across), moves it to the foot, and
// returns the first box whose findCollisionPiece is null. We pass a
// StructurePieceAccessor whose findCollisionPiece always returns null (a fresh
// region), so the FIRST candidate is returned for the initial corridorLength.
//
// The C++ MineShaftCorridorParity.h replays the same seeded RandomSource (with a
// "never collides" predicate) and recomputes; the test compares each row
// bit-for-bit (all ints, decimal).
//
//   tools/run_groundtruth.ps1 -Tool MineShaftCorridorParity -Out mcpp/build/mine_shaft_corridor.tsv
//
// MineShaftCorridor.findCorridorSize is public static, so it is invoked directly
// (no reflection needed); the public BoundingBox getters read back the result.
//
// Direction ordinals (net.minecraft.core.Direction.ordinal()):
//   DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5.
//
// TSV rows (leading TAG, all integers decimal):
//   CORR  <seed> <footX> <footY> <footZ> <dirOrd> <found> <minX> <minY> <minZ> <maxX> <maxY> <maxZ>
// where <found> is 1 if findCorridorSize returned a box (always the case for the
// no-collision accessor), 0 if it returned null (then the six box ints are 0).
//
// O is captured at class load so any bootstrap chatter on stdout stays out of the
// TSV.

import net.minecraft.core.Direction;
import net.minecraft.server.Bootstrap;
import net.minecraft.SharedConstants;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.StructurePieceAccessor;
import net.minecraft.world.level.levelgen.structure.structures.MineshaftPieces;

public class MineShaftCorridorParity {
    static final java.io.PrintStream O = System.out;

    // A StructurePieceAccessor that never reports a collision (fresh region):
    // findCollisionPiece(box) == null, so findCorridorSize returns its first
    // candidate. addPiece is unused by findCorridorSize.
    static final StructurePieceAccessor NO_COLLISION = new StructurePieceAccessor() {
        @Override
        public void addPiece(StructurePiece piece) {
        }

        @Override
        public StructurePiece findCollisionPiece(BoundingBox box) {
            return null;
        }
    };

    static void emit(long seed, int footX, int footY, int footZ, Direction dir, BoundingBox bb) {
        if (bb == null) {
            O.println("CORR\t" + seed + "\t" + footX + "\t" + footY + "\t" + footZ
                + "\t" + dir.ordinal()
                + "\t0\t0\t0\t0\t0\t0\t0");
        } else {
            O.println("CORR\t" + seed + "\t" + footX + "\t" + footY + "\t" + footZ
                + "\t" + dir.ordinal()
                + "\t1\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ()
                + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ());
        }
    }

    public static void main(String[] args) {
        // The BoundingBox ctor can normalize an inverted box (at int-wrap edges of
        // footX + blockLength - 1) and log via log4j to the stdout FD, which would
        // pollute the TSV; silence the root logger before any logging occurs. (The
        // swap that normalizes is performed inside the certified BoundingBox.)
        org.apache.logging.log4j.core.config.Configurator.setRootLevel(
            org.apache.logging.log4j.Level.OFF);

        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Finite, well-spread inputs. Seeds chosen so nextInt(3) lands on all three
        // corridorLength values (2,3,4). Foot coords span negative, zero, large
        // positive, and values near the int-wrap edge of footX + (blockLength - 1)
        // (blockLength is at most 4*5 = 20, so blockLength - 1 <= 19).
        long[] seeds = {
            0L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L, 9L, 42L, 123L, 1000L, 999999L,
            -1L, -42L, 0x5DEECE66DL, 1234567890123L, 0xCAFEBABEL, 8675309L
        };
        int[] coords = {
            0, 16, -16, 64, -64, 256, -256, 1024, -1024,
            Integer.MAX_VALUE - 19, Integer.MIN_VALUE, Integer.MAX_VALUE - 1
        };
        // Orientations that occur for a corridor are the four horizontals; we also
        // include UP/DOWN to certify they take the `default` (NORTH-like) arm.
        Direction[] dirs = {
            Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST,
            Direction.DOWN, Direction.UP
        };

        for (long seed : seeds) {
            for (int footX : coords) {
                for (int footZ : coords) {
                    int footY = 50;
                    for (Direction dir : dirs) {
                        RandomSource r = RandomSource.create(seed);
                        BoundingBox bb = MineshaftPieces.MineShaftCorridor.findCorridorSize(
                            NO_COLLISION, r, footX, footY, footZ, dir);
                        emit(seed, footX, footY, footZ, dir, bb);
                    }
                }
            }
        }
    }
}

// Ground-truth generator for the RNG-driven ScatteredFeaturePiece constructor
// geometry, driving the REAL decompiled 26.1.2 classes:
//
//   net.minecraft.world.level.levelgen.structure.structures.SwampHutPiece
//   net.minecraft.world.level.levelgen.structure.structures.DesertPyramidPiece
//       (both extend ScatteredFeaturePiece; their ctor calls
//        getRandomHorizontalDirection(random) -> exactly one RandomSource.nextInt(4)
//        draw, then StructurePiece.makeBoundingBox + setOrientation)
//   net.minecraft.util.RandomSource (LegacyRandomSource via create(seed))
//
// The C++ ScatteredFeaturePieceBox.h replays the same seeded RandomSource and
// recomputes box + orientation/rotation/mirror; the test compares each row
// bit-for-bit (all ints here, decimal).
//
//   tools/run_groundtruth.ps1 -Tool ScatteredFeatureBoxParity -Out mcpp/build/scattered_feature_box.tsv
//
// Enum ordinals are exchanged as ints (match the C++ enums in StructurePieceMath.h):
//   Direction: DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5  (Direction.ordinal())
//   Mirror:    NONE=0, LEFT_RIGHT=1, FRONT_BACK=2              (Mirror.ordinal())
//   Rotation:  NONE=0, CLOCKWISE_90=1, CLOCKWISE_180=2, COUNTERCLOCKWISE_90=3
//
// TSV rows (leading TAG, all integers decimal):
//   SWAMP   <seed> <west> <north> <dirOrd> <rotOrd> <mirOrd> <minX> <minY> <minZ> <maxX> <maxY> <maxZ>
//   DESERT  <seed> <west> <north> <dirOrd> <rotOrd> <mirOrd> <minX> <minY> <minZ> <maxX> <maxY> <maxZ>
//
// O is captured at class load so any bootstrap chatter on stdout stays out of
// the TSV.

import java.lang.reflect.Field;
import net.minecraft.core.Direction;
import net.minecraft.server.Bootstrap;
import net.minecraft.SharedConstants;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.structures.DesertPyramidPiece;
import net.minecraft.world.level.levelgen.structure.structures.SwampHutPiece;

public class ScatteredFeatureBoxParity {
    static final java.io.PrintStream O = System.out;

    // Reflectively read the private/protected orientation off the constructed
    // piece so we certify the value the ctor stored (NOT a re-derivation here).
    static final Field ORIENTATION_FIELD;
    static {
        try {
            ORIENTATION_FIELD = StructurePiece.class.getDeclaredField("orientation");
            ORIENTATION_FIELD.setAccessible(true);
        } catch (NoSuchFieldException e) {
            throw new RuntimeException(e);
        }
    }

    static Direction orientationOf(StructurePiece p) {
        try {
            return (Direction) ORIENTATION_FIELD.get(p);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }
    }

    static void emit(String tag, long seed, int west, int north, StructurePiece p) {
        BoundingBox bb = p.getBoundingBox();
        Direction dir = orientationOf(p);
        O.println(tag
            + "\t" + seed + "\t" + west + "\t" + north
            + "\t" + dir.ordinal()
            + "\t" + p.getRotation().ordinal()
            + "\t" + p.getMirror().ordinal()
            + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ()
            + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ());
    }

    public static void main(String[] args) {
        // makeBoundingBox can produce an inverted box at the int-wrap edge of
        // (x + depth - 1); the BoundingBox ctor then logs an ERROR via log4j whose
        // default ConsoleAppender writes straight to the stdout FD and would pollute
        // the TSV. Silence the root logger before any logging occurs. (The box is
        // still normalized by the ctor's swap — that swap is what we certify.)
        org.apache.logging.log4j.core.config.Configurator.setRootLevel(org.apache.logging.log4j.Level.OFF);

        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Finite, well-spread inputs. Seeds chosen so nextInt(4) lands on all four
        // directions; west/north span negative, zero, large positive, and values
        // near the int-wrap edge of makeBoundingBox (x + depth - 1).
        long[] seeds = {
            0L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 42L, 123L, 1000L, 999999L,
            -1L, -42L, 0x5DEECE66DL, 1234567890123L, 0xCAFEBABEL, 8675309L
        };
        int[] coords = {
            0, 16, -16, 64, -64, 256, -256, 1024, -1024,
            Integer.MAX_VALUE - 21, Integer.MIN_VALUE, Integer.MAX_VALUE - 1
        };

        for (long seed : seeds) {
            for (int west : coords) {
                for (int north : coords) {
                    RandomSource r1 = RandomSource.create(seed);
                    emit("SWAMP", seed, west, north, new SwampHutPiece(r1, west, north));

                    RandomSource r2 = RandomSource.create(seed);
                    emit("DESERT", seed, west, north, new DesertPyramidPiece(r2, west, north));
                }
            }
        }
    }
}

// Ground-truth generator for the RNG-driven room-placement geometry, driving the
// REAL decompiled 26.1.2 class:
//
//   net.minecraft.world.level.levelgen.structure.structures
//       .MineshaftPieces.MineShaftRoom(int genDepth, RandomSource random,
//            int west, int north, MineshaftStructure.Type type)
//                                                  [MineshaftPieces.java:1068-1076]
//   net.minecraft.util.RandomSource (LegacyRandomSource via create(seed))
//
// The MineShaftRoom constructor's only computation is the BoundingBox it passes to
// its super-constructor:
//
//   new BoundingBox(west, 50, north,
//                   west  + 7 + random.nextInt(6),    // maxX  (draw #1)
//                   54        + random.nextInt(6),    // maxY  (draw #2)
//                   north + 7 + random.nextInt(6));   // maxZ  (draw #3)
//
// StructurePiece stores that box verbatim (no further RNG), so reading back
// getBoundingBox() recovers exactly the three nextInt(6) draws in argument
// (maxX, maxY, maxZ) order. `type` (NORMAL/MESA) does not affect this box.
//
// MineShaftRoom and its constructor are public, so it is instantiated directly
// (no reflection needed); the public getBoundingBox() reads back the result.
//
// TSV rows (leading TAG, all integers decimal):
//   ROOM <seed> <west> <north> <minX> <minY> <minZ> <maxX> <maxY> <maxZ>
//
//   tools/run_groundtruth.ps1 -Tool MineShaftRoomBoxParity -Out mcpp/build/mine_shaft_room_box.tsv
//
// O is captured at class load so any bootstrap chatter on stdout stays out of the
// TSV.

import net.minecraft.server.Bootstrap;
import net.minecraft.SharedConstants;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.structures.MineshaftPieces;
import net.minecraft.world.level.levelgen.structure.structures.MineshaftStructure;

@SuppressWarnings({"deprecation", "unchecked"})
public class MineShaftRoomBoxParity {
    static final java.io.PrintStream O = System.out;

    static void emit(long seed, int west, int north, BoundingBox bb) {
        O.println("ROOM\t" + seed + "\t" + west + "\t" + north
            + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ()
            + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ());
    }

    public static void main(String[] args) {
        // The BoundingBox ctor can normalize an inverted box (at int-wrap edges of
        // west/north + the +7+draw offset) and log via log4j to the stdout FD,
        // which would pollute the TSV; silence the root logger before any logging.
        org.apache.logging.log4j.core.config.Configurator.setRootLevel(
            org.apache.logging.log4j.Level.OFF);

        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Finite, well-spread inputs. Seeds chosen so the three nextInt(6) draws
        // land across the full 0..5 range. west/north span negative, zero, large
        // positive, and a value near the int-wrap edge (the offset is +7+draw, max
        // +12, so west near Integer.MAX_VALUE - 12 wraps and exercises the
        // inverted-bounds normalization in the BoundingBox ctor).
        long[] seeds = {
            0L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L, 9L, 42L, 123L, 1000L, 999999L,
            -1L, -42L, 0x5DEECE66DL, 1234567890123L, 0xCAFEBABEL, 8675309L
        };
        int[] coords = {
            0, 16, -16, 64, -64, 256, -256, 1024, -1024,
            Integer.MAX_VALUE - 12, Integer.MIN_VALUE, Integer.MAX_VALUE - 1
        };
        // type does not affect the box, but vary it to certify that independence.
        MineshaftStructure.Type[] types = {
            MineshaftStructure.Type.NORMAL, MineshaftStructure.Type.MESA
        };

        for (long seed : seeds) {
            for (int west : coords) {
                for (int north : coords) {
                    for (MineshaftStructure.Type type : types) {
                        RandomSource r = RandomSource.create(seed);
                        MineshaftPieces.MineShaftRoom room =
                            new MineshaftPieces.MineShaftRoom(0, r, west, north, type);
                        emit(seed, west, north, room.getBoundingBox());
                    }
                }
            }
        }
    }
}

// Reference value generator for the C++ StrongholdPieces small-door selector port
//   (mcpp/src/world/level/levelgen/structure/structures/StrongholdSmallDoor.h).
//
// Drives the REAL decompiled method
//   net.minecraft.world.level.levelgen.structure.structures
//       .StrongholdPieces$StrongholdPiece.randomSmallDoor(RandomSource)
// from client.jar. It is a protected instance method on the ABSTRACT base
// StrongholdPiece; its body is purely:
//
//   int selection = random.nextInt(5);
//   switch (selection) {
//      case 0: case 1: default: return SmallDoorType.OPENING;
//      case 2:                  return SmallDoorType.WOOD_DOOR;
//      case 3:                  return SmallDoorType.GRATES;
//      case 4:                  return SmallDoorType.IRON_DOOR;
//   }
//
// Because the declaring class is abstract, we allocate a CONCRETE subclass
// instance — StrongholdPieces$Straight — WITHOUT running any constructor, via
// sun.misc.Unsafe.allocateInstance(...) reached PURELY REFLECTIVELY (no
// `import sun.misc.Unsafe`, so javac emits no internal-API warning). The
// instance's fields are never read by randomSmallDoor, so a zero-initialised
// instance is sufficient and we never touch the piece accessor / world.
//
// For each case the driver:
//   1. seeds RandomSource.create(seed) (a LegacyRandomSource, the production
//      type behind every Stronghold piece constructor),
//   2. calls the REAL randomSmallDoor(random) reflectively COUNT times in a row,
//      recording each returned SmallDoorType.ordinal(),
//   3. draws one more random.nextLong() at the end and records it as a witness:
//      a port that consumed a different number of RNG values per call would
//      change this long, catching a draw-count bug the door codes alone miss.
//
// SmallDoorType.ordinal(): OPENING=0, WOOD_DOOR=1, GRATES=2, IRON_DOOR=3
// (enum declaration order) — emitted verbatim; the C++ port mirrors these.
//
//   javac -cp 26.1.2/client.jar;26.1.2/libs/* -d <out> StrongholdSmallDoorParity.java
//   java  -cp <out>;26.1.2/client.jar;26.1.2/libs/* StrongholdSmallDoorParity > ssd.tsv
//
// Rows (tab-separated):
//   SSD  <seed>  <count>  <ord0> <ord1> ... <ord{count-1}>  <afterLong>
//     ordK      = SmallDoorType ordinal of the K-th randomSmallDoor() call
//     afterLong = random.nextLong() drawn immediately after the last call
//
// O is captured at class load so any bootstrap chatter on stdout stays out of
// the TSV.

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.SharedConstants;
import net.minecraft.server.Bootstrap;
import net.minecraft.util.RandomSource;

@SuppressWarnings({"deprecation", "unchecked"})
public class StrongholdSmallDoorParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Abstract declaring class + its protected randomSmallDoor(RandomSource).
        Class<?> pieceClass =
            Class.forName("net.minecraft.world.level.levelgen.structure.structures."
                          + "StrongholdPieces$StrongholdPiece");
        Method randomSmallDoor =
            pieceClass.getDeclaredMethod("randomSmallDoor", RandomSource.class);
        randomSmallDoor.setAccessible(true);

        // A concrete subclass to instantiate (no constructor is run).
        Class<?> concreteClass =
            Class.forName("net.minecraft.world.level.levelgen.structure.structures."
                          + "StrongholdPieces$Straight");

        // sun.misc.Unsafe.allocateInstance(...) reached purely reflectively.
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        Object unsafe = theUnsafe.get(null);
        Method allocateInstance =
            unsafeClass.getMethod("allocateInstance", Class.class);
        Object piece = allocateInstance.invoke(unsafe, concreteClass);

        long[] seeds = {
            0L, 1L, 2L, 3L, 4L, 5L, 42L, 7L, 8675309L, 123456789L,
            -1L, -2L, -3L, -987654321L, 2147483647L, -2147483648L,
            1234567890123456789L, -1234567890123456789L,
            4503599627370496L, -4503599627370496L, 999999999999L, 100L, 65536L,
            -100L, 256L, 1000000L, -1000000L, 31L, 32L, 33L,
        };

        // Number of consecutive randomSmallDoor() calls per case. Picked so the
        // emitted ordinal stream exercises all five nextInt(5) outcomes (and thus
        // every switch arm, including the 0/1/default OPENING fall-through) across
        // the seed set.
        int count = 32;

        for (long seed : seeds) {
            RandomSource random = RandomSource.create(seed);
            StringBuilder row = new StringBuilder();
            row.append("SSD\t").append(seed).append('\t').append(count);
            for (int k = 0; k < count; k++) {
                Object doorType = randomSmallDoor.invoke(piece, random);
                int ordinal = ((Enum<?>) doorType).ordinal();
                row.append('\t').append(ordinal);
            }
            long after = random.nextLong();
            row.append('\t').append(after);
            O.println(row.toString());
        }
    }
}

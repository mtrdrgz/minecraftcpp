// Reference value generator for the C++ JungleTempleStoneSelector port
//   (mcpp/src/world/level/levelgen/structure/structures/JungleTempleStoneSelector.h).
//
// Drives the REAL decompiled inner class
//   net.minecraft.world.level.levelgen.structure.structures
//       .JungleTemplePiece$MossStoneSelector
// from client.jar. The class is a private static StructurePiece.BlockSelector;
// its behavioural method is:
//
//   public void next(RandomSource random, int worldX, int worldY, int worldZ,
//                    boolean isEdge) {
//      if (random.nextFloat() < 0.4F) this.next = COBBLESTONE.defaultBlockState();
//      else                           this.next = MOSSY_COBBLESTONE.defaultBlockState();
//   }
//   public BlockState getNext() { return this.next; }
//
// For each case the driver:
//   1. seeds RandomSource.create(seed) (a LegacyRandomSource, the production
//      type used by generateBox),
//   2. calls the REAL next(random, wx, wy, wz, isEdge) reflectively N times in a
//      row (one block cell per call), recording for each the selected block,
//   3. draws one more random.nextLong() at the end and records it. That witness
//      proves the selector consumed exactly ONE nextFloat() per call: a port
//      that drew a different number of values would change this long.
//
// The selected block is emitted as an opaque code derived from the REAL
// BuiltInRegistries.BLOCK key:
//   0 = minecraft:cobblestone        (Blocks.COBBLESTONE)
//   1 = minecraft:mossy_cobblestone  (Blocks.MOSSY_COBBLESTONE)
// The C++ side certifies the branch selection LOGIC (nextFloat() < 0.4f) and the
// RNG-consumption count, not the registry table.
//
//   javac -cp 26.1.2/client.jar;26.1.2/libs/* -d <out> JungleTempleStoneSelectorParity.java
//   java  -cp <out>;26.1.2/client.jar;26.1.2/libs/* JungleTempleStoneSelectorParity > jtss.tsv
//
// Rows (tab-separated):
//   JTSS  <seed>  <count>  <code0> <code1> ... <code{count-1}>  <afterLong>
//     codeK  = block code (0 cobblestone / 1 mossy) of the K-th next() call
//     afterLong = random.nextLong() drawn immediately after the last next()
//
// O is captured at class load so any bootstrap chatter on stdout stays out of
// the TSV.

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import net.minecraft.SharedConstants;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.server.Bootstrap;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;

public class JungleTempleStoneSelectorParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Resolve the REAL private inner selector class and its members.
        Class<?> selectorClass =
            Class.forName("net.minecraft.world.level.levelgen.structure.structures."
                          + "JungleTemplePiece$MossStoneSelector");
        Constructor<?> ctor = selectorClass.getDeclaredConstructor();
        ctor.setAccessible(true);
        Method nextMethod =
            selectorClass.getMethod("next", RandomSource.class, int.class, int.class,
                                     int.class, boolean.class);
        // The enclosing class is private, so even these public methods need
        // setAccessible(true) to be invoked reflectively from here.
        nextMethod.setAccessible(true);
        // getNext() is declared on the abstract base StructurePiece.BlockSelector.
        Method getNextMethod = selectorClass.getMethod("getNext");
        getNextMethod.setAccessible(true);

        // Identities of the two real blocks the selector can pick, captured via
        // the production registry so the emitted codes are genuine vanilla.
        Block cobblestone =
            BuiltInRegistries.BLOCK.getValue(Identifier.withDefaultNamespace("cobblestone"));
        Block mossyCobblestone =
            BuiltInRegistries.BLOCK.getValue(Identifier.withDefaultNamespace("mossy_cobblestone"));

        long[] seeds = {
            0L, 1L, 2L, 3L, 42L, 7L, 8675309L, 123456789L,
            -1L, -2L, -987654321L, 2147483647L, -2147483648L,
            1234567890123456789L, -1234567890123456789L,
            4503599627370496L, -4503599627370496L, 999999999999L, 100L, 65536L,
        };

        // Varied world coordinates / isEdge flags. The selector ignores them; we
        // pass distinct values to prove they never change the result.
        int[][] coords = {
            {0, 0, 0}, {1, 2, 3}, {-5, 64, -9}, {1000, -64, 2000},
            {-2147483648, 2147483647, 0}, {7, 7, 7},
        };
        boolean[] edges = {false, true};

        // Number of consecutive cells (next() calls) per case.
        int count = 16;

        for (long seed : seeds) {
            for (int[] c : coords) {
                for (boolean isEdge : edges) {
                    Object selector = ctor.newInstance();
                    RandomSource random = RandomSource.create(seed);
                    StringBuilder row = new StringBuilder();
                    row.append("JTSS\t").append(seed).append('\t').append(count);
                    for (int k = 0; k < count; k++) {
                        nextMethod.invoke(selector, random, c[0], c[1], c[2], isEdge);
                        BlockState state = (BlockState) getNextMethod.invoke(selector);
                        Block block = state.getBlock();
                        int code;
                        if (block == cobblestone) {
                            code = 0;
                        } else if (block == mossyCobblestone) {
                            code = 1;
                        } else {
                            throw new IllegalStateException(
                                "unexpected selector block: "
                                + BuiltInRegistries.BLOCK.getKey(block));
                        }
                        row.append('\t').append(code);
                    }
                    long after = random.nextLong();
                    row.append('\t').append(after);
                    O.println(row.toString());
                }
            }
        }
    }
}

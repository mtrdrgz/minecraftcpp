// Ground-truth generator for net.minecraft.core.QuartPos using the REAL
// decompiled 26.1.2 class. Pure static int bit arithmetic — no registries, no
// Bootstrap required. Calls the real public methods directly.
//
//   tools/run_groundtruth.ps1 -Tool QuartPosParity -Out mcpp/build/quart_pos.tsv
//
// All values are int — emitted in decimal; the C++ test recomputes and must
// match bit-for-bit.

import net.minecraft.core.QuartPos;

public class QuartPosParity {
    static final java.io.PrintStream O = System.out;

    // Thorough battery: zeros, +/-1, around the 4-block quart boundary (so the
    // arithmetic-shift sign behaviour and the &3 mask on negatives are exercised),
    // section-scale values, and extremes near the int range / chunk world limits.
    static final int[] VALS = {
        0, 1, 2, 3, 4, 5, 6, 7, 8,
        -1, -2, -3, -4, -5, -6, -7, -8, -9,
        15, 16, 17, -15, -16, -17,
        31, 32, 33, -31, -32, -33,
        63, 64, 65, -63, -64, -65,
        100, -100, 127, 128, -128, 255, 256, -256,
        1000, -1000, 4096, -4096,
        1048575, 1048576, -1048576, 2097151, 2097152, -2097152,
        30000000, -30000000, 33554431, -33554432,
        Integer.MAX_VALUE, Integer.MIN_VALUE,
        Integer.MAX_VALUE - 1, Integer.MIN_VALUE + 1,
        Integer.MAX_VALUE - 3, Integer.MIN_VALUE + 3,
        536870911, -536870912, 268435455, -268435456,
        1431655765, -1431655765
    };

    public static void main(String[] args) {
        for (int v : VALS) {
            O.println("FROM_BLOCK\t"   + v + "\t" + QuartPos.fromBlock(v));
            O.println("QUART_LOCAL\t"  + v + "\t" + QuartPos.quartLocal(v));
            O.println("TO_BLOCK\t"     + v + "\t" + QuartPos.toBlock(v));
            O.println("FROM_SECTION\t" + v + "\t" + QuartPos.fromSection(v));
            O.println("TO_SECTION\t"   + v + "\t" + QuartPos.toSection(v));
        }
        // Round-trips through the boundary: every quart maps back through a block,
        // and fromBlock(toBlock(q)) == q for all q; this also exercises shift
        // overflow on the extremes of toBlock / fromSection (well-defined in Java
        // because the value is truncated to 32 bits).
        for (int v : VALS) {
            O.println("RT_QB\t" + v + "\t" + QuartPos.fromBlock(QuartPos.toBlock(v)));
            O.println("RT_QS\t" + v + "\t" + QuartPos.toSection(QuartPos.fromSection(v)));
        }
    }
}

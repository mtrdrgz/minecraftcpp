// Ground-truth generator that VERIFIES the existing C++ port of
// net.minecraft.core.QuartPos (mcpp/src/core/QuartPos.h) against the REAL
// decompiled 26.1.2 class.
//
// QuartPos is pure static int bit arithmetic: no registries, no world, no
// Bootstrap. We call the real methods through reflection + setAccessible (the
// methods are public, but reflection is used per the repo's verify convention
// so the gate exercises the genuine net.minecraft.core.QuartPos bytecode rather
// than a re-declared signature). We also read the public-static-final constants
// BITS/SIZE/MASK and the private SECTION_TO_QUARTS_BITS via reflection.
//
//   tools/run_groundtruth.ps1 -Tool QuartPosVerifyParity -Out mcpp/build/quart_pos_verify.tsv
//
// Output: tab-separated <TAG>\t<input>\t<output> rows on STDOUT. Every value is
// an int, emitted in decimal; the C++ test recomputes via mc::quartpos and
// compares bit-for-bit. Constants emit on a single CONSTS row.

import java.lang.reflect.Field;
import java.lang.reflect.Method;

public class QuartPosVerifyParity {
    static final java.io.PrintStream O = System.out;

    // FINITE / PHYSICAL int battery. Covers: zero, +/-1, the 4-block quart
    // boundary on both sign sides (so the arithmetic >>2 sign-fill and the &3
    // mask on negative coords are exercised — Java's &3 on -1 yields 3, a known
    // foot-gun), 16/32/64-aligned section scales, chunk/world-height limits, and
    // the int extremes (MAX/MIN +/-1, +/-3) which exercise <<2 two's-complement
    // truncation. No NaN/Inf/-0.0 — these are ints; every value is finite.
    static final int[] VALS = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
        -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12,
        15, 16, 17, -15, -16, -17,
        31, 32, 33, -31, -32, -33,
        63, 64, 65, -63, -64, -65,
        100, -100, 127, 128, -128, 255, 256, -256, 511, 512, -512,
        1000, -1000, 4096, -4096, 8191, 8192, -8192,
        1048575, 1048576, -1048576, 2097151, 2097152, -2097152,
        30000000, -30000000, 33554431, -33554432,
        536870911, -536870912, 268435455, -268435456,
        1431655765, -1431655765,
        Integer.MAX_VALUE, Integer.MIN_VALUE,
        Integer.MAX_VALUE - 1, Integer.MIN_VALUE + 1,
        Integer.MAX_VALUE - 3, Integer.MIN_VALUE + 3
    };

    static int call(Method m, int v) throws Exception {
        return (Integer) m.invoke(null, v);
    }

    public static void main(String[] args) throws Exception {
        Class<?> cls = Class.forName("net.minecraft.core.QuartPos");

        Method fromBlock   = cls.getDeclaredMethod("fromBlock", int.class);
        Method quartLocal  = cls.getDeclaredMethod("quartLocal", int.class);
        Method toBlock     = cls.getDeclaredMethod("toBlock", int.class);
        Method fromSection = cls.getDeclaredMethod("fromSection", int.class);
        Method toSection   = cls.getDeclaredMethod("toSection", int.class);
        for (Method m : new Method[]{fromBlock, quartLocal, toBlock, fromSection, toSection}) {
            m.setAccessible(true);
        }

        // Constants — including the private SECTION_TO_QUARTS_BITS.
        Field fBits = cls.getDeclaredField("BITS");
        Field fSize = cls.getDeclaredField("SIZE");
        Field fMask = cls.getDeclaredField("MASK");
        Field fS2Q  = cls.getDeclaredField("SECTION_TO_QUARTS_BITS");
        for (Field f : new Field[]{fBits, fSize, fMask, fS2Q}) f.setAccessible(true);
        // CONSTS \t BITS \t SIZE \t MASK \t SECTION_TO_QUARTS_BITS
        O.println("CONSTS\t" + fBits.getInt(null) + "\t" + fSize.getInt(null)
                  + "\t" + fMask.getInt(null) + "\t" + fS2Q.getInt(null));

        for (int v : VALS) {
            O.println("FROM_BLOCK\t"   + v + "\t" + call(fromBlock, v));
            O.println("QUART_LOCAL\t"  + v + "\t" + call(quartLocal, v));
            O.println("TO_BLOCK\t"     + v + "\t" + call(toBlock, v));
            O.println("FROM_SECTION\t" + v + "\t" + call(fromSection, v));
            O.println("TO_SECTION\t"   + v + "\t" + call(toSection, v));
        }

        // Round-trips through the quart<->block and quart<->section boundaries.
        // fromBlock(toBlock(q)) == q for all q (lossless); toSection(fromSection(q))
        // == q likewise. These also exercise <<2 two's-complement truncation on the
        // extremes (well-defined in Java because the result is truncated to int32).
        for (int v : VALS) {
            O.println("RT_QB\t" + v + "\t" + call(fromBlock, call(toBlock, v)));
            O.println("RT_QS\t" + v + "\t" + call(toSection, call(fromSection, v)));
        }
    }
}

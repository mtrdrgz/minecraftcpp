// Reference value generator for the C++ Xoroshiro128PlusPlus port
// (mc::levelgen::Xoroshiro128PlusPlus in world/level/levelgen/RandomSource.h).
//
// Runs the REAL decompiled net.minecraft.world.level.levelgen.Xoroshiro128PlusPlus
// from client.jar so the emitted nextLong() sequences are exact ground truth for
// the rotateLeft / XOR / shift state update. The two-arg (seedLo, seedHi)
// constructor and nextLong() are public, so we call them directly; the zero-seed
// fallback (seedLo|seedHi == 0 -> GOLDEN_RATIO_64 / SILVER_RATIO_64) is exercised
// by including the (0,0) seed pair in the battery.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/Xoroshiro128Parity.java
//   java  -cp <out>;26.1.2/client.jar Xoroshiro128Parity > xoroshiro128.tsv
//
// Rows are tab-separated. Longs are emitted as 16-hex-digit raw bit patterns
// (Long.toHexString zero-padded) so the C++ side compares bit-for-bit:
//
//   SEQ  <seedLoHex> <seedHiHex> <step>  <nextLongHex>
//        From a Xoroshiro128PlusPlus(seedLo, seedHi), the value returned by the
//        (step+1)-th nextLong() call (step is 0-based). One row per draw so the
//        full state-advance chain is checked, not just the first output.
//
// O is captured at class load so any bootstrap chatter on stdout stays out of
// the TSV.
public class Xoroshiro128Parity {
    static final java.io.PrintStream O = System.out;

    // 16-hex-digit zero-padded raw bits of a long (two's complement pattern).
    static String hx(long v) {
        String s = Long.toHexString(v);
        StringBuilder sb = new StringBuilder();
        for (int i = s.length(); i < 16; i++) sb.append('0');
        sb.append(s);
        return sb.toString();
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Public (long seedLo, long seedHi) constructor and public nextLong().
        Class<?> cls = Class.forName(
            "net.minecraft.world.level.levelgen.Xoroshiro128PlusPlus");
        java.lang.reflect.Constructor<?> ctor =
            cls.getDeclaredConstructor(long.class, long.class);
        ctor.setAccessible(true);
        java.lang.reflect.Method mNext = cls.getDeclaredMethod("nextLong");
        mNext.setAccessible(true);

        // Seed-pair battery: finite/physical 64-bit values only. Includes the
        // (0,0) pair (triggers the GOLDEN/SILVER fallback), all four sign-corner
        // single-bit / extreme combos, the golden/silver constants themselves,
        // and a spread of arbitrary mixed seeds. No NaN/Infinity (these are
        // integer seeds, so every bit pattern is a legal physical input).
        long G = -7046029254386353131L;  // RandomSupport.GOLDEN_RATIO_64
        long S = 7640891576956012809L;   // RandomSupport.SILVER_RATIO_64
        long[][] seeds = new long[][] {
            { 0L, 0L },                       // zero -> fallback to (G, S)
            { 1L, 0L },                       // not zero (no fallback)
            { 0L, 1L },                       // not zero (no fallback)
            { 1L, 1L },
            { -1L, -1L },                     // all bits set
            { -1L, 0L },
            { 0L, -1L },
            { Long.MAX_VALUE, Long.MIN_VALUE },
            { Long.MIN_VALUE, Long.MAX_VALUE },
            { Long.MIN_VALUE, Long.MIN_VALUE },
            { Long.MAX_VALUE, Long.MAX_VALUE },
            { G, S },                         // the canonical fallback constants
            { S, G },
            { 42L, 1337L },
            { 123456789L, 987654321L },
            { -1234567890123456789L, 8675309L },
            { 0x0123456789ABCDEFL, 0xFEDCBA9876543210L },
            { 0xDEADBEEFCAFEBABEL, 0x1122334455667788L },
            { -9223372036854775808L, 9223372036854775807L },
            { 7L, -7L },
            { 0x8000000000000000L, 0x0000000000000001L },
        };

        final int STEPS = 32;  // advance the state 32 times per seed pair

        for (long[] pair : seeds) {
            long lo = pair[0];
            long hi = pair[1];
            Object rng = ctor.newInstance(lo, hi);
            for (int step = 0; step < STEPS; step++) {
                long v = (Long) mNext.invoke(rng);
                O.println("SEQ\t" + hx(lo) + "\t" + hx(hi) + "\t" + step + "\t" + hx(v));
            }
        }
    }
}

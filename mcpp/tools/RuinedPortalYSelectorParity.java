// Ground-truth generator for the PURE, RNG-driven static helpers in the REAL
// decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.RuinedPortalStructure
//
//   private static boolean sample(WorldgenRandom random, float limit)        [.java:152-158]
//   private static int     getRandomWithinInterval(RandomSource random,
//                                                   int minPreferred, int max) [.java:227-229]
//
// Both are fully self-contained (NO level / chunk-generator / registry access);
// they are driven only by a RandomSource + int/float arithmetic. We invoke the
// REAL private static methods reflectively over a REAL seeded RandomSource so the
// emitted values + RNG-consumption pattern are exact ground truth. The body is
// NEVER replicated Java-side. The C++ port (RuinedPortalYSelector.h) replays the
// same seed with the certified mc::levelgen::LegacyRandomSource.
//
//   tools/run_groundtruth.ps1 -Tool RuinedPortalYSelectorParity -Out mcpp/build/ruined_portal_yselector.tsv
//
// sample() takes a WorldgenRandom; we wrap a LegacyRandomSource in a real
// WorldgenRandom. WorldgenRandom.next(bits) delegates to the wrapped
// LegacyRandomSource.next(bits) (WorldgenRandom.java:30-35), so nextFloat() is
// byte-identical to a direct LegacyRandomSource(seed). getRandomWithinInterval()
// takes the RandomSource interface; we pass a LegacyRandomSource directly.
//
// Tab-separated rows (floats as raw IEEE-754 bits per harness convention):
//   SAMPLE  <limitBits>  <seed>  <result 0|1>
//        RuinedPortalStructure.sample(new WorldgenRandom(LegacyRandomSource(seed)),
//                                     Float.intBitsToFloat(limitBits))
//   INTERVAL  <minPreferred>  <max>  <seed>  <result>
//        RuinedPortalStructure.getRandomWithinInterval(LegacyRandomSource(seed),
//                                                       minPreferred, max)
//
// O is captured at class load so any bootstrap chatter on stdout stays out of TSV.
@SuppressWarnings({"deprecation", "unchecked"})
public class RuinedPortalYSelectorParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> cls = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.RuinedPortalStructure");

        Class<?> worldgenRandomCls =
            Class.forName("net.minecraft.world.level.levelgen.WorldgenRandom");
        Class<?> randomSourceCls =
            Class.forName("net.minecraft.util.RandomSource");

        java.lang.reflect.Method sample =
            cls.getDeclaredMethod("sample", worldgenRandomCls, float.class);
        sample.setAccessible(true);

        java.lang.reflect.Method getRandomWithinInterval =
            cls.getDeclaredMethod("getRandomWithinInterval", randomSourceCls, int.class, int.class);
        getRandomWithinInterval.setAccessible(true);

        java.lang.reflect.Constructor<?> wgrCtor =
            worldgenRandomCls.getConstructor(randomSourceCls);

        // ---- sample(): limit battery -------------------------------------------------
        // Real call sites pass air_pocket_probability in [0,1] (codec floatRange).
        // Probe the two short-circuit sentinels (0.0F, 1.0F) plus a spread of
        // interior probabilities so the strict `<` and the single draw are pinned.
        float[] limits = {
            0.0F, 1.0F, 0.5F, 0.1F, 0.25F, 0.3F, 0.4F, 0.6F, 0.75F, 0.9F, 0.99F, 0.01F,
            0.07F, 0.2F,
        };
        long[] seeds = {
            0L, 1L, 2L, 42L, 123456789L, -1L, -987654321L,
            2147483647L, -1234567890123456789L, 8675309L, 1000000007L, -42L,
        };

        for (float limit : limits) {
            int limitBits = Float.floatToRawIntBits(limit);
            for (long seed : seeds) {
                Object legacy = newLegacy(seed);
                Object wgr = wgrCtor.newInstance(legacy);
                boolean r;
                try {
                    r = (Boolean) sample.invoke(null, wgr, limit);
                } catch (java.lang.reflect.InvocationTargetException e) {
                    throw rethrow(e);
                }
                O.println("SAMPLE\t" + limitBits + "\t" + seed + "\t" + (r ? 1 : 0));
            }
        }

        // ---- getRandomWithinInterval(): [minPreferred, max] battery ------------------
        // Real call sites: (70, surfaceY-ySpan) for IN_MOUNTAIN and (minY, surfaceY-ySpan)
        // for UNDERGROUND, where minY = heightAccessor.getMinY()+15. So minPreferred and
        // max can land in either order (the no-draw `max` branch fires when min>=max).
        // Battery covers min<max (draws), min==max and min>max (both return max, no draw),
        // plus negative y's typical of underground placement.
        int[][] intervals = {
            { 70, 80 },     // min < max, small range
            { 70, 71 },     // min < max, bound 2
            { 70, 70 },     // min == max -> return max, no draw
            { 70, 60 },     // min > max  -> return max, no draw
            { 15, 100 },    // wide range
            { -49, -10 },   // wholly negative (deep underground)
            { -49, 0 },
            { -64, 64 },    // straddles zero
            { 27, 29 },     // a real nether interior band
            { 32, 100 },    // a real nether air-pocket band
            { 0, 1 },       // min < max, smallest drawing range
            { 5, 5 },       // degenerate, no draw
            { 100, 70 },    // min > max, no draw
            { -1, 1 },
            { 16, 17 },
        };

        for (int[] iv : intervals) {
            int minPreferred = iv[0];
            int max = iv[1];
            for (long seed : seeds) {
                Object legacy = newLegacy(seed);
                int r;
                try {
                    r = (Integer) getRandomWithinInterval.invoke(null, legacy, minPreferred, max);
                } catch (java.lang.reflect.InvocationTargetException e) {
                    throw rethrow(e);
                }
                O.println("INTERVAL\t" + minPreferred + "\t" + max + "\t" + seed + "\t" + r);
            }
        }

        O.flush();
    }

    static Object newLegacy(long seed) throws Exception {
        Class<?> legacyCls =
            Class.forName("net.minecraft.world.level.levelgen.LegacyRandomSource");
        return legacyCls.getConstructor(long.class).newInstance(seed);
    }

    static RuntimeException rethrow(java.lang.reflect.InvocationTargetException e) {
        Throwable cause = e.getCause();
        if (cause instanceof RuntimeException) return (RuntimeException) cause;
        if (cause instanceof Error) throw (Error) cause;
        return new RuntimeException(cause);
    }
}

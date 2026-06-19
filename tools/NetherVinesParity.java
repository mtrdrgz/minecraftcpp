// Reference value generator for the C++ net.minecraft.world.level.block.NetherVines
// port (verified against mcpp/src/world/level/block/NetherVines.h ::
// mc::world::level::block::NetherVines).
//
// Runs the REAL decompiled net.minecraft.world.level.block.NetherVines from
// client.jar so the emitted growth-count sequences are exact ground truth. The
// behaviour under test is the bone-meal geometric-decay loop:
//
//     double growProbabilty = 1.0;
//     for (count = 0; random.nextDouble() < growProbabilty; count++)
//         growProbabilty *= 0.826;
//     return count;
//
// We drive it with a REAL net.minecraft.world.level.levelgen.LegacyRandomSource
// seeded per row, and the C++ test seeds an mc::levelgen::LegacyRandomSource
// IDENTICALLY (java.util.Random LCG -> next(26)/next(27) nextDouble). The method
// body is NEVER replicated Java-side: we reflectively invoke the real static.
//
//   mcpp/tools/run_groundtruth.ps1 -Tool NetherVinesParity -Out mcpp/build/nether_vines.tsv
//
// Rows are tab-separated:
//   NV  <seed>  <c0..c15>
//       NetherVines.getBlocksToGrowWhenBonemealed(LegacyRandomSource(seed))
//       invoked 16 times in sequence from ONE continuously-advancing RNG (no
//       reseed between calls), so each call resumes the previous RNG state. This
//       walks the LCG through a wide range of positions and exercises the loop
//       at many depths (short and long geometric tails) per seed.
//
// O is captured at class load so any bootstrap chatter on stdout stays out of
// the TSV.
@SuppressWarnings({"deprecation", "unchecked"})
public class NetherVinesParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> nvClass = Class.forName("net.minecraft.world.level.block.NetherVines");

        // getBlocksToGrowWhenBonemealed(RandomSource) is public static; the param
        // is the interface net.minecraft.util.RandomSource. Resolve by name+arity
        // so we don't depend on the exact parameter-type spelling, then open it.
        java.lang.reflect.Method grow = null;
        for (java.lang.reflect.Method m : nvClass.getDeclaredMethods()) {
            if (m.getName().equals("getBlocksToGrowWhenBonemealed") && m.getParameterCount() == 1) {
                grow = m;
                break;
            }
        }
        if (grow == null) {
            throw new IllegalStateException("getBlocksToGrowWhenBonemealed(RandomSource) not found");
        }
        grow.setAccessible(true);

        // FINITE battery of seeds spanning zero, small positives, the signed-int
        // boundary, large magnitudes and negatives, so the LCG starts from many
        // distinct internal states. 16 consecutive draws per seed amplify the
        // coverage to 16 * seeds distinct loop evaluations.
        long[] seeds = {
            0L, 1L, 2L, 3L, 7L, 42L, 99L, 123L, 1000L, 65535L,
            123456789L, 987654321L, 2147483647L, 2147483648L, 4294967296L,
            -1L, -2L, -42L, -987654321L, -1234567890123456789L,
            8675309L, 1311768467463790320L, 6148914691236517205L,
            -6148914691236517206L, 9223372036854775807L, -9223372036854775808L,
            314159265L, 271828182L, 161803398L, 141421356L,
            555555555L, -555555555L, 1024L, 4096L, 100000L, -100000L,
            777L, -777L, 13L, -13L,
        };

        for (long seed : seeds) {
            // ONE RNG advanced continuously across all 16 draws (no reseed).
            net.minecraft.world.level.levelgen.LegacyRandomSource rng =
                new net.minecraft.world.level.levelgen.LegacyRandomSource(seed);
            StringBuilder sb = new StringBuilder();
            sb.append("NV\t").append(seed);
            for (int i = 0; i < 16; i++) {
                int c = (Integer) grow.invoke(null, rng);
                sb.append('\t').append(c);
            }
            O.println(sb.toString());
        }
    }
}

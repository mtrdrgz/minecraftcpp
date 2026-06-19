// Reference value generator for the C++ net.minecraft.world.entity.ai.util.RandomPos
// port (mcpp/src/world/entity/ai/util/RandomPos.h ::
// mc::entity::ai::generateRandomDirection / generateRandomDirectionWithinRadians).
//
// Runs the REAL decompiled net.minecraft.world.entity.ai.util.RandomPos from
// client.jar, driving its PURE static helpers reflectively. Both helpers consume
// only a RandomSource + Mth and emit an integer BlockPos (no world/level access),
// so they are exact, deterministic ground truth.
//
// We seed a REAL net.minecraft.world.level.levelgen.LegacyRandomSource per row, and
// the C++ test seeds an mc::levelgen::LegacyRandomSource IDENTICALLY, replaying the
// same RNG stream. Input doubles are exchanged as raw IEEE-754 bit patterns
// (Double.doubleToRawLongBits, hex) so no decimal round-trip can perturb the low
// bits; outputs are integer BlockPos coordinates compared in decimal.
//
//   mcpp/tools/run_groundtruth.ps1 -Tool RandomPosParity -Out mcpp/build/random_pos.tsv
//
// Rows are tab-separated. All doubles are 16-hex-digit raw long bits.
//   DIR  <seed>  <horizontalDist>  <verticalDist>  <x> <y> <z>
//        generateRandomDirection(LegacyRandomSource(seed), hDist, vDist) -> BlockPos
//   RAD  <seed>  <minH#> <maxH#> <vDist> <flyH> <xDir#> <zDir#> <maxRad#>  <present> <x> <y> <z>
//        generateRandomDirectionWithinRadians(...) -> BlockPos or null (present=0/1)
//
// O is captured at class load so any bootstrap chatter on stdout stays out of the TSV.
public class RandomPosParity {
    static final java.io.PrintStream O = System.out;

    static String hx(double d) { return Long.toHexString(Double.doubleToRawLongBits(d)); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> rpClass = Class.forName("net.minecraft.world.entity.ai.util.RandomPos");
        Class<?> rsIface = Class.forName("net.minecraft.util.RandomSource");

        // generateRandomDirection(RandomSource, int, int) -> BlockPos
        java.lang.reflect.Method genDir =
            rpClass.getDeclaredMethod("generateRandomDirection", rsIface, int.class, int.class);
        genDir.setAccessible(true);

        // generateRandomDirectionWithinRadians(
        //   RandomSource, double, double, int, int, double, double, double) -> @Nullable BlockPos
        java.lang.reflect.Method genRad = rpClass.getDeclaredMethod(
            "generateRandomDirectionWithinRadians",
            rsIface, double.class, double.class, int.class, int.class,
            double.class, double.class, double.class);
        genRad.setAccessible(true);

        // BlockPos accessors (avoid importing the class directly).
        Class<?> bpClass = Class.forName("net.minecraft.core.BlockPos");
        java.lang.reflect.Method getX = bpClass.getMethod("getX");
        java.lang.reflect.Method getY = bpClass.getMethod("getY");
        java.lang.reflect.Method getZ = bpClass.getMethod("getZ");

        long[] seeds = { 0L, 1L, 2L, 42L, 123456789L, -1L, -987654321L,
                         2147483647L, -1234567890123456789L, 8675309L, 99L, 7L };

        // ---- DIR battery: generateRandomDirection -------------------------------
        int[][] dirCfg = new int[][] {
            { 1, 1 }, { 3, 1 }, { 5, 2 }, { 7, 3 }, { 7, 7 },
            { 10, 7 }, { 16, 0 }, { 0, 4 }, { 2, 2 }, { 31, 15 },
        };
        for (int[] c : dirCfg) {
            int h = c[0], v = c[1];
            for (long seed : seeds) {
                net.minecraft.world.level.levelgen.LegacyRandomSource rng =
                    new net.minecraft.world.level.levelgen.LegacyRandomSource(seed);
                Object bp = genDir.invoke(null, rng, h, v);
                int x = (Integer) getX.invoke(bp);
                int y = (Integer) getY.invoke(bp);
                int z = (Integer) getZ.invoke(bp);
                O.println("DIR\t" + seed + "\t" + h + "\t" + v + "\t" + x + "\t" + y + "\t" + z);
            }
        }

        // ---- RAD battery: generateRandomDirectionWithinRadians ------------------
        // Representative configs spanning: tight vs wide radial cones, fractional
        // distances, vertical offsets, flying heights, and direction vectors in all
        // quadrants (including axis-aligned and zero-ish) to exercise atan2's LUT,
        // the float-jitter promotion, the SQRT_OF_TWO scale, and the |xt|/|zt| gate.
        double[][] radCfg = new double[][] {
            // minH, maxH, vDist, flyH, xDir, zDir, maxRad
            {  4.0, 10.0, 7, 0,  1.0,  0.0, Math.PI },
            {  4.0, 10.0, 7, 0,  0.0,  1.0, Math.PI / 2 },
            {  6.0, 15.0, 5, 0, -1.0,  0.0, Math.PI / 4 },
            {  6.0, 15.0, 5, 0,  0.0, -1.0, 0.5 },
            {  2.0,  8.0, 3, 2,  1.0,  1.0, Math.PI / 6 },
            {  2.0,  8.0, 3, 2, -1.0, -1.0, 1.0 },
            { 10.0, 20.0, 8, 4,  3.0, -2.0, Math.PI },
            { 10.0, 20.0, 8, 4, -2.0,  3.0, 0.1 },
            {  1.0,  3.0, 1, 0,  0.5,  0.25, Math.PI / 3 },
            {  5.0, 12.0, 4, 1,  7.0,  7.0, 2.0 },
            {  3.0,  9.0, 6, 0, -5.0,  1.0, Math.PI },
            {  4.0, 16.0, 7, 0,  0.0,  0.0, Math.PI },   // degenerate dir vector
            {  8.0, 24.0, 10, 8, 100.0, -100.0, Math.PI },
            {  4.0, 10.0, 7, 0,  1.0,  0.0, 0.0 },       // zero cone (no jitter spread)
        };
        for (double[] c : radCfg) {
            double minH = c[0], maxH = c[1];
            int vDist = (int) c[2], flyH = (int) c[3];
            double xDir = c[4], zDir = c[5], maxRad = c[6];
            for (long seed : seeds) {
                net.minecraft.world.level.levelgen.LegacyRandomSource rng =
                    new net.minecraft.world.level.levelgen.LegacyRandomSource(seed);
                Object bp = genRad.invoke(null, rng, minH, maxH, vDist, flyH, xDir, zDir, maxRad);
                StringBuilder sb = new StringBuilder();
                sb.append("RAD\t").append(seed)
                  .append('\t').append(hx(minH)).append('\t').append(hx(maxH))
                  .append('\t').append(vDist).append('\t').append(flyH)
                  .append('\t').append(hx(xDir)).append('\t').append(hx(zDir))
                  .append('\t').append(hx(maxRad));
                if (bp == null) {
                    sb.append("\t0\t0\t0\t0");
                } else {
                    int x = (Integer) getX.invoke(bp);
                    int y = (Integer) getY.invoke(bp);
                    int z = (Integer) getZ.invoke(bp);
                    sb.append("\t1\t").append(x).append('\t').append(y).append('\t').append(z);
                }
                O.println(sb.toString());
            }
        }
    }
}

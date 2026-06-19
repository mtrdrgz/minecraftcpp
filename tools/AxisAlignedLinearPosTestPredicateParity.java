// Ground-truth generator for the COMPLETE boolean predicate
//   net.minecraft.world.level.levelgen.structure.templatesystem.AxisAlignedLinearPosTest
//       boolean test(BlockPos inTemplatePos, BlockPos worldPos,
//                    BlockPos worldReference, RandomSource random)
//
// Unlike LinearPosTestMathParity (which emits only the deterministic distance +
// threshold and EXCLUDES the random.nextFloat() draw), this gate drives the REAL
// AxisAlignedLinearPosTest.test() method end to end: it constructs the real rule,
// seeds a REAL RandomSource identically to the C++ side, and calls test() N times
// in a row so the whole nextFloat() stream is exercised. We record the boolean
// result of each call (the `rnd <= clampedLerp(inverseLerp(dist,..),..)` decision)
// PLUS the distance and threshold bits for diagnostics. No world, no datapack —
// test() touches only Direction, Mth, and random.nextFloat().
//
// Two RNG flavours, both certified + seeded IDENTICALLY on the C++ side:
//   LEG -> RandomSource.create(seed)        == LegacyRandomSource(seed)
//   XOR -> new XoroshiroRandomSource(seed)
//
//   tools/run_groundtruth.ps1 -Tool AxisAlignedLinearPosTestPredicateParity \
//       -Out mcpp/build/axis_aligned_linear_pos_predicate.tsv
//
// TSV row (tab-separated):
//   AALP <rng> <seedDec> <axisOrd> <wx> <wy> <wz> <rx> <ry> <rz>
//        <minDist> <maxDist> <minChanceBits> <maxChanceBits>
//        <distInt> <thresholdBits> <r0> ... <r{N-1}>
// where rk is the boolean test() result of the k-th consecutive call (0/1), the
// chance/threshold floats are Float.floatToRawIntBits (decimal int), and the RNG
// is re-seeded fresh per row so the C++ side can reproduce the identical stream.

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.structure.templatesystem.AxisAlignedLinearPosTest;

public class AxisAlignedLinearPosTestPredicateParity {
    static final java.io.PrintStream OUT = System.out;
    static final int N = 8; // consecutive test() calls per (rng,seed,rule,positions) row

    // Build a real RandomSource of the requested flavour, seeded identically to
    // the C++ side. "LEG" via the public factory; "XOR" via the package-private
    // (long) constructor (reflection + setAccessible).
    static RandomSource makeRng(String rng, long seed) throws Exception {
        if (rng.equals("LEG")) {
            return RandomSource.create(seed);
        } else if (rng.equals("XOR")) {
            Class<?> c = Class.forName("net.minecraft.world.level.levelgen.XoroshiroRandomSource");
            Constructor<?> ctor = c.getDeclaredConstructor(long.class);
            ctor.setAccessible(true);
            return (RandomSource) ctor.newInstance(seed);
        }
        throw new IllegalArgumentException("rng " + rng);
    }

    static Object field(Object o, String name) throws Exception {
        Field f = o.getClass().getDeclaredField(name);
        f.setAccessible(true);
        return f.get(o);
    }

    // The exact threshold the real test() compares random.nextFloat() against —
    // recomputed via the REAL Mth for the diagnostics column only (the boolean is
    // taken straight from test()).
    static float threshold(int dist, int minDist, int maxDist, float minChance, float maxChance) {
        float factor = Mth.inverseLerp((float) dist, (float) minDist, (float) maxDist);
        return Mth.clampedLerp(factor, minChance, maxChance);
    }

    // AxisAlignedLinearPosTest.test() distance — replicate AxisAlignedLinearPosTest.java:42-46
    // using the REAL Direction normal steps, for the diagnostics column only.
    static int axisDist(Direction.Axis axis, BlockPos worldPos, BlockPos worldRef) {
        Direction d = Direction.get(Direction.AxisDirection.POSITIVE, axis);
        float xd = Math.abs((worldPos.getX() - worldRef.getX()) * d.getStepX());
        float yd = Math.abs((worldPos.getY() - worldRef.getY()) * d.getStepY());
        float zd = Math.abs((worldPos.getZ() - worldRef.getZ()) * d.getStepZ());
        return (int) (xd + yd + zd);
    }

    static int bits(float f) { return Float.floatToRawIntBits(f); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Representative rule chance pairs [minChance, maxChance]; ctor requires
        // minDist < maxDist. Include in/decreasing, equal, and out-of-[0,1] chances
        // (clampedLerp clamps the FACTOR, not the chances, so chances may exceed 1).
        float[][] chances = {
            {0.0F, 1.0F}, {1.0F, 0.0F}, {0.25F, 0.75F}, {0.9F, 0.1F},
            {0.5F, 0.5F}, {0.05F, 0.95F}, {0.3333333F, 0.6666667F}, {-0.5F, 1.5F},
        };
        int[][] ranges = {
            {0, 1}, {0, 16}, {1, 8}, {-8, 8}, {3, 4}, {0, 1000},
        };

        // Position pairs spanning negatives, zero, large magnitudes, and a pair
        // that overflows the float mantissa (>2^24) so the float round-trip bites.
        int[][] worldPts = {
            {0, 0, 0}, {1, 2, 3}, {-5, -6, -7}, {16, -64, 16}, {100, 70, -100},
            {123456, -789, 654321}, {16777217, 0, 0}, {-2000000000, 5, 2000000000},
        };
        int[][] refPts = {
            {0, 0, 0}, {-1, -2, -3}, {7, 8, 9}, {16, 64, 16}, {-50, 60, 50},
            {654321, 789, -123456}, {-16777216, 0, 0}, {2000000000, -5, -2000000000},
        };

        Direction.Axis[] axes = { Direction.Axis.X, Direction.Axis.Y, Direction.Axis.Z };
        String[] rngs = { "LEG", "XOR" };
        long[] seeds = { 0L, 1L, 42L, -1L, 1234567890123456789L, 987654321L };

        // inTemplatePos is unused by the real body; a fixed value is fine.
        BlockPos inTemplate = BlockPos.ZERO;

        for (Direction.Axis axis : axes) {
            int axisOrd = axis.ordinal();
            for (float[] ch : chances) {
                for (int[] rg : ranges) {
                    AxisAlignedLinearPosTest rule =
                        new AxisAlignedLinearPosTest(ch[0], ch[1], rg[0], rg[1], axis);
                    int minDist = (Integer) field(rule, "minDist");
                    int maxDist = (Integer) field(rule, "maxDist");
                    float minChance = (Float) field(rule, "minChance");
                    float maxChance = (Float) field(rule, "maxChance");

                    for (int pi = 0; pi < worldPts.length; pi++) {
                        BlockPos wp = new BlockPos(worldPts[pi][0], worldPts[pi][1], worldPts[pi][2]);
                        BlockPos rp = new BlockPos(refPts[pi][0], refPts[pi][1], refPts[pi][2]);
                        int dist = axisDist(axis, wp, rp);
                        float thr = threshold(dist, minDist, maxDist, minChance, maxChance);

                        for (String rng : rngs) {
                            for (long seed : seeds) {
                                RandomSource r = makeRng(rng, seed);
                                StringBuilder sb = new StringBuilder();
                                sb.append("AALP\t").append(rng).append('\t').append(seed)
                                  .append('\t').append(axisOrd)
                                  .append('\t').append(wp.getX()).append('\t').append(wp.getY()).append('\t').append(wp.getZ())
                                  .append('\t').append(rp.getX()).append('\t').append(rp.getY()).append('\t').append(rp.getZ())
                                  .append('\t').append(minDist).append('\t').append(maxDist)
                                  .append('\t').append(bits(minChance)).append('\t').append(bits(maxChance))
                                  .append('\t').append(dist).append('\t').append(bits(thr));
                                for (int k = 0; k < N; k++) {
                                    boolean res = rule.test(inTemplate, wp, rp, r);
                                    sb.append('\t').append(res ? 1 : 0);
                                }
                                OUT.println(sb.toString());
                            }
                        }
                    }
                }
            }
        }

        OUT.flush();
    }
}

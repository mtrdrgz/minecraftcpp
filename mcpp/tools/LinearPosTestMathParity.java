// Ground-truth generator for the PURE distance + acceptance-threshold math in
// the REAL decompiled 26.1.2 classes:
//   net.minecraft.world.level.levelgen.structure.templatesystem.LinearPosTest
//   net.minecraft.world.level.levelgen.structure.templatesystem.AxisAlignedLinearPosTest
//
// Both PosRuleTest.test() implementations compute an integer distance from
// worldPos to worldReference, then return
//   random.nextFloat() <= Mth.clampedLerp(Mth.inverseLerp(dist, minDist, maxDist),
//                                          minChance, maxChance)
// We exercise the DETERMINISTIC half exactly as the real method does:
//   * the distance: LinearPosTest uses BlockPos.distManhattan(worldReference);
//     AxisAlignedLinearPosTest uses Direction.get(POSITIVE, axis).getStepX/Y/Z
//     and (int)(|dx*stepX| + |dy*stepY| + |dz*stepZ|).
//   * the threshold: the REAL net.minecraft.util.Mth.inverseLerp / clampedLerp
//     (float overloads), reading the rule's private fields via reflection.
// The trailing random.nextFloat() comparison is the ONLY impure part and is
// excluded. No world, no registry, no RandomSource here.
//
//   tools/run_groundtruth.ps1 -Tool LinearPosTestMathParity -Out mcpp/build/linear_pos_test_math.tsv
//
// TSV columns (per row, tab-separated):
//   LIN  wx wy wz rx ry rz minDist maxDist minChanceBits maxChanceBits | distInt thresholdBits
//   AXIS axisOrd wx wy wz rx ry rz minDist maxDist minChanceBits maxChanceBits | distInt thresholdBits
// Floats are emitted as Float.floatToRawIntBits (decimal int) so the C++ side
// can std::bit_cast<float> them with zero rounding loss.

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import net.minecraft.world.level.levelgen.structure.templatesystem.AxisAlignedLinearPosTest;
import net.minecraft.world.level.levelgen.structure.templatesystem.LinearPosTest;

public class LinearPosTestMathParity {
    static final java.io.PrintStream OUT = System.out;

    // Read a declared private field by name (works for the rule classes' final ints/floats).
    static Object field(Object o, String name) throws Exception {
        Field f = o.getClass().getDeclaredField(name);
        f.setAccessible(true);
        return f.get(o);
    }

    // The exact threshold the real PosRuleTest.test() compares random.nextFloat() against.
    // dist/minDist/maxDist promote to float -> Mth.inverseLerp(float,float,float);
    // minChance/maxChance are float -> Mth.clampedLerp(float,float,float).
    static float threshold(int dist, int minDist, int maxDist, float minChance, float maxChance) {
        float factor = Mth.inverseLerp((float) dist, (float) minDist, (float) maxDist);
        return Mth.clampedLerp(factor, minChance, maxChance);
    }

    // LinearPosTest.test() distance == BlockPos.distManhattan (real Vec3i method).
    static int linDist(BlockPos worldPos, BlockPos worldRef) {
        return worldPos.distManhattan(worldRef);
    }

    // AxisAlignedLinearPosTest.test() distance — replicate AxisAlignedLinearPosTest.java:42-46
    // using the REAL Direction normal steps.
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

        // ── Representative rule configs (ctor REQUIRES minDist < maxDist) ──────
        // [minChance, maxChance, minDist, maxDist]
        float[][] chances = {
            {0.0F, 1.0F}, {1.0F, 0.0F}, {0.25F, 0.75F}, {0.9F, 0.1F},
            {0.5F, 0.5F}, {0.05F, 0.95F}, {0.3333333F, 0.6666667F}, {-0.5F, 1.5F},
        };
        int[][] ranges = {
            {0, 1}, {0, 16}, {1, 8}, {-8, 8}, {-100, -10}, {3, 4}, {0, 1000}, {-2147483648, 2147483647},
        };

        // Position pairs spanning negatives, zero, large magnitudes, and a pair
        // that overflows the float mantissa (>2^24) so the float round-trip bites.
        int[][] worldPts = {
            {0, 0, 0}, {1, 2, 3}, {-5, -6, -7}, {16, -64, 16}, {100, 70, -100},
            {123456, -789, 654321}, {16777217, 0, 0}, {-2000000000, 5, 2000000000},
            {2147483647, 2147483647, 2147483647},
        };
        int[][] refPts = {
            {0, 0, 0}, {-1, -2, -3}, {7, 8, 9}, {16, 64, 16}, {-50, 60, 50},
            {654321, 789, -123456}, {-16777216, 0, 0}, {2000000000, -5, -2000000000},
            {-2147483648, -2147483648, -2147483648},
        };

        Direction.Axis[] axes = { Direction.Axis.X, Direction.Axis.Y, Direction.Axis.Z };

        // ── LinearPosTest rows ────────────────────────────────────────────────
        for (float[] ch : chances) {
            for (int[] rg : ranges) {
                LinearPosTest rule = new LinearPosTest(ch[0], ch[1], rg[0], rg[1]);
                int minDist = (Integer) field(rule, "minDist");
                int maxDist = (Integer) field(rule, "maxDist");
                float minChance = (Float) field(rule, "minChance");
                float maxChance = (Float) field(rule, "maxChance");
                for (int pi = 0; pi < worldPts.length; pi++) {
                    BlockPos wp = new BlockPos(worldPts[pi][0], worldPts[pi][1], worldPts[pi][2]);
                    BlockPos rp = new BlockPos(refPts[pi][0], refPts[pi][1], refPts[pi][2]);
                    int dist = linDist(wp, rp);
                    float thr = threshold(dist, minDist, maxDist, minChance, maxChance);
                    OUT.println("LIN\t" + wp.getX() + "\t" + wp.getY() + "\t" + wp.getZ()
                        + "\t" + rp.getX() + "\t" + rp.getY() + "\t" + rp.getZ()
                        + "\t" + minDist + "\t" + maxDist + "\t" + bits(minChance) + "\t" + bits(maxChance)
                        + "\t" + dist + "\t" + bits(thr));
                }
            }
        }

        // ── AxisAlignedLinearPosTest rows ─────────────────────────────────────
        for (Direction.Axis axis : axes) {
            for (float[] ch : chances) {
                for (int[] rg : ranges) {
                    AxisAlignedLinearPosTest rule = new AxisAlignedLinearPosTest(ch[0], ch[1], rg[0], rg[1], axis);
                    int minDist = (Integer) field(rule, "minDist");
                    int maxDist = (Integer) field(rule, "maxDist");
                    float minChance = (Float) field(rule, "minChance");
                    float maxChance = (Float) field(rule, "maxChance");
                    int axisOrd = axis.ordinal();
                    for (int pi = 0; pi < worldPts.length; pi++) {
                        BlockPos wp = new BlockPos(worldPts[pi][0], worldPts[pi][1], worldPts[pi][2]);
                        BlockPos rp = new BlockPos(refPts[pi][0], refPts[pi][1], refPts[pi][2]);
                        int dist = axisDist(axis, wp, rp);
                        float thr = threshold(dist, minDist, maxDist, minChance, maxChance);
                        OUT.println("AXIS\t" + axisOrd
                            + "\t" + wp.getX() + "\t" + wp.getY() + "\t" + wp.getZ()
                            + "\t" + rp.getX() + "\t" + rp.getY() + "\t" + rp.getZ()
                            + "\t" + minDist + "\t" + maxDist + "\t" + bits(minChance) + "\t" + bits(maxChance)
                            + "\t" + dist + "\t" + bits(thr));
                    }
                }
            }
        }

        OUT.flush();
    }
}

import java.lang.reflect.Method;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.block.TargetBlock;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;

// Ground-truth emitter for the pure redstone-strength helper of
//   net.minecraft.world.level.block.TargetBlock (Minecraft 26.1.2):
//   private static int getRedstoneStrength(BlockHitResult, Vec3)   TargetBlock.java:61-77
//
// It drives the REAL class via reflection (the method is private static and
// reads ONLY the hit Direction's axis plus the three Vec3 coordinates — no
// world/registry coupling). Emits tab-separated rows consumed by
// TargetBlockRedstoneParityTest.cpp.
//
// Direction ordinals (Direction.java:33-38): DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5.
// Doubles emitted as raw long bits via Double.doubleToRawLongBits; ints decimal.
//
// TAG:
//   STR  <dirOrd> <xbits> <ybits> <zbits> <getRedstoneStrength>
public class TargetBlockRedstoneParity {
    static final java.io.PrintStream O = System.out;

    static String hx(double d) { return String.format("%016x", Double.doubleToRawLongBits(d)); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Reach the private static helper directly so the math under test is the
        // REAL bytecode, not a re-implementation.
        Method m = TargetBlock.class.getDeclaredMethod(
            "getRedstoneStrength", BlockHitResult.class, Vec3.class);
        m.setAccessible(true);

        // Only the direction's AXIS matters, but exercise all six faces so the
        // axis-selection branch (Y -> max(distX,distZ); Z -> max(distX,distY);
        // X -> max(distY,distZ)) is fully covered.
        Direction[] dirs = Direction.values();

        // Representative coordinate battery. Includes:
        //  * exact face centres (.5 frac) -> distance 0 -> full strength 15
        //  * exact integer coords (frac 0) -> distance 0.5 -> strength 1
        //  * near-edges, off-centre offsets, and negatives (Mth.frac trap)
        //  * large-magnitude integers + fractional parts (long-floor trap)
        //  * values that drive the clamp/ceil boundaries
        double[] coords = {
            0.0, -0.0, 0.5, 1.0, -1.0, 0.25, 0.75, -0.25, -0.75,
            0.0625, 0.9375, 0.4999999, 0.5000001, 0.1, 0.2, 0.3, 0.7, 0.9,
            1.5, 2.5, -2.5, 7.5, -7.5, 16.5, -16.5,
            123.0, 123.5, 123.4321, -123.4321, 1000000.5, -1000000.5,
            0.4666666, 0.5333333, 0.03333333, 0.96666667,
            -0.0001, 0.0001, -0.4999, 0.4999,
            // values whose frac sits just inside/outside the strength brackets
            0.4666667, 0.5333333, 0.43333333, 0.56666667,
            // big magnitudes to stress (long)floor in Mth.frac
            1.0e9 + 0.5, -1.0e9 - 0.5, 2.0e9 + 0.25, -2.0e9 - 0.25
        };

        // Full cross-product over the curated coordinate set for every face would
        // be enormous; instead drive a dense, representative battery: for each
        // face, sweep each coordinate as the "interesting" axis while holding the
        // others at a few fixed offsets, plus a block of fully-varied triples.
        double[] holds = { 0.5, 0.0, 0.25, 0.4666666 };

        for (Direction d : dirs) {
            // (a) sweep x with held y,z
            for (double x : coords)
                for (double h : holds)
                    emit(m, d, x, h, h);
            // (b) sweep y with held x,z
            for (double y : coords)
                for (double h : holds)
                    emit(m, d, h, y, h);
            // (c) sweep z with held x,y
            for (double z : coords)
                for (double h : holds)
                    emit(m, d, h, h, z);
        }

        // (d) a block of fully-varied triples (axis interactions / max() picks)
        double[][] triples = {
            {0.5, 0.5, 0.5}, {0.0, 0.0, 0.0}, {0.5, 0.0, 0.0}, {0.0, 0.5, 0.0},
            {0.0, 0.0, 0.5}, {0.25, 0.75, 0.5}, {0.1, 0.9, 0.5}, {0.4, 0.6, 0.45},
            {0.46, 0.54, 0.5}, {0.43, 0.57, 0.5}, {123.4, -56.6, 7.5},
            {-0.25, 0.25, 0.5}, {0.3333, 0.6667, 0.5}, {0.4999, 0.5001, 0.5},
            {1000000.4, -999999.6, 0.5}, {0.0625, 0.9375, 0.5},
            {0.48, 0.52, 0.5}, {0.45, 0.55, 0.49}, {0.40, 0.60, 0.5}
        };
        for (Direction d : dirs)
            for (double[] t : triples)
                emit(m, d, t[0], t[1], t[2]);
    }

    static void emit(Method m, Direction d, double x, double y, double z) throws Exception {
        // getRedstoneStrength reads ONLY hitResult.getDirection() and the Vec3.
        // Build a BlockHitResult carrying the face direction (pos/inside are
        // irrelevant to the helper) and a Vec3 carrying the hit location.
        BlockHitResult hit = new BlockHitResult(new Vec3(x, y, z), d, BlockPos.ZERO, false);
        Vec3 loc = new Vec3(x, y, z);
        int r = (int) m.invoke(null, hit, loc);
        O.println("STR\t" + d.ordinal() + "\t" + hx(x) + "\t" + hx(y) + "\t" + hx(z) + "\t" + r);
    }
}

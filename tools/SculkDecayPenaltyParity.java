// Ground truth for the pure sculk-charge decay-penalty arithmetic of
// net.minecraft.world.level.block.SculkBlock (Minecraft Java Edition 26.1.2).
//
//   SculkBlock.getDecayPenalty(SculkSpreader, BlockPos, BlockPos, int)
//                                                          SculkBlock.java:60-66
//
// getDecayPenalty is `private static` and PURE — it depends only on the
// spreader's int noGrowthRadius, two BlockPos and an int charge, and computes
// the per-tick charge loss a decaying sculk cursor incurs. We DO NOT replicate
// its body here: we resolve the REAL method by reflection and INVOKE it, so the
// expected values come straight from the shipped bytecode (the float sqrt, the
// float/int division, the (int)(float) narrowing and the Math.max(1,..) floor
// are all executed by the real JVM).
//
// The `spreader` argument is only a carrier for noGrowthRadius(); we build a
// real SculkSpreader via its public constructor with the desired noGrowthRadius
// (the other ctor args do not influence getDecayPenalty). BlockPos uses its
// public (int,int,int) constructor.
//
// SharedConstants/Bootstrap are booted so the registry/tag statics SculkSpreader
// touches at construction are initialised exactly as in-game.
//
// Output (tab-separated). Each row carries the four int inputs plus the expected
// penalty as a decimal int; the C++ side recomputes getDecayPenalty and compares
// the EXACT integer (no tolerance):
//   DECAY  <noGrowthRadius>  <dxFromOrigin>  <dyFromOrigin>  <dzFromOrigin>  <charge>  <penalty>
// (origin is fixed at a non-zero anchor; pos = origin + (dx,dy,dz).)
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

public class SculkDecayPenaltyParity {
    static final java.io.PrintStream O = System.out;

    // REAL classes / members, resolved reflectively.
    static Method GET_DECAY_PENALTY;        // SculkBlock.getDecayPenalty(SculkSpreader,BlockPos,BlockPos,int)
    static Constructor<?> SPREADER_CTOR;    // SculkSpreader(boolean,TagKey,int,int,int,int)
    static Constructor<?> BLOCKPOS_CTOR;    // BlockPos(int,int,int)
    static Object REPLACEABLE_TAG;          // a TagKey<Block> for the spreader ctor (unused by the math)

    static int realGetDecayPenalty(int noGrowthRadius, Object pos, Object originPos, int charge) throws Exception {
        // SculkSpreader(isWorldGeneration, replaceableBlocks, growthSpawnCost,
        //               noGrowthRadius, chargeDecayRate, additionalDecayRate)
        // Only noGrowthRadius is read by getDecayPenalty; the rest are harmless.
        Object spreader = SPREADER_CTOR.newInstance(false, REPLACEABLE_TAG, 10, noGrowthRadius, 10, 5);
        return (Integer) GET_DECAY_PENALTY.invoke(null, spreader, pos, originPos, charge);
    }

    static Object blockPos(int x, int y, int z) throws Exception {
        return BLOCKPOS_CTOR.newInstance(x, y, z);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> sculkBlock = Class.forName("net.minecraft.world.level.block.SculkBlock");
        Class<?> spreaderCls = Class.forName("net.minecraft.world.level.block.SculkSpreader");
        Class<?> blockPosCls = Class.forName("net.minecraft.core.BlockPos");
        Class<?> tagKeyCls = Class.forName("net.minecraft.tags.TagKey");

        GET_DECAY_PENALTY = sculkBlock.getDeclaredMethod(
            "getDecayPenalty", spreaderCls, blockPosCls, blockPosCls, int.class);
        GET_DECAY_PENALTY.setAccessible(true);

        SPREADER_CTOR = spreaderCls.getDeclaredConstructor(
            boolean.class, tagKeyCls, int.class, int.class, int.class, int.class);
        SPREADER_CTOR.setAccessible(true);

        BLOCKPOS_CTOR = blockPosCls.getDeclaredConstructor(int.class, int.class, int.class);
        BLOCKPOS_CTOR.setAccessible(true);

        // Any real Block TagKey works as the (unused-by-math) replaceableBlocks
        // argument; BlockTags.SCULK_REPLACEABLE is exactly what the level spreader
        // uses. Resolved reflectively to avoid a compile-time symbol.
        Class<?> blockTags = Class.forName("net.minecraft.tags.BlockTags");
        REPLACEABLE_TAG = blockTags.getField("SCULK_REPLACEABLE").get(null);

        // Fixed non-zero origin so dx/dy/dz exercise non-trivial int deltas (and
        // so the double distSqr is a real sum of three squared deltas).
        final int ORIGIN_X = 100, ORIGIN_Y = -13, ORIGIN_Z = -250;

        // noGrowthRadius domain: the two real configs are 1 (worldgen) and 4
        // (level); we sweep a band around them. All chosen values keep
        // (24 - noGrowthRadius) != 0 so maxReachSquared != 0 (the game never
        // produces noGrowthRadius == 24).
        int[] radii = { 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 23 };

        // Charge domain: 0..MAX_CHARGE (1000) plus the boundary values. (charge
        // is never < 0 when this branch runs, and is clamped to <= 1000 by
        // addCursors, but we include the full inclusive range.)
        int[] charges = {
            0, 1, 2, 3, 5, 7, 10, 13, 17, 25, 37, 50, 63, 99, 100, 127, 200,
            255, 333, 500, 511, 750, 900, 999, 1000
        };

        // Position deltas from the catalyst origin. distChessboard is bounded by
        // 1024 in the cursor's reasonableness check; the decay factor saturates
        // to 1 once the euclidean distance exceeds noGrowthRadius + (24 -
        // noGrowthRadius) = 24, so we sweep finely from 0 out past 24 and then a
        // few far probes, across all three axes (incl. negatives).
        java.util.ArrayList<int[]> deltas = new java.util.ArrayList<>();
        // Pure-X sweep (covers below/at/just-past the reach radius transition).
        for (int d = 0; d <= 30; d++) deltas.add(new int[]{ d, 0, 0 });
        for (int d = 1; d <= 30; d++) deltas.add(new int[]{ -d, 0, 0 });
        // Pure-Y and pure-Z sweeps (vertical and lateral spread).
        for (int d = 0; d <= 26; d++) deltas.add(new int[]{ 0, d, 0 });
        for (int d = 0; d <= 26; d++) deltas.add(new int[]{ 0, 0, d });
        // Diagonal sweeps so distSqr is a genuine 2- and 3-axis sum (non-integer
        // sqrt -> exercises the float narrowing on irrational distances).
        for (int d = 0; d <= 20; d++) deltas.add(new int[]{ d, d, 0 });
        for (int d = 0; d <= 20; d++) deltas.add(new int[]{ d, d, d });
        for (int d = 1; d <= 18; d++) deltas.add(new int[]{ d, -d, d });
        // Mixed irrational distances around the reach boundary.
        int[][] mixed = {
            {3,4,0}, {5,12,0}, {8,15,0}, {7,24,0}, {1,2,2}, {2,3,6}, {4,4,7},
            {9,12,20}, {6,6,6}, {10,10,10}, {11,11,11}, {12,16,21}, {13,0,0},
            {17,6,8}, {23,1,1}, {24,0,0}, {25,0,0}, {30,30,30}, {100,0,0}
        };
        for (int[] m : mixed) deltas.add(m);

        for (int r : radii) {
            for (int[] dxyz : deltas) {
                Object originPos = blockPos(ORIGIN_X, ORIGIN_Y, ORIGIN_Z);
                Object pos = blockPos(ORIGIN_X + dxyz[0], ORIGIN_Y + dxyz[1], ORIGIN_Z + dxyz[2]);
                for (int charge : charges) {
                    int penalty = realGetDecayPenalty(r, pos, originPos, charge);
                    O.println("DECAY\t" + r + "\t" + dxyz[0] + "\t" + dxyz[1] + "\t" + dxyz[2]
                        + "\t" + charge + "\t" + penalty);
                }
            }
        }
    }
}

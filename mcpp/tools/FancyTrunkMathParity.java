// Reference value generator for the C++ port of the PURE static math helpers of
//   net.minecraft.world.level.levelgen.feature.trunkplacers.FancyTrunkPlacer
// (mcpp/src/world/level/levelgen/feature/trunkplacers/FancyTrunkMath.h).
//
// Drives the REAL decompiled FancyTrunkPlacer from client.jar via reflection so
// the emitted rows are exact ground truth. The methods under test are pure (no
// world / no RandomSource / no registries), so they can be invoked directly:
//
//   private static float treeShape(int height, int y)
//   private        int   getSteps(BlockPos pos)              (uses only |x|,|y|,|z|)
//   private        Axis  getLogAxis(BlockPos start, BlockPos block)
//   private        boolean trimBranches(int height, int localY)
//
// getSteps/getLogAxis/trimBranches are instance methods but read NO instance
// state, so we invoke them on a throwaway FancyTrunkPlacer(1,0,0).
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/FancyTrunkMathParity.java
//   java  -cp <out>;26.1.2/client.jar FancyTrunkMathParity > fancy_trunk_math.tsv
//
// Rows are tab-separated. Floats are raw 32-bit hex (%08x of
// Float.floatToRawIntBits) for bit-for-bit comparison; ints decimal; bool 0/1.
//
//   TS  <height>  <y>           <treeShape_bits>
//   ST  <dx> <dy> <dz>          <getSteps_int>
//   LA  <sx> <sz> <bx> <bz>     <axisOrdinal>           (X=0 Y=1 Z=2)
//   TB  <height> <localY>       <0|1>
//
// O is captured at class load so any bootstrap chatter on stdout stays out of TSV.
public class FancyTrunkMathParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) {
        return String.format("%08x", Float.floatToRawIntBits(v));
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> ftp = Class.forName(
            "net.minecraft.world.level.levelgen.feature.trunkplacers.FancyTrunkPlacer");

        // FancyTrunkPlacer(int baseHeight, int heightRandA, int heightRandB) — public.
        java.lang.reflect.Constructor<?> ctor =
            ftp.getDeclaredConstructor(int.class, int.class, int.class);
        ctor.setAccessible(true);
        Object placer = ctor.newInstance(1, 0, 0);

        java.lang.reflect.Method treeShape =
            ftp.getDeclaredMethod("treeShape", int.class, int.class);
        treeShape.setAccessible(true);

        Class<?> blockPos = Class.forName("net.minecraft.core.BlockPos");
        java.lang.reflect.Constructor<?> bpCtor =
            blockPos.getDeclaredConstructor(int.class, int.class, int.class);
        bpCtor.setAccessible(true);

        java.lang.reflect.Method getSteps =
            ftp.getDeclaredMethod("getSteps", blockPos);
        getSteps.setAccessible(true);

        java.lang.reflect.Method getLogAxis =
            ftp.getDeclaredMethod("getLogAxis", blockPos, blockPos);
        getLogAxis.setAccessible(true);

        java.lang.reflect.Method trimBranches =
            ftp.getDeclaredMethod("trimBranches", int.class, int.class);
        trimBranches.setAccessible(true);

        // ---- treeShape battery -------------------------------------------------
        // height range covers real fancy-oak heights (treeHeight+2, treeHeight~9..25)
        // plus small/large/edge values that stress the float early-out and the
        // sqrt(negative) abs>=radius branch. y from 0..height covers the whole tree.
        int[] tsHeights = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                            16, 18, 20, 24, 27, 30, 40, 50, 100 };
        for (int h : tsHeights) {
            for (int y = -2; y <= h + 2; y++) {
                float r = (Float) treeShape.invoke(null, h, y);
                O.println("TS\t" + h + "\t" + y + "\t" + f(r));
            }
        }

        // ---- getSteps battery (pure |dx|,|dy|,|dz| max) ------------------------
        int[] comps = { Integer.MIN_VALUE, -1000000, -257, -256, -255, -16, -3,
                        -2, -1, 0, 1, 2, 3, 16, 255, 256, 257, 1000000,
                        Integer.MAX_VALUE };
        // getSteps(BlockPos) = max(|x|,|y|,|z|); we feed delta = endPos - startPos.
        // A focused cross-product (not full N^3) keeps the file reasonable while
        // hitting INT_MIN overflow (abs(INT_MIN)==INT_MIN) and ties.
        for (int dx : comps) {
            for (int dy : comps) {
                int dz = -(dx ^ dy); // deterministic third component, varied sign
                Object pos = bpCtor.newInstance(dx, dy, dz);
                int steps = (Integer) getSteps.invoke(placer, pos);
                O.println("ST\t" + dx + "\t" + dy + "\t" + dz + "\t" + steps);
            }
        }

        // ---- getLogAxis battery (ordinal X=0 Y=1 Z=2; tie -> X) ----------------
        int[] axc = { Integer.MIN_VALUE, -100, -2, -1, 0, 1, 2, 100,
                      Integer.MAX_VALUE };
        for (int sx : axc) {
            for (int bx : axc) {
                for (int sz : new int[]{ -3, 0, 1, 5 }) {
                    for (int bz : new int[]{ -5, 0, 1, 3 }) {
                        Object start = bpCtor.newInstance(sx, 0, sz);
                        Object block = bpCtor.newInstance(bx, 0, bz);
                        Object axisEnum = getLogAxis.invoke(placer, start, block);
                        int ordinal = ((Enum<?>) axisEnum).ordinal();
                        O.println("LA\t" + sx + "\t" + sz + "\t" + bx + "\t" + bz
                                  + "\t" + ordinal);
                    }
                }
            }
        }

        // ---- trimBranches battery (int >= height*0.2 double) -------------------
        int[] tbH = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 15, 20, 25, 50, 100, 1000 };
        for (int h : tbH) {
            for (int ly = -3; ly <= h + 1; ly++) {
                boolean t = (Boolean) trimBranches.invoke(placer, h, ly);
                O.println("TB\t" + h + "\t" + ly + "\t" + (t ? 1 : 0));
            }
        }
    }
}

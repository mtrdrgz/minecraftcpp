// Ground-truth generator for the PURE world-height-limit check nested in the
// REAL decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.pools.JigsawPlacement
//     private static boolean isStartTooCloseToWorldHeightLimits(
//         LevelHeightAccessor heightAccessor, DimensionPadding dimensionPadding,
//         BoundingBox centerPieceBb)
//
// The helper is self-contained integer math: NO world writes, NO RandomSource,
// NO registry/datapack. We drive it via reflection (the method is private static)
// over a concrete LevelHeightAccessor (the built-in LevelHeightAccessor.create
// factory), the real DimensionPadding record (incl. its static ZERO sentinel),
// and real BoundingBox instances. JigsawPlacement's <clinit> can touch the
// registry/loggers, so we Bootstrap first.
//
//   tools/run_groundtruth.ps1 -Tool JigsawHeightLimitParity -Out mcpp/build/jigsaw_height_limit.tsv
//
// Line format (all ints decimal; result is 0/1):
//   CHK <worldMinY> <worldMaxY> <padBottom> <padTop> <isZeroSentinel> <bbMinY> <bbMaxY> <result>
//        -- isStartTooCloseToWorldHeightLimits(create(minY,height), padding, bb)
//           worldMinY/worldMaxY are heightAccessor.getMinY()/getMaxY();
//           isZeroSentinel == 1 iff the *identity* DimensionPadding.ZERO was passed.

import java.lang.reflect.Field;
import java.lang.reflect.Method;

import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.pools.DimensionPadding;

public class JigsawHeightLimitParity {
    static final java.io.PrintStream OUT = System.out;

    static Method M_check;
    static DimensionPadding ZERO;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> jp = Class.forName(
            "net.minecraft.world.level.levelgen.structure.pools.JigsawPlacement");
        M_check = jp.getDeclaredMethod(
            "isStartTooCloseToWorldHeightLimits",
            LevelHeightAccessor.class, DimensionPadding.class, BoundingBox.class);
        M_check.setAccessible(true);

        // DimensionPadding.ZERO — the identity sentinel that short-circuits.
        Field zf = DimensionPadding.class.getField("ZERO");
        ZERO = (DimensionPadding) zf.get(null);

        // Representative world spans: real overworld (-64,384), nether (0,128),
        // a custom tall world, and a tiny/edge one.
        int[][] worlds = {
            {-64, 384},  // overworld: minY=-64, height=384  -> maxY=319
            {0, 256},    // legacy:    minY=0,   height=256   -> maxY=255
            {0, 128},    // nether-ish: minY=0,  height=128   -> maxY=127
            {-32, 100},  // arbitrary
            {16, 1},     // degenerate single-layer world
        };

        // Padding cases. Include the ZERO sentinel (sentinel=true), a value-equal
        // (0,0) that is NOT the sentinel, symmetric, and asymmetric paddings.
        // Each entry: {bottom, top, useZeroSentinel}.
        int[][] pads = {
            {0, 0, 1},   // the real DimensionPadding.ZERO singleton
            {0, 0, 0},   // fresh (0,0): value-equal but NOT == ZERO -> no short-circuit
            {4, 4, 0},   // symmetric
            {1, 7, 0},   // asymmetric
            {10, 0, 0},  // bottom-only
            {0, 10, 0},  // top-only
            {64, 64, 0}, // large symmetric
        };

        for (int[] w : worlds) {
            LevelHeightAccessor acc = LevelHeightAccessor.create(w[0], w[1]);
            int worldMinY = acc.getMinY();
            int worldMaxY = acc.getMaxY();

            // Box minY/maxY chosen relative to each world so we straddle the
            // padded bounds: well-inside, flush-with-padding (the strict-compare
            // boundary), one-over each edge, and fully outside.
            int[][] boxes = {
                {worldMinY,          worldMaxY},               // flush with raw world bounds
                {worldMinY + 5,      worldMaxY - 5},           // a touch inside
                {worldMinY + 64,     worldMaxY - 64},          // well inside
                {worldMinY - 1,      worldMaxY},               // below world bottom
                {worldMinY,          worldMaxY + 1},           // above world top
                {worldMinY + 1,      worldMinY + 1},           // tiny, near floor
                {worldMaxY - 1,      worldMaxY - 1},           // tiny, near ceiling
                {(worldMinY + worldMaxY) / 2, (worldMinY + worldMaxY) / 2}, // mid
            };

            for (int[] p : pads) {
                DimensionPadding pad = (p[2] == 1) ? ZERO : new DimensionPadding(p[0], p[1]);
                for (int[] b : boxes) {
                    // BoundingBox spanning the same X/Z but the box's Y range;
                    // only minY()/maxY() are read by the helper.
                    BoundingBox bb = new BoundingBox(0, b[0], 0, 1, b[1], 1);
                    boolean res = (Boolean) M_check.invoke(null, acc, pad, bb);
                    OUT.println("CHK\t" + worldMinY + "\t" + worldMaxY
                        + "\t" + pad.bottom() + "\t" + pad.top()
                        + "\t" + (p[2] == 1 ? 1 : 0)
                        + "\t" + bb.minY() + "\t" + bb.maxY()
                        + "\t" + (res ? 1 : 0));
                }
            }
        }

        OUT.flush();
    }
}

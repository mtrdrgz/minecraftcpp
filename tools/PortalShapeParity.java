// Ground-truth generator for net.minecraft.world.level.portal.PortalShape's pure
// static math: getRelativePosition(BlockUtil.FoundRectangle, Direction.Axis, Vec3,
// EntityDimensions) — the relative-coordinate mapping used to carry an entity to
// the same spot in the destination portal across an overworld<->nether teleport.
//
// Drives the REAL class via reflection (the method is public static, but reflection
// keeps us free of any compile-time coupling). Builds genuine BlockUtil.FoundRectangle
// / BlockPos / Vec3 / EntityDimensions instances; no Level / entity / registry is
// touched. Emits one TSV row per case, every double as 16-hex of doubleToRawLongBits
// and every float input as 8-hex of floatToRawIntBits, so the C++ port (PortalShape.h)
// can reproduce the result bit-for-bit.
//
// Row: REL <axisOrdinal> <minX> <minY> <minZ> <axis1Size> <axis2Size> \
//          <posX> <posY> <posZ> <width> <height> <relRight> <relUp> <relForward>
//   axisOrdinal: Direction.Axis ordinal of the portal axis (X=0, Y=1, Z=2)
//   minX/minY/minZ/axis1Size/axis2Size: decimal ints (FoundRectangle)
//   posX/posY/posZ: 16-hex doubles (entity position)
//   width/height:   8-hex floats  (entity dimensions)
//   rel*:           16-hex doubles (the returned Vec3 components)
import java.lang.reflect.Method;

import net.minecraft.SharedConstants;
import net.minecraft.server.Bootstrap;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.BlockUtil;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.phys.Vec3;

public class PortalShapeParity {
    static final java.io.PrintStream O = System.out;

    static String hd(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }
    static String hf(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static Method GET_RELATIVE; // PortalShape.getRelativePosition(...)

    static {
        try {
            Class<?> ps = Class.forName("net.minecraft.world.level.portal.PortalShape");
            for (Method m : ps.getDeclaredMethods()) {
                if (m.getName().equals("getRelativePosition")) { GET_RELATIVE = m; break; }
            }
            if (GET_RELATIVE == null) throw new IllegalStateException("getRelativePosition not found");
            GET_RELATIVE.setAccessible(true);
        } catch (Throwable t) { throw new RuntimeException(t); }
    }

    // Entity-dimension footprints to exercise (width, height). Includes vanilla
    // sizes (player 0.6x1.8, slime/zombie/horse...) and fractions that stress the
    // float-widen-before-subtract trap, plus the zero size (width<=0 / height<=0).
    static final float[][] DIMS = {
        {0.6f, 1.8f},   // player
        {0.6f, 0.6f},   // crouched-ish / small
        {1.0f, 1.0f},
        {2.0f, 2.0f},
        {1.4f, 1.6f},   // horse-ish
        {0.98f, 0.7f},  // bee-ish
        {0.25f, 0.25f}, // tiny slime
        {0.0f, 0.0f},   // degenerate -> both relative axes take the else branch
        {3.0f, 3.0f},   // larger than some rectangles -> width/height may go negative
        {0.3f, 1.95f},  // sneaking player-ish
    };

    // Portal rectangle widths/heights to scan (axis1Size = portal interior width,
    // axis2Size = interior height). 0 forces a degenerate rectangle.
    static final int[][] RECTS = {
        {0, 0}, {1, 1}, {2, 3}, {3, 4}, {4, 5}, {1, 21}, {21, 21}, {5, 1}, {2, 2}, {1, 3},
    };

    // minCorner candidates (covers negative, zero, positive, and large coords so
    // the int->double widen in get(axis) and the +0.5 grouping are both exercised).
    static final int[][] MINS = {
        {0, 0, 0}, {-5, 64, -5}, {100, -32, 7}, {-128, 0, 256}, {3, 70, -9},
    };

    // Entity positions relative to a few placements; spans inside/outside the
    // rectangle so Mth.clamp's [0,1] saturation on both ends is hit, plus exact
    // boundary and centre values.
    static final double[][] POS = {
        {0.5, 64.0, 0.5}, {-4.2, 65.3, -4.9}, {101.5, -30.0, 8.5},
        {-127.0, 0.0, 257.0}, {3.5, 71.25, -8.5}, {0.0, 0.0, 0.0},
        {2.5, 66.0, 2.5}, {-0.0001, 64.9999, 0.0001}, {7.0, 100.0, -2.0},
        {3.25, 70.5, -9.75},
    };

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Portal axes are the horizontal ones (X and Z); Y is the height axis used
        // internally. We drive both X and Z to verify the forwardAxis swap.
        Direction.Axis[] axes = { Direction.Axis.X, Direction.Axis.Z };

        long n = 0;
        for (Direction.Axis axis : axes) {
            for (int[] mn : MINS) {
                for (int[] r : RECTS) {
                    BlockUtil.FoundRectangle rect =
                        new BlockUtil.FoundRectangle(new BlockPos(mn[0], mn[1], mn[2]), r[0], r[1]);
                    for (float[] d : DIMS) {
                        EntityDimensions dim = EntityDimensions.scalable(d[0], d[1]);
                        for (double[] p : POS) {
                            Vec3 pos = new Vec3(p[0], p[1], p[2]);
                            Vec3 out = (Vec3) GET_RELATIVE.invoke(null, rect, axis, pos, dim);
                            O.println("REL\t" + axis.ordinal()
                                + "\t" + mn[0] + "\t" + mn[1] + "\t" + mn[2]
                                + "\t" + r[0] + "\t" + r[1]
                                + "\t" + hd(p[0]) + "\t" + hd(p[1]) + "\t" + hd(p[2])
                                + "\t" + hf(dim.width()) + "\t" + hf(dim.height())
                                + "\t" + hd(out.x) + "\t" + hd(out.y) + "\t" + hd(out.z));
                            n++;
                        }
                    }
                }
            }
        }
        // NOTE: do NOT print a summary to System.out/err here — after Bootstrap.bootStrap()
        // Minecraft installs a logger that reroutes System.err back onto System.out, which
        // would contaminate the TSV. The row count is reported by run_groundtruth.ps1.
    }
}

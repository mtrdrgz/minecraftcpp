// WorldBorderParity — ground-truth emitter for the PURE geometry/lerp math of the
// Minecraft 26.1.2 world border (net.minecraft.world.level.border.WorldBorder and
// its private StaticBorderExtent / MovingBorderExtent).
//
// We drive the REAL WorldBorder class through its public API only — the default
// constructor just stores the default Settings (no world/registry coupling; the
// SavedData base is a single boolean dirty flag), and setCenter / setSize /
// setAbsoluteMaxSize / lerpSizeBetween / tick build and advance the real private
// extents. We then read back the public getters (getMinX/getMaxX/getMinZ/getMaxZ
// with a partial-tick delta, getSize/getLerpSpeed/getLerpTime/getLerpTarget/
// getStatus, getDistanceToBorder, isWithinBounds, clampVec3ToBound). No reflection
// into private extents is needed: replaying the identical (construct + N tick)
// sequence in C++ reproduces the same StaticBorderExtent/MovingBorderExtent state.
//
// Float = %08x(Float.floatToRawIntBits); double = %016x(Double.doubleToRawLongBits);
// long = decimal; int = decimal; status = ordinal name.
//
// TAGs (tab-separated):
//   STATIC  cx cz absMax size  delta x z margin
//           -> minX maxX minZ maxZ getSize getLerpSpeed getLerpTime(long)
//              getLerpTarget statusName getDistanceToBorder(x,z) within(x,z,margin)
//              clampX(x) clampZ(z)
//   MOVING  cx cz absMax from to duration gameTime ticks  delta x z margin
//           -> (same 14-field payload, read AFTER `ticks` calls to tick())
//
//   java WorldBorderParity > world_border.tsv

import java.io.PrintStream;
import net.minecraft.world.level.border.WorldBorder;
import net.minecraft.world.phys.Vec3;

public class WorldBorderParity {
    static final PrintStream O = System.out;

    static String fx(float f) { return String.format("%08x", Float.floatToRawIntBits(f)); }
    static String dx(double d) { return String.format("%016x", Double.doubleToRawLongBits(d)); }

    // Emit the shared 14-field read-back payload for a configured border, at the
    // given partial-tick delta and probe point (x,z,margin).
    static String payload(WorldBorder wb, float delta, double x, double z, double margin) {
        double minX = wb.getMinX(delta);
        double maxX = wb.getMaxX(delta);
        double minZ = wb.getMinZ(delta);
        double maxZ = wb.getMaxZ(delta);
        double size = wb.getSize();
        double lerpSpeed = wb.getLerpSpeed();
        long lerpTime = wb.getLerpTime();
        double lerpTarget = wb.getLerpTarget();
        String status = wb.getStatus().name();
        double dist = wb.getDistanceToBorder(x, z);
        boolean within = wb.isWithinBounds(x, z, margin);
        Vec3 clamped = wb.clampVec3ToBound(x, 0.0, z);
        return dx(minX) + "\t" + dx(maxX) + "\t" + dx(minZ) + "\t" + dx(maxZ)
             + "\t" + dx(size) + "\t" + dx(lerpSpeed) + "\t" + lerpTime
             + "\t" + dx(lerpTarget) + "\t" + status
             + "\t" + dx(dist) + "\t" + (within ? 1 : 0)
             + "\t" + dx(clamped.x) + "\t" + dx(clamped.z);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Representative battery. Centers include 0, off-center, and values that
        // push the clamped edges past +/-absoluteMaxSize. Sizes include the vanilla
        // default (5.999997E7F float literal), small, odd, and over-max sizes.
        double[] centers = { 0.0, 100.5, -250.25, 1.0E7, -2.9999984E7, 2.9999984E7, 12345.678 };
        double[] sizes = { 0.0, 1.0, 2.0, 16.0, 60.0, 1000.0, 5.999997E7F, 1.0E8, 0.5, 123.456 };
        int[] absMaxes = { 29999984, 1000, 100, 50000000 };
        // partial-tick deltas in [0,1] (renderer passes deltaPartialTick).
        float[] deltas = { 0.0f, 0.25f, 0.5f, 0.6666667f, 1.0f, 0.123f };
        // probe points: inside, on edges, outside, off in one axis.
        double[][] probes = {
            { 0.0, 0.0 }, { 8.0, -8.0 }, { 100.0, 100.0 }, { -30.0, 7.5 },
            { 1.0E7, -1.0E7 }, { 30000000.0, 0.0 }, { 12.34, 56.78 }
        };
        double[] margins = { 0.0, 1.0, 5.0, -2.0 };

        // --- STATIC scenarios -------------------------------------------------
        for (double cx : centers) {
            for (double cz : centers) {
                for (int absMax : absMaxes) {
                    for (double size : sizes) {
                        WorldBorder wb = new WorldBorder();
                        wb.setAbsoluteMaxSize(absMax);
                        wb.setCenter(cx, cz);
                        wb.setSize(size);
                        for (float delta : deltas) {
                            for (double[] p : probes) {
                                for (double margin : margins) {
                                    O.println("STATIC\t" + dx(cx) + "\t" + dx(cz) + "\t" + absMax
                                            + "\t" + dx(size) + "\t" + fx(delta)
                                            + "\t" + dx(p[0]) + "\t" + dx(p[1]) + "\t" + dx(margin)
                                            + "\t" + payload(wb, delta, p[0], p[1], margin));
                                }
                            }
                        }
                    }
                }
            }
        }

        // --- MOVING scenarios -------------------------------------------------
        // lerpSizeBetween(from, to, ticks, gameTime). from != to (== drives a real
        // MovingBorderExtent; from == to collapses to a StaticBorderExtent in vanilla,
        // which we exercise separately under STATIC). Then advance `ticks` calls of
        // tick() so previousSize != size and the lerp progresses.
        double[][] fromTo = {
            { 100.0, 50.0 },     // shrinking
            { 50.0, 100.0 },     // growing
            { 1000.0, 16.0 },    // big shrink
            { 16.0, 1000.0 },    // big grow
            { 60.0, 60.0001 },   // tiny grow
            { 0.5, 2.5 },        // small
            { 5.999997E7F, 1.0E7 } // from default size
        };
        long[] durations = { 1L, 2L, 5L, 20L, 100L, 1200L };
        long[] gameTimes = { 0L, 1000L, 123456L };
        int[] tickCounts = { 0, 1, 2, 3 };
        // a leaner probe/delta set for the (much larger) moving cross-product.
        float[] mDeltas = { 0.0f, 0.5f, 1.0f, 0.25f };
        double[][] mProbes = { { 0.0, 0.0 }, { 25.0, -25.0 }, { 600.0, 600.0 }, { 12.34, 56.78 } };
        double[] mMargins = { 0.0, 2.0 };
        int[] mAbsMaxes = { 29999984, 100 };

        for (double[] ft : fromTo) {
            for (long dur : durations) {
                for (long gt : gameTimes) {
                    for (int absMax : mAbsMaxes) {
                        for (int ticks : tickCounts) {
                            // duration must exceed the number of ticks we apply, else
                            // vanilla swaps in a StaticBorderExtent(to) mid-way (which
                            // the C++ replay also models via update()'s finished flag);
                            // we keep ticks < dur here to read the live moving extent.
                            if (ticks >= dur) continue;
                            WorldBorder wb = new WorldBorder();
                            wb.setAbsoluteMaxSize(absMax);
                            wb.setCenter(ft[0] * 0.0 + 7.5, -3.25); // fixed off-center
                            wb.lerpSizeBetween(ft[0], ft[1], dur, gt);
                            for (int i = 0; i < ticks; i++) wb.tick();
                            for (float delta : mDeltas) {
                                for (double[] p : mProbes) {
                                    for (double margin : mMargins) {
                                        O.println("MOVING\t" + dx(7.5) + "\t" + dx(-3.25)
                                                + "\t" + absMax
                                                + "\t" + dx(ft[0]) + "\t" + dx(ft[1])
                                                + "\t" + dur + "\t" + gt + "\t" + ticks
                                                + "\t" + fx(delta)
                                                + "\t" + dx(p[0]) + "\t" + dx(p[1]) + "\t" + dx(margin)
                                                + "\t" + payload(wb, delta, p[0], p[1], margin));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        O.flush();
    }
}

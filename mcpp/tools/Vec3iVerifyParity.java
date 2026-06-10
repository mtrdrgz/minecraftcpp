// Ground-truth generator that VERIFIES the existing C++ port of
// net.minecraft.core.Vec3i (Minecraft 26.1.2) against the REAL decompiled class,
// covering the surface called out by the assignment that was NOT yet gated:
//   compareTo(Vec3i)                       Vec3i.java:57-63
//   getX/getY/getZ                         Vec3i.java:65-75
//   distSqr(Vec3i)                         Vec3i.java:201-203 (-> distToLowCornerSqr)
//   distManhattan(Vec3i)                   Vec3i.java:223-228 (int->float->(int))
//   distChessboard(Vec3i)                  Vec3i.java:230-235
//   closerThan(Vec3i,double)               Vec3i.java:193-195 (distSqr < Mth.square(d))
//   cross(Vec3i)                           Vec3i.java:185-191
//   below()/below(steps)                   Vec3i.java:124-130 (-> relative(DOWN,..))
//   above()/above(steps)                   Vec3i.java:116-122 (-> relative(UP,..))
//   relative(Direction[,steps])            Vec3i.java:164-172
//
//   tools/run_groundtruth.ps1 -Tool Vec3iVerifyParity -Out mcpp/build/vec3i_verify.tsv
//
// All exercised methods are public; no reflection needed. Vec3i.<clinit> is
// harmless (ZERO + codecs), but we still bootstrap cheaply to be robust against
// any static-init dependency, exactly like the BlockPos verifier. O is captured
// before Bootstrap so the TSV on stdout stays clean even if logging reconfigures
// System.out.
//
// distSqr is a double -> emitted as raw IEEE-754 bits (%016x) for bit-exact
// compare. closerThan's distance argument is a double; we feed it back as raw
// bits so the C++ side reconstructs the EXACT double. Everything else is
// int/long/bool decimal.

import net.minecraft.core.Direction;
import net.minecraft.core.Vec3i;

public class Vec3iVerifyParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // FINITE/PHYSICAL int battery: zeros, units, signs, small spreads, the world
    // border (30000000), build/section boundaries, and overflow-prone extremes
    // (INT_MIN/MAX, 2^30) so cross()/distChessboard()/distManhattan() exercise
    // two's-complement wrap + the (int)(float) narrow.
    static final int[][] V = {
        {0, 0, 0}, {1, 2, 3}, {-1, -2, -3}, {7, -8, 9}, {-15, 16, -17},
        {100, -200, 300}, {255, 256, -256}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
        {16, 16, 16}, {-16, -16, -16}, {64, -64, 320}, {-320, 384, -384},
        {30000000, -30000000, 12345}, {-12345, 67890, -67890},
        {33554431, 2047, -33554432}, {-33554432, -2048, 33554431},
        {2147483647, 2147483647, 2147483647}, {-2147483648, -2147483648, -2147483648},
        {2147483647, -2147483648, 0}, {1073741824, 1073741824, 1073741824},
        {65535, -65536, 65536}, {3, 0, -4}, {1000000, 1000000, 1000000}
    };

    // Step magnitudes for relative/above/below (incl. 0 short-circuit + overflow).
    static final int[] STEPS = { 0, 1, -1, 2, -2, 3, 7, 15, -15, 16, -16, 100, -100, 65535, -65536, 2147483647, -2147483648 };

    // Finite/physical distances for closerThan: straddle exact integer dist^2
    // boundaries, plus 0 and large radii.
    static final double[] DISTS = {
        0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 3.7416573867739413, 5.0, 10.0, 100.0,
        1.7320508075688772, 2.449489742783178, 1.4142135623730951, 0.9999999, 1.0000001, 1000000.0
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ── getX/getY/getZ ───────────────────────────────────────────────────
        for (int[] a : V) {
            Vec3i p = new Vec3i(a[0], a[1], a[2]);
            O.println("V3_GET\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                + "\t" + p.getX() + "\t" + p.getY() + "\t" + p.getZ());
        }

        // ── above/below (0-arg + steps) -> relative(UP/DOWN, steps) ──────────
        for (int[] a : V) {
            Vec3i p = new Vec3i(a[0], a[1], a[2]);
            emit3("V3_ABOVE0", a, p.above());
            emit3("V3_BELOW0", a, p.below());
            for (int s : STEPS) {
                emitS("V3_ABOVE", a, s, p.above(s));
                emitS("V3_BELOW", a, s, p.below(s));
            }
        }

        // ── relative(Direction) one-arg + relative(Direction, steps) ─────────
        for (int[] a : V) {
            Vec3i p = new Vec3i(a[0], a[1], a[2]);
            for (Direction dir : Direction.values()) {
                Vec3i r1 = p.relative(dir);
                O.println("V3_RELDIR1\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + dir.get3DDataValue()
                    + "\t" + r1.getX() + "\t" + r1.getY() + "\t" + r1.getZ());
                for (int s : STEPS) {
                    Vec3i r = p.relative(dir, s);
                    O.println("V3_RELDIR\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                        + "\t" + dir.get3DDataValue() + "\t" + s
                        + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
                }
            }
        }

        // ── compareTo / cross / distSqr / distManhattan / distChessboard ─────
        //    over the cartesian product of the battery.
        for (int[] a : V) {
            Vec3i p = new Vec3i(a[0], a[1], a[2]);
            for (int[] b : V) {
                Vec3i q = new Vec3i(b[0], b[1], b[2]);

                O.println("V3_CMP\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + p.compareTo(q));

                Vec3i c = p.cross(q);
                O.println("V3_CROSS\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + c.getX() + "\t" + c.getY() + "\t" + c.getZ());

                O.println("V3_DISTSQR\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + d(p.distSqr(q)));

                O.println("V3_DISTMAN\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + p.distManhattan(q));

                O.println("V3_DISTCHESS\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + p.distChessboard(q));
            }
        }

        // ── closerThan(Vec3i, double) over a focused finite subset x DISTS ───
        // (cartesian over full V x V x DISTS would explode; use near pairs that
        // straddle the radius boundaries plus extremes.)
        int[][] NEAR = {
            {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {2, 0, 0}, {2, 2, 1},
            {3, 0, 0}, {0, 3, 0}, {3, 3, 3}, {-2, 1, -2}, {5, 0, 0}, {10, 10, 10},
            {100, -100, 50}, {30000000, -30000000, 0}
        };
        for (int[] a : NEAR) {
            Vec3i p = new Vec3i(a[0], a[1], a[2]);
            for (int[] b : NEAR) {
                Vec3i q = new Vec3i(b[0], b[1], b[2]);
                for (double dist : DISTS) {
                    O.println("V3_CLOSER\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                        + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                        + "\t" + d(dist)
                        + "\t" + (p.closerThan(q, dist) ? 1 : 0));
                }
            }
        }
    }

    static void emit3(String tag, int[] a, Vec3i r) {
        O.println(tag + "\t" + a[0] + "\t" + a[1] + "\t" + a[2]
            + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
    }
    static void emitS(String tag, int[] a, int s, Vec3i r) {
        O.println(tag + "\t" + a[0] + "\t" + a[1] + "\t" + a[2] + "\t" + s
            + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
    }
}

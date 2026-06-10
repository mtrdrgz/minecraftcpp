// Ground-truth generator for net.minecraft.core.Vec3i (Minecraft 26.1.2) using
// the REAL decompiled class. Pure integer vector; distSqr/distToCenterSqr/
// distToLowCornerSqr return double, distManhattan/distChessboard/get return int.
//
//   tools/run_groundtruth.ps1 -Tool Vec3iParity -Out mcpp/build/vec3i.tsv
//
// TSV row format (tab-separated), dispatched by leading TAG in the C++ test.
// Doubles are emitted as %016x raw bits; ints in decimal. No Bootstrap needed
// (Vec3i / Direction enum are pure and do not touch registries).

import net.minecraft.core.Direction;
import net.minecraft.core.Vec3i;

public class Vec3iParity {
    static final java.io.PrintStream O = System.out;

    // Wide int battery: zeros, units, signs, powers of two, large/overflow-prone.
    static final int[] XS = {
        0, 1, -1, 2, -2, 3, 7, 8, 15, 16, -16, 17, 31, 32, -32, 100, -100,
        255, 256, -256, 1000, -1000, 65535, 65536, -65536,
        30000000, -30000000, 1073741824, -1073741824, 2147483647, -2147483648
    };
    static final int[] STEPS = { 0, 1, -1, 2, -2, 3, 7, 16, -16, 100, -100, 65535, -65536, 2147483647, -2147483648 };

    static String hd(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    public static void main(String[] args) throws Exception {
        // A compact-but-thorough set of representative vectors.
        int[][] V = {
            {0, 0, 0}, {1, 2, 3}, {-1, -2, -3}, {7, -8, 9}, {-15, 16, -17},
            {100, -200, 300}, {255, 256, -256}, {30000000, -30000000, 12345},
            {2147483647, 2147483647, 2147483647}, {-2147483648, -2147483648, -2147483648},
            {2147483647, -2147483648, 0}, {1073741824, 1073741824, 1073741824},
            {65535, -65536, 65536}, {3, 0, -4}, {-5, 12, 0}
        };

        for (int[] a : V) {
            Vec3i p = new Vec3i(a[0], a[1], a[2]);

            // getX/getY/getZ
            O.println("GET\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                + "\t" + p.getX() + "\t" + p.getY() + "\t" + p.getZ());

            // get(Direction.Axis)
            O.println("GETAXIS\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                + "\t" + p.get(Direction.Axis.X) + "\t" + p.get(Direction.Axis.Y) + "\t" + p.get(Direction.Axis.Z));

            // multiply(int) over a range of scales
            for (int s : STEPS) {
                Vec3i m = p.multiply(s);
                O.println("MULI\t" + a[0] + "\t" + a[1] + "\t" + a[2] + "\t" + s
                    + "\t" + m.getX() + "\t" + m.getY() + "\t" + m.getZ());
            }

            // above/below/north/south/west/east with step variants
            for (int s : STEPS) {
                emit3("ABOVE", a, s, p.above(s));
                emit3("BELOW", a, s, p.below(s));
                emit3("NORTH", a, s, p.north(s));
                emit3("SOUTH", a, s, p.south(s));
                emit3("WEST",  a, s, p.west(s));
                emit3("EAST",  a, s, p.east(s));
            }
            // zero-arg variants (== step 1)
            emit0("ABOVE0", a, p.above());
            emit0("BELOW0", a, p.below());
            emit0("NORTH0", a, p.north());
            emit0("SOUTH0", a, p.south());
            emit0("WEST0",  a, p.west());
            emit0("EAST0",  a, p.east());

            // relative(Direction,int) for every direction
            for (Direction d : Direction.values())
                for (int s : STEPS) {
                    Vec3i r = p.relative(d, s);
                    O.println("RELDIR\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                        + "\t" + d.get3DDataValue() + "\t" + s
                        + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
                }
            // relative(Direction) one-arg
            for (Direction d : Direction.values()) {
                Vec3i r = p.relative(d);
                O.println("RELDIR1\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + d.get3DDataValue()
                    + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
            }
            // relative(Direction.Axis,int)
            for (Direction.Axis ax : Direction.Axis.values())
                for (int s : STEPS) {
                    Vec3i r = p.relative(ax, s);
                    O.println("RELAXIS\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                        + "\t" + ax.ordinal() + "\t" + s
                        + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
                }
        }

        // Pairwise operations: offset/subtract/cross/distSqr/distManhattan/
        // distChessboard/closerThan over the cartesian product of V.
        for (int[] a : V) {
            Vec3i p = new Vec3i(a[0], a[1], a[2]);
            for (int[] b : V) {
                Vec3i q = new Vec3i(b[0], b[1], b[2]);

                Vec3i off = p.offset(q);
                O.println("OFFV\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + off.getX() + "\t" + off.getY() + "\t" + off.getZ());

                Vec3i sub = p.subtract(q);
                O.println("SUBV\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + sub.getX() + "\t" + sub.getY() + "\t" + sub.getZ());

                Vec3i cr = p.cross(q);
                O.println("CROSS\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + cr.getX() + "\t" + cr.getY() + "\t" + cr.getZ());

                O.println("DISTSQR\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + hd(p.distSqr(q)));

                O.println("DISTMAN\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + p.distManhattan(q));

                O.println("DISTCHESS\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + p.distChessboard(q));
            }
        }

        // offset(int,int,int) explicit, including (0,0,0) short-circuit branch.
        int[][] OFFS = {
            {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, -1, -1},
            {15, -16, 17}, {2147483647, 1, -1}, {-2147483648, -1, 1}, {100, -100, 65536}
        };
        for (int[] a : V) {
            Vec3i p = new Vec3i(a[0], a[1], a[2]);
            for (int[] o : OFFS) {
                Vec3i r = p.offset(o[0], o[1], o[2]);
                O.println("OFFI\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + o[0] + "\t" + o[1] + "\t" + o[2]
                    + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
            }
        }

        // multiply(int,int,int)
        for (int[] a : V) {
            Vec3i p = new Vec3i(a[0], a[1], a[2]);
            for (int[] o : OFFS) {
                Vec3i r = p.multiply(o[0], o[1], o[2]);
                O.println("MUL3\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + o[0] + "\t" + o[1] + "\t" + o[2]
                    + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
            }
        }

        // Double-corner distances over a battery of double sample points.
        double[][] PTS = {
            {0.0, 0.0, 0.0}, {0.5, 0.5, 0.5}, {1.25, -2.5, 3.75},
            {100.1, -200.2, 300.3}, {-0.5, -0.5, -0.5},
            {1e9, -1e9, 1234.5}, {2147483647.0, -2147483648.0, 0.0},
            {0.49999999999, 0.5, 0.50000000001}
        };
        for (int[] a : V) {
            Vec3i p = new Vec3i(a[0], a[1], a[2]);
            for (double[] d : PTS) {
                O.println("DLOW\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + hd(d[0]) + "\t" + hd(d[1]) + "\t" + hd(d[2])
                    + "\t" + hd(p.distToLowCornerSqr(d[0], d[1], d[2])));
                O.println("DCENTER\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + hd(d[0]) + "\t" + hd(d[1]) + "\t" + hd(d[2])
                    + "\t" + hd(p.distToCenterSqr(d[0], d[1], d[2])));
            }
        }

        // closerThan(Vec3i,double) — booleans (1/0) over distance thresholds.
        double[] DISTS = { 0.0, 0.5, 1.0, 1.5, 2.0, 5.0, 10.0, 100.0, 1e9, -1.0 };
        for (int[] a : V) {
            Vec3i p = new Vec3i(a[0], a[1], a[2]);
            for (int[] b : V) {
                Vec3i q = new Vec3i(b[0], b[1], b[2]);
                for (double dist : DISTS) {
                    boolean r = p.closerThan(q, dist);
                    O.println("CLOSER\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                        + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                        + "\t" + hd(dist) + "\t" + (r ? 1 : 0));
                }
            }
        }
    }

    static void emit3(String tag, int[] a, int s, Vec3i r) {
        O.println(tag + "\t" + a[0] + "\t" + a[1] + "\t" + a[2] + "\t" + s
            + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
    }
    static void emit0(String tag, int[] a, Vec3i r) {
        O.println(tag + "\t" + a[0] + "\t" + a[1] + "\t" + a[2]
            + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
    }
}

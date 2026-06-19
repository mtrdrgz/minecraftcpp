// Ground-truth generator that VERIFIES the existing C++ ports of
// net.minecraft.core.BlockPos (Minecraft 26.1.2) against the REAL decompiled
// class. Exercises the BlockPos surface called out by the assignment:
//   static  asLong(int,int,int)            BlockPos.java:111-116
//   static  getX/getY/getZ(long)           BlockPos.java:75-85
//   static  of(long)                       BlockPos.java:87-89
//   static  offset(long,int,int,int)       BlockPos.java:71-73
//   static  offset(long,Direction)         BlockPos.java:67-69
//   static  getFlatIndex(long)             BlockPos.java:118-120
//   instance offset(int,int,int)           BlockPos.java:122-124
//   instance relative(Direction)           BlockPos.java:198-200
//   instance relative(Direction,steps)     BlockPos.java:202-206
//   instance above/below/north/south/west/east(+steps) BlockPos.java:150-196
//   instance distManhattan(Vec3i)          Vec3i.java:223-228 (inherited)
//
//   tools/run_groundtruth.ps1 -Tool BlockPosVerifyParity -Out mcpp/build/block_pos_verify.tsv
//
// All exercised methods are public; no reflection needed. Pure bit/int
// arithmetic, but BlockPos.<clinit> is harmless on its own — we still bootstrap
// (cheaply) to be safe against any static-init dependency, exactly like
// PosCodecParity. O is captured before Bootstrap so the TSV on stdout stays
// clean even if logging reconfigures System.out.
//
// Longs/ints emitted in decimal; the C++ test recomputes and compares exactly.

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;

public class BlockPosVerifyParity {
    static final java.io.PrintStream O = System.out;

    // FINITE/PHYSICAL int battery: zeros, units, signs, packing boundaries
    // (26-bit horizontal MAX_HORIZONTAL_COORDINATE=33554431, 12-bit Y range
    // [-2048,2047]), the 30000000 world border, and overflow-prone extremes.
    static final int[] XS = {
        0, 1, -1, 15, 16, -16, 17, 100, -100, 1000, -1000,
        33554431, -33554432, 33554430, -33554431,
        30000000, -30000000, 12345, -67890, 2097151, -2097152,
        2147483647, -2147483648, 1073741824, -1073741824, 65535, -65536
    };
    // Y is packed into 12 bits; sample its physical + boundary range too.
    static final int[] YS = {
        0, 1, -1, 15, 16, -16, 63, 64, -64, 255, 256, -320,
        2047, -2048, 2046, -2047, 1000, -1000, 319, 384,
        2147483647, -2147483648
    };
    static final int[] STEPS = { 0, 1, -1, 2, -2, 3, 7, 15, -15, 16, -16, 100, -100, 65535, -65536 };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ── static asLong / getX/Y/Z / of / getFlatIndex ────────────────────
        for (int x : XS) for (int y : YS) for (int z : XS) {
            long node = BlockPos.asLong(x, y, z);
            O.println("BP_ASLONG\t" + x + "\t" + y + "\t" + z + "\t" + node);
            O.println("BP_GET\t" + node + "\t" + BlockPos.getX(node) + "\t" + BlockPos.getY(node) + "\t" + BlockPos.getZ(node));
            BlockPos of = BlockPos.of(node);
            O.println("BP_OF\t" + node + "\t" + of.getX() + "\t" + of.getY() + "\t" + of.getZ());
            O.println("BP_FLAT\t" + node + "\t" + BlockPos.getFlatIndex(node));
        }

        // ── static offset(long, int,int,int) and offset(long, Direction) ────
        long[] BASES = {
            BlockPos.asLong(100, 64, -200),
            BlockPos.asLong(0, 0, 0),
            BlockPos.asLong(33554431, 2047, -33554432),
            BlockPos.asLong(-33554432, -2048, 33554431),
            BlockPos.asLong(30000000, 319, -30000000)
        };
        for (long base : BASES) {
            for (int sx : STEPS) for (int sy : STEPS) for (int sz : STEPS)
                O.println("BP_OFFSETL\t" + base + "\t" + sx + "\t" + sy + "\t" + sz + "\t" + BlockPos.offset(base, sx, sy, sz));
            for (Direction d : Direction.values())
                O.println("BP_OFFSETD\t" + base + "\t" + d.get3DDataValue() + "\t" + BlockPos.offset(base, d));
        }

        // ── instance offset/relative/above…east/distManhattan ──────────────
        int[][] V = {
            {0, 0, 0}, {1, 2, 3}, {-1, -2, -3}, {7, -8, 9}, {-15, 16, -17},
            {100, -200, 300}, {255, 256, -256},
            {33554431, 2047, -33554432}, {-33554432, -2048, 33554431},
            {30000000, -30000000, 12345}, {2147483647, 2147483647, 2147483647},
            {-2147483648, -2147483648, -2147483648}, {2147483647, -2147483648, 0},
            {1073741824, 1073741824, 1073741824}, {65535, -65536, 65536}, {3, 0, -4}
        };

        for (int[] a : V) {
            BlockPos p = new BlockPos(a[0], a[1], a[2]);

            // instance offset(int,int,int) incl. the (0,0,0)->this short-circuit
            int[][] OFFS = {
                {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, -1, -1},
                {15, -16, 17}, {2147483647, 1, -1}, {-2147483648, -1, 1}, {100, -100, 65536}
            };
            for (int[] o : OFFS) {
                BlockPos r = p.offset(o[0], o[1], o[2]);
                O.println("BP_OFFI\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + o[0] + "\t" + o[1] + "\t" + o[2]
                    + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
            }

            // relative(Direction) one-arg + above/below/north/south/west/east()
            for (Direction d : Direction.values()) {
                BlockPos r = p.relative(d);
                O.println("BP_RELDIR1\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + d.get3DDataValue()
                    + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
            }
            emit0("BP_ABOVE0", a, p.above());
            emit0("BP_BELOW0", a, p.below());
            emit0("BP_NORTH0", a, p.north());
            emit0("BP_SOUTH0", a, p.south());
            emit0("BP_WEST0",  a, p.west());
            emit0("BP_EAST0",  a, p.east());

            // relative(Direction,steps) + above/…/east(steps)
            for (int s : STEPS) {
                for (Direction d : Direction.values()) {
                    BlockPos r = p.relative(d, s);
                    O.println("BP_RELDIR\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                        + "\t" + d.get3DDataValue() + "\t" + s
                        + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
                }
                emit3("BP_ABOVE", a, s, p.above(s));
                emit3("BP_BELOW", a, s, p.below(s));
                emit3("BP_NORTH", a, s, p.north(s));
                emit3("BP_SOUTH", a, s, p.south(s));
                emit3("BP_WEST",  a, s, p.west(s));
                emit3("BP_EAST",  a, s, p.east(s));
            }
        }

        // distManhattan(Vec3i) over the cartesian product (inherited from Vec3i,
        // but exercised through a real BlockPos receiver/argument).
        for (int[] a : V) {
            BlockPos p = new BlockPos(a[0], a[1], a[2]);
            for (int[] b : V) {
                BlockPos q = new BlockPos(b[0], b[1], b[2]);
                O.println("BP_DISTMAN\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + p.distManhattan(q));
            }
        }
    }

    static void emit3(String tag, int[] a, int s, BlockPos r) {
        O.println(tag + "\t" + a[0] + "\t" + a[1] + "\t" + a[2] + "\t" + s
            + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
    }
    static void emit0(String tag, int[] a, BlockPos r) {
        O.println(tag + "\t" + a[0] + "\t" + a[1] + "\t" + a[2]
            + "\t" + r.getX() + "\t" + r.getY() + "\t" + r.getZ());
    }
}

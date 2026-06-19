// Ground-truth generator for the pure-integer surface of net.minecraft.core.BlockBox
// (Minecraft 26.1.2) using the REAL decompiled class. BlockBox is a record of two
// BlockPos (min,max) whose canonical constructor normalizes to per-component min/max.
//
//   tools/run_groundtruth.ps1 -Tool BlockBoxParity -Out mcpp/build/block_box.tsv
//
// TSV row format (tab-separated), dispatched by leading TAG in the C++ test.
// All values are ints in decimal (BlockBox's int surface has no float/double).
// Directions are emitted as get3DDataValue() (DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,
// EAST=5) to match the C++ mc::Direction ordinal.
//
// No Bootstrap needed (BlockBox / BlockPos / Direction are pure core classes that
// do not touch registries). Every method called is PUBLIC, so no reflection.

import net.minecraft.core.BlockBox;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;

public class BlockBoxParity {
    static final java.io.PrintStream O = System.out;

    // Wide int battery: zeros, units, signs, powers of two, large/overflow-prone.
    static final int[] STEPS = { 0, 1, -1, 2, -2, 3, 7, 16, -16, 100, -100, 65535, -65536, 2147483647, -2147483648 };

    static String p3(BlockPos p) { return p.getX() + "\t" + p.getY() + "\t" + p.getZ(); }

    public static void main(String[] args) throws Exception {
        // Representative corner pairs (a,b) — given to BlockBox of(a,b)/ctor (which
        // normalizes), so we cover already-sorted, reversed, mixed, equal, extreme.
        int[][] CORNERS = {
            {0, 0, 0}, {1, 2, 3}, {-1, -2, -3}, {7, -8, 9}, {-15, 16, -17},
            {5, 5, 5}, {-5, 12, 0}, {3, 0, -4}, {100, -200, 300}, {255, 256, -256},
            {30000000, -30000000, 12345}, {-30000000, 30000000, -12345},
            {2147483647, 2147483647, 2147483647}, {-2147483648, -2147483648, -2147483648},
            {2147483647, -2147483648, 0}, {1073741824, -1073741824, 65536}
        };

        // --- of(pos), of(a,b)/ctor normalization, isBlock, sizeX/Y/Z over pairs ---
        for (int[] a : CORNERS) {
            BlockPos pa = new BlockPos(a[0], a[1], a[2]);

            // of(pos): box is exactly [pos,pos]; isBlock() must be true.
            BlockBox single = BlockBox.of(pa);
            O.println("OFP\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                + "\t" + p3(single.min()) + "\t" + p3(single.max())
                + "\t" + (single.isBlock() ? 1 : 0));

            for (int[] b : CORNERS) {
                BlockPos pb = new BlockPos(b[0], b[1], b[2]);
                BlockBox box = BlockBox.of(pa, pb);  // canonical ctor normalizes
                // OFAB: min/max after normalization, isBlock, sizeX/Y/Z.
                O.println("OFAB\t" + a[0] + "\t" + a[1] + "\t" + a[2]
                    + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                    + "\t" + p3(box.min()) + "\t" + p3(box.max())
                    + "\t" + (box.isBlock() ? 1 : 0)
                    + "\t" + box.sizeX() + "\t" + box.sizeY() + "\t" + box.sizeZ());
            }
        }

        // --- include(pos) ("encapsulate"): grow a box to swallow a probe point ---
        int[][] PROBES = {
            {0, 0, 0}, {10, 10, 10}, {-10, -10, -10}, {1, -1, 1}, {500, -500, 0},
            {2147483647, 0, -2147483648}, {-30000000, 30000000, 30000000}, {7, 7, 7}
        };
        for (int[] a : CORNERS) {
            for (int[] b : CORNERS) {
                BlockBox box = BlockBox.of(new BlockPos(a[0], a[1], a[2]), new BlockPos(b[0], b[1], b[2]));
                for (int[] pr : PROBES) {
                    BlockBox inc = box.include(new BlockPos(pr[0], pr[1], pr[2]));
                    O.println("INC\t" + p3(box.min()) + "\t" + p3(box.max())
                        + "\t" + pr[0] + "\t" + pr[1] + "\t" + pr[2]
                        + "\t" + p3(inc.min()) + "\t" + p3(inc.max()));
                }
            }
        }

        // --- contains(pos) ("isInside"): probe inside/edge/outside ---
        for (int[] a : CORNERS) {
            for (int[] b : CORNERS) {
                BlockBox box = BlockBox.of(new BlockPos(a[0], a[1], a[2]), new BlockPos(b[0], b[1], b[2]));
                for (int[] pr : PROBES) {
                    boolean c = box.contains(new BlockPos(pr[0], pr[1], pr[2]));
                    O.println("CON\t" + p3(box.min()) + "\t" + p3(box.max())
                        + "\t" + pr[0] + "\t" + pr[1] + "\t" + pr[2]
                        + "\t" + (c ? 1 : 0));
                }
                // Also probe the actual min/max corners and a center-ish point — must be inside.
                BlockPos mn = box.min(), mx = box.max();
                emitCon(box, mn.getX(), mn.getY(), mn.getZ());
                emitCon(box, mx.getX(), mx.getY(), mx.getZ());
            }
        }

        // --- move(direction, amount): translate whole box ---
        for (int[] a : CORNERS) {
            for (int[] b : CORNERS) {
                BlockBox box = BlockBox.of(new BlockPos(a[0], a[1], a[2]), new BlockPos(b[0], b[1], b[2]));
                for (Direction d : Direction.values()) {
                    for (int s : STEPS) {
                        BlockBox mv = box.move(d, s);
                        O.println("MOV\t" + p3(box.min()) + "\t" + p3(box.max())
                            + "\t" + d.get3DDataValue() + "\t" + s
                            + "\t" + p3(mv.min()) + "\t" + p3(mv.max()));
                    }
                }
            }
        }

        // --- extend(direction, amount): grow box one side ---
        for (int[] a : CORNERS) {
            for (int[] b : CORNERS) {
                BlockBox box = BlockBox.of(new BlockPos(a[0], a[1], a[2]), new BlockPos(b[0], b[1], b[2]));
                for (Direction d : Direction.values()) {
                    for (int s : STEPS) {
                        BlockBox ex = box.extend(d, s);
                        O.println("EXT\t" + p3(box.min()) + "\t" + p3(box.max())
                            + "\t" + d.get3DDataValue() + "\t" + s
                            + "\t" + p3(ex.min()) + "\t" + p3(ex.max()));
                    }
                }
            }
        }

        // --- offset(Vec3i): translate by a free vector ---
        int[][] OFFS = {
            {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, -1, -1},
            {15, -16, 17}, {2147483647, 1, -1}, {-2147483648, -1, 1}, {100, -100, 65536}
        };
        for (int[] a : CORNERS) {
            for (int[] b : CORNERS) {
                BlockBox box = BlockBox.of(new BlockPos(a[0], a[1], a[2]), new BlockPos(b[0], b[1], b[2]));
                for (int[] o : OFFS) {
                    BlockBox of = box.offset(new net.minecraft.core.Vec3i(o[0], o[1], o[2]));
                    O.println("OFF\t" + p3(box.min()) + "\t" + p3(box.max())
                        + "\t" + o[0] + "\t" + o[1] + "\t" + o[2]
                        + "\t" + p3(of.min()) + "\t" + p3(of.max()));
                }
            }
        }
    }

    static void emitCon(BlockBox box, int x, int y, int z) {
        boolean c = box.contains(new BlockPos(x, y, z));
        O.println("CON\t" + p3(box.min()) + "\t" + p3(box.max())
            + "\t" + x + "\t" + y + "\t" + z + "\t" + (c ? 1 : 0));
    }
}

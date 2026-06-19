// Ground-truth generator for the PURE aggregation / corner / identity helpers of
// the REAL decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.BoundingBox
//
// These are exactly the members the sibling BoundingBoxParity.java gate skips
// (BoundingBox.h:35-39 lists them as UNPORTED):
//   * static Optional<BoundingBox> encapsulatingPositions(Iterable<BlockPos>)
//   * (mutating)  BoundingBox encapsulate(BoundingBox)
//   * (mutating)  BoundingBox move(int,int,int)
//   * void        forAllCorners(Consumer<BlockPos>)   -- ordered 8-corner walk
//   * int         hashCode()  ( == Objects.hash(six ints) )
//   * boolean     equals(Object)
//
// All are self-contained int arithmetic: NO world writes, NO RandomSource, NO
// registry/datapack. BoundingBox is public but its six int fields are private, so
// we read them by reflection. We Bootstrap per the harness contract before
// touching anything.
//
//   tools/run_groundtruth.ps1 -Tool BoundingBoxAggregateParity -Out mcpp/build/bounding_box_aggregate.tsv
//
// Line formats (all values decimal ints; a box is six ints minX..maxZ):
//   ENCAP   <a6> <b6> | <box6>                 -- a.encapsulate(b) (mutating; box6 == a after)
//   MOVE    <a6> <dx dy dz> | <box6>           -- a.move(dx,dy,dz) (mutating)
//   ENCPOSN <n> <px py pz>*n | <present> [box6]-- encapsulatingPositions(list); present 0 -> no box6
//   CORNERS <a6> | <cx cy cz>*8                -- forAllCorners visitation order (8 BlockPos)
//   HASH    <a6> | <int>                        -- hashCode()
//   EQ      <a6> <b6> | <0|1>                   -- a.equals(b)

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.levelgen.structure.BoundingBox;

public class BoundingBoxAggregateParity {
    static final java.io.PrintStream OUT = System.out;

    static Field F_minX, F_minY, F_minZ, F_maxX, F_maxY, F_maxZ;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        F_minX = field("minX"); F_minY = field("minY"); F_minZ = field("minZ");
        F_maxX = field("maxX"); F_maxY = field("maxY"); F_maxZ = field("maxZ");

        // Representative box battery: zero-volume, chunk-edge, negatives, mixed,
        // and Integer extremes (the hashCode/move wrap traps live near MIN/MAX).
        int[][] boxes = {
            {0, 0, 0, 0, 0, 0},
            {0, 0, 0, 15, 5, 15},
            {0, 0, 0, 16, 5, 16},
            {-1, 0, -1, 0, 5, 0},
            {-16, 60, -16, 16, 70, 16},
            {3, 0, 7, 20, 8, 41},
            {-33, -5, -33, -1, 5, -1},
            {1000, 0, -1000, 1003, 9, -990},
            {-100, -64, -100, 100, 320, 100},
            {-7, -7, -7, 7, 7, 7},
            {Integer.MIN_VALUE, 0, Integer.MIN_VALUE, Integer.MIN_VALUE + 30, 4, Integer.MIN_VALUE + 47},
            {Integer.MAX_VALUE - 30, 0, Integer.MAX_VALUE - 47, Integer.MAX_VALUE, 4, Integer.MAX_VALUE},
            {Integer.MIN_VALUE, Integer.MIN_VALUE, Integer.MIN_VALUE, Integer.MAX_VALUE, Integer.MAX_VALUE, Integer.MAX_VALUE},
            {7, -3, 11, 7, -3, 11},
            {-2147483640, 1, -2, 2147483640, 3, 4},
        };

        // Deltas for move() including wrap-inducing extremes.
        int[][] deltas = {
            {0, 0, 0}, {1, 2, 3}, {-5, -6, -7},
            {Integer.MAX_VALUE, 1, -1}, {16, -16, 32},
            {Integer.MIN_VALUE, Integer.MIN_VALUE, Integer.MIN_VALUE},
        };

        for (int[] a : boxes) {
            emitCorners(a);
            emitHash(a);
            for (int[] d : deltas) emitMove(a, d);
            for (int[] b : boxes) {
                emitEncap(a, b);
                emitEq(a, b);
            }
        }

        // encapsulatingPositions over several ordered position lists, plus the
        // empty case (-> Optional.empty()).
        int[][][] posLists = {
            { },                                              // empty -> absent
            { {5, 6, 7} },                                    // single
            { {0, 0, 0}, {10, 20, 30} },                      // grows in +dir
            { {10, 20, 30}, {0, 0, 0} },                      // grows in -dir (order)
            { {-5, 3, -5}, {5, -3, 5}, {0, 100, 0} },         // spread
            { {Integer.MIN_VALUE, 0, 0}, {Integer.MAX_VALUE, 0, 0} }, // extreme span
            { {7, 7, 7}, {7, 7, 7}, {7, 7, 7} },              // all-equal (point box)
        };
        for (int[][] pl : posLists) emitEncPosN(pl);

        OUT.flush();
    }

    // ── emitters ──

    static BoundingBox box(int[] v) {
        return new BoundingBox(v[0], v[1], v[2], v[3], v[4], v[5]);
    }

    static String six(int[] v) {
        return v[0] + "\t" + v[1] + "\t" + v[2] + "\t" + v[3] + "\t" + v[4] + "\t" + v[5];
    }

    static String dump(BoundingBox b) throws Exception {
        return F_minX.getInt(b) + "\t" + F_minY.getInt(b) + "\t" + F_minZ.getInt(b) + "\t"
             + F_maxX.getInt(b) + "\t" + F_maxY.getInt(b) + "\t" + F_maxZ.getInt(b);
    }

    static void emitEncap(int[] a, int[] b) throws Exception {
        BoundingBox A = box(a);            // mutated in place by encapsulate
        A.encapsulate(box(b));
        OUT.println("ENCAP\t" + six(a) + "\t" + six(b) + "\t" + dump(A));
    }

    static void emitMove(int[] a, int[] d) throws Exception {
        BoundingBox A = box(a);            // mutated in place by move
        A.move(d[0], d[1], d[2]);
        OUT.println("MOVE\t" + six(a) + "\t" + d[0] + "\t" + d[1] + "\t" + d[2] + "\t" + dump(A));
    }

    static void emitEncPosN(int[][] pl) throws Exception {
        List<BlockPos> list = new ArrayList<>();
        for (int[] p : pl) list.add(new BlockPos(p[0], p[1], p[2]));
        java.util.Optional<BoundingBox> opt = BoundingBox.encapsulatingPositions(list);
        StringBuilder sb = new StringBuilder("ENCPOSN\t").append(pl.length);
        for (int[] p : pl) sb.append("\t").append(p[0]).append("\t").append(p[1]).append("\t").append(p[2]);
        if (opt.isPresent()) {
            sb.append("\t1\t").append(dump(opt.get()));
        } else {
            sb.append("\t0");
        }
        OUT.println(sb.toString());
    }

    static void emitCorners(int[] a) {
        BoundingBox A = box(a);
        final List<Vec3i> corners = new ArrayList<>();
        A.forAllCorners(pos -> corners.add(new Vec3i(pos.getX(), pos.getY(), pos.getZ())));
        StringBuilder sb = new StringBuilder("CORNERS\t").append(six(a));
        for (Vec3i c : corners) sb.append("\t").append(c.getX()).append("\t").append(c.getY()).append("\t").append(c.getZ());
        OUT.println(sb.toString());
    }

    static void emitHash(int[] a) {
        OUT.println("HASH\t" + six(a) + "\t" + box(a).hashCode());
    }

    static void emitEq(int[] a, int[] b) {
        OUT.println("EQ\t" + six(a) + "\t" + six(b) + "\t" + (box(a).equals(box(b)) ? 1 : 0));
    }

    static Field field(String name) throws Exception {
        Field f = BoundingBox.class.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }
}

// Ground-truth generator for the PURE integer geometry in the REAL decompiled
// 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.BoundingBox
//
// Every method exercised here is self-contained int arithmetic: NO world writes,
// NO RandomSource, NO registry/datapack. BoundingBox is a plain public class, but
// its six int fields are private, so we read them via reflection. We still
// Bootstrap (per the harness contract) before touching anything.
//
//   tools/run_groundtruth.ps1 -Tool BoundingBoxParity -Out mcpp/build/bounding_box.tsv
//
// Line formats (all values decimal ints; a box is six ints minX..maxZ):
//   CTOR   <x0 y0 z0 x1 y1 z1> | <box6>         -- ctor incl. inverted-bounds fix
//   CORNER <ax ay az bx by bz> | <box6>         -- fromCorners(Vec3i,Vec3i)
//   ORIENT <fx fy fz ox oy oz w h d dirOrd> | <box6>  -- orientBox(...)
//   ENCAPB <a6> <b6> | <box6>                   -- encapsulating(a,b)
//   ENCPOS <a6> <px py pz> | <box6>             -- a.encapsulate(BlockPos)
//   MOVED  <a6> <dx dy dz> | <box6>             -- a.moved(dx,dy,dz)
//   INFL   <a6> <ix iy iz> | <box6>             -- a.inflatedBy(ix,iy,iz)
//   SPAN   <a6> | <xSpan ySpan zSpan>           -- getXSpan/getYSpan/getZSpan
//   LEN    <a6> | <lx ly lz>                    -- getLength()
//   CENTER <a6> | <cx cy cz>                    -- getCenter()
//   ISIN   <a6> <x y z> | <0|1>                 -- isInside(x,y,z)
//   ISECT  <a6> <b6> | <0|1>                    -- a.intersects(b)
//   ISECT4 <a6> <minX minZ maxX maxZ> | <0|1>   -- a.intersects(minX,minZ,maxX,maxZ)
//   CHUNKS <a6> | <n> <packedLong>*n            -- intersectingChunks() packed keys, in order

import java.lang.reflect.Field;
import java.util.List;
import java.util.stream.Collectors;

import net.minecraft.core.Direction;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.levelgen.structure.BoundingBox;

public class BoundingBoxParity {
    static final java.io.PrintStream OUT = System.out;

    static Field F_minX, F_minY, F_minZ, F_maxX, F_maxY, F_maxZ;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        F_minX = field("minX"); F_minY = field("minY"); F_minZ = field("minZ");
        F_maxX = field("maxX"); F_maxY = field("maxY"); F_maxZ = field("maxZ");

        // Representative coordinate battery: small structured values, chunk-edge
        // values (multiples / off-by-one around 16 to stress blockToSectionCoord's
        // signed >>4), negatives, and Integer extremes for the wrap traps.
        int[] coords = { Integer.MIN_VALUE, Integer.MIN_VALUE + 1, -2147483640,
            -1000, -257, -256, -255, -17, -16, -15, -1, 0, 1, 7, 15, 16, 17, 31,
            32, 33, 255, 256, 257, 1000, 2147483640, Integer.MAX_VALUE - 1,
            Integer.MAX_VALUE };
        int[] small = { -3, -1, 0, 1, 2, 5 };
        int[] dims = { 1, 2, 3, 5 };

        // ── CTOR: ordered and inverted corner pairs (covers the swap fix). ──
        for (int x0 : coords) for (int x1 : new int[]{x0, x0 + 1, x0 - 1, 0, x0 + 7}) {
            // y/z chosen from a small varied set to keep the battery strided, not
            // a full cube; include inverted axes individually and together.
            int[] ys = { 0, 5, -5, x0 };
            int[] zs = { 0, 3, -3, x1 };
            for (int yi = 0; yi < ys.length; yi++)
                emitCtor(x0, ys[yi], zs[yi], x1, zs[ys.length - 1 - yi], ys[ys.length - 1 - yi]);
        }

        // ── fromCorners: same ordered/inverted pairs, all axes. ──
        for (int a : small) for (int b : small) {
            emitCorner(a, a + 1, a + 2, b, b - 1, b + 3);
            emitCorner(a, b, a, b, a, b);
        }

        // ── orientBox: every horizontal + verticals (default branch), small foot
        // offsets, and the four real Directions. ──
        Direction[] dirs = Direction.values(); // DOWN,UP,NORTH,SOUTH,WEST,EAST
        int[] foot = { -16, 0, 16 };
        int[] off = { -2, 0, 3 };
        for (Direction d : dirs)
            for (int fx : foot) for (int fz : foot)
                for (int ox : off) for (int oz : off)
                    for (int w : dims) for (int dp : dims)
                        emitOrient(fx, 64, fz, ox, 0, oz, w, 4, dp, d);

        // A representative set of boxes for the binary ops / getters / chunks.
        int[][] boxes = {
            {0, 0, 0, 0, 0, 0}, {0, 0, 0, 15, 5, 15}, {0, 0, 0, 16, 5, 16},
            {-1, 0, -1, 0, 5, 0}, {-16, 60, -16, 16, 70, 16},
            {3, 0, 7, 20, 8, 41}, {-33, -5, -33, -1, 5, -1},
            {1000, 0, -1000, 1003, 9, -990}, {-100, -64, -100, 100, 320, 100},
            {Integer.MIN_VALUE, 0, Integer.MIN_VALUE, Integer.MIN_VALUE + 30, 4, Integer.MIN_VALUE + 47},
            {Integer.MAX_VALUE - 30, 0, Integer.MAX_VALUE - 47, Integer.MAX_VALUE, 4, Integer.MAX_VALUE},
            {-7, -7, -7, 7, 7, 7},
        };

        for (int[] a : boxes) {
            BoundingBox A = box(a);
            emitSpan(a, A);
            emitLen(a, A);
            emitCenter(a, A);
            emitChunks(a, A);
            // moved / inflatedBy with assorted deltas incl. wrap-inducing ones.
            int[][] deltas = {{0,0,0},{1,2,3},{-5,-6,-7},{Integer.MAX_VALUE,1,-1},{16,-16,32}};
            for (int[] dlt : deltas) { emitMoved(a, dlt); emitInfl(a, dlt); }
            // isInside probes at corners, just-outside, and center.
            int[][] pts = {
                {a[0],a[1],a[2]}, {a[3],a[4],a[5]}, {a[0]-1,a[1],a[2]},
                {a[3]+1,a[4],a[5]}, {a[0],a[1]-1,a[2]}, {(a[0]/2)+(a[3]/2),a[1],a[2]},
            };
            for (int[] p : pts) emitIsIn(a, p);
            // binary box ops vs every other box.
            for (int[] bb : boxes) {
                BoundingBox B = box(bb);
                emitEncapB(a, bb);
                emitIsect(a, bb);
            }
            // encapsulate(BlockPos), intersects(4-arg) over assorted points.
            for (int[] p : pts) { emitEncPos(a, p); emitIsect4(a, p); }
        }

        OUT.flush();
    }

    // ── helpers ──

    static BoundingBox box(int[] v) {
        return new BoundingBox(v[0], v[1], v[2], v[3], v[4], v[5]);
    }

    static String dump(BoundingBox b) throws Exception {
        return F_minX.getInt(b) + "\t" + F_minY.getInt(b) + "\t" + F_minZ.getInt(b) + "\t"
             + F_maxX.getInt(b) + "\t" + F_maxY.getInt(b) + "\t" + F_maxZ.getInt(b);
    }

    static String six(int[] v) {
        return v[0] + "\t" + v[1] + "\t" + v[2] + "\t" + v[3] + "\t" + v[4] + "\t" + v[5];
    }

    static void emitCtor(int x0, int y0, int z0, int x1, int y1, int z1) throws Exception {
        BoundingBox b = new BoundingBox(x0, y0, z0, x1, y1, z1);
        OUT.println("CTOR\t" + x0 + "\t" + y0 + "\t" + z0 + "\t" + x1 + "\t" + y1 + "\t" + z1
            + "\t" + dump(b));
    }

    static void emitCorner(int ax, int ay, int az, int bx, int by, int bz) throws Exception {
        BoundingBox b = BoundingBox.fromCorners(new Vec3i(ax, ay, az), new Vec3i(bx, by, bz));
        OUT.println("CORNER\t" + ax + "\t" + ay + "\t" + az + "\t" + bx + "\t" + by + "\t" + bz
            + "\t" + dump(b));
    }

    static void emitOrient(int fx, int fy, int fz, int ox, int oy, int oz,
                           int w, int h, int d, Direction dir) throws Exception {
        BoundingBox b = BoundingBox.orientBox(fx, fy, fz, ox, oy, oz, w, h, d, dir);
        OUT.println("ORIENT\t" + fx + "\t" + fy + "\t" + fz + "\t" + ox + "\t" + oy + "\t" + oz
            + "\t" + w + "\t" + h + "\t" + d + "\t" + dir.ordinal() + "\t" + dump(b));
    }

    static void emitSpan(int[] a, BoundingBox A) {
        OUT.println("SPAN\t" + six(a) + "\t" + A.getXSpan() + "\t" + A.getYSpan() + "\t" + A.getZSpan());
    }

    static void emitLen(int[] a, BoundingBox A) {
        Vec3i l = A.getLength();
        OUT.println("LEN\t" + six(a) + "\t" + l.getX() + "\t" + l.getY() + "\t" + l.getZ());
    }

    static void emitCenter(int[] a, BoundingBox A) {
        var c = A.getCenter();
        OUT.println("CENTER\t" + six(a) + "\t" + c.getX() + "\t" + c.getY() + "\t" + c.getZ());
    }

    static void emitMoved(int[] a, int[] d) throws Exception {
        BoundingBox A = box(a);
        BoundingBox m = A.moved(d[0], d[1], d[2]);
        OUT.println("MOVED\t" + six(a) + "\t" + d[0] + "\t" + d[1] + "\t" + d[2] + "\t" + dump(m));
    }

    static void emitInfl(int[] a, int[] d) throws Exception {
        BoundingBox A = box(a);
        BoundingBox m = A.inflatedBy(d[0], d[1], d[2]);
        OUT.println("INFL\t" + six(a) + "\t" + d[0] + "\t" + d[1] + "\t" + d[2] + "\t" + dump(m));
    }

    static void emitIsIn(int[] a, int[] p) {
        BoundingBox A = box(a);
        OUT.println("ISIN\t" + six(a) + "\t" + p[0] + "\t" + p[1] + "\t" + p[2]
            + "\t" + (A.isInside(p[0], p[1], p[2]) ? 1 : 0));
    }

    static void emitEncapB(int[] a, int[] b) throws Exception {
        BoundingBox r = BoundingBox.encapsulating(box(a), box(b));
        OUT.println("ENCAPB\t" + six(a) + "\t" + six(b) + "\t" + dump(r));
    }

    static void emitEncPos(int[] a, int[] p) throws Exception {
        BoundingBox A = box(a); // encapsulate mutates A; A is a throwaway copy.
        A.encapsulate(new net.minecraft.core.BlockPos(p[0], p[1], p[2]));
        OUT.println("ENCPOS\t" + six(a) + "\t" + p[0] + "\t" + p[1] + "\t" + p[2] + "\t" + dump(A));
    }

    static void emitIsect(int[] a, int[] b) {
        BoundingBox A = box(a), B = box(b);
        OUT.println("ISECT\t" + six(a) + "\t" + six(b) + "\t" + (A.intersects(B) ? 1 : 0));
    }

    static void emitIsect4(int[] a, int[] p) {
        BoundingBox A = box(a);
        // reuse three of the point coords as a (minX,minZ,maxX,maxZ) probe window.
        int minX = p[0], minZ = p[2], maxX = p[0] + 4, maxZ = p[2] + 4;
        OUT.println("ISECT4\t" + six(a) + "\t" + minX + "\t" + minZ + "\t" + maxX + "\t" + maxZ
            + "\t" + (A.intersects(minX, minZ, maxX, maxZ) ? 1 : 0));
    }

    static void emitChunks(int[] a, BoundingBox A) {
        List<ChunkPos> chunks = A.intersectingChunks().collect(Collectors.toList());
        StringBuilder sb = new StringBuilder("CHUNKS\t").append(six(a)).append("\t").append(chunks.size());
        for (ChunkPos c : chunks) sb.append("\t").append(c.pack());
        OUT.println(sb.toString());
    }

    static Field field(String name) throws Exception {
        Field f = BoundingBox.class.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }
}

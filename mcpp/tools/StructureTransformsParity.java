// Ground-truth generator for the pure structure-transform math using the REAL
// decompiled 26.1.2 classes:
//   net.minecraft.world.level.block.Rotation / Mirror
//   net.minecraft.core.Direction
//   net.minecraft.world.level.levelgen.structure.BoundingBox
//   net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate
//       (static transform / getZeroPositionWithTransform / getBoundingBox)
//
// These are pure functions (no registries/world), so no Bootstrap is needed.
// The C++ test (StructureTransformsParityTest) recomputes each row from the same
// inputs and must match bit-for-bit (doubles compared as raw IEEE-754 bits).
//
//   tools/run_groundtruth.ps1 -Tool StructureTransformsParity -Out mcpp/build/structure_transforms.tsv
//
// Enum ordinals are exchanged as ints (match the C++ enums):
//   Rotation: NONE=0, CLOCKWISE_90=1, CLOCKWISE_180=2, COUNTERCLOCKWISE_90=3
//   Mirror:   NONE=0, LEFT_RIGHT=1, FRONT_BACK=2
//   Direction:DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5

import java.lang.reflect.Method;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.phys.Vec3;

public class StructureTransformsParity {
    static final java.io.PrintStream OUT = System.out;

    static final Rotation[] ROTS = { Rotation.NONE, Rotation.CLOCKWISE_90, Rotation.CLOCKWISE_180, Rotation.COUNTERCLOCKWISE_90 };
    static final Mirror[] MIRS = { Mirror.NONE, Mirror.LEFT_RIGHT, Mirror.FRONT_BACK };
    static final Direction[] DIRS = { Direction.DOWN, Direction.UP, Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST };

    static int ri(Rotation r) { return java.util.Arrays.asList(ROTS).indexOf(r); }
    static int mi(Mirror m) { return java.util.Arrays.asList(MIRS).indexOf(m); }
    static int di(Direction d) { return d.ordinal(); }

    static String hx(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    public static void main(String[] args) throws Exception {
        // Constructing an inverted BoundingBox logs an ERROR via log4j, whose default
        // (no-Bootstrap) ConsoleAppender writes straight to the stdout FD and would
        // pollute the TSV. Silence the root logger before any logging occurs. OUT
        // (captured at class load) is the real stdout the TSV is written to.
        org.apache.logging.log4j.core.config.Configurator.setRootLevel(org.apache.logging.log4j.Level.OFF);

        // Rotation.getRotated(Rotation)
        for (Rotation self : ROTS)
            for (Rotation rot : ROTS)
                OUT.println("ROT_GETROTATED\t" + ri(self) + "\t" + ri(rot) + "\t" + ri(self.getRotated(rot)));

        // Rotation.rotate(Direction)
        for (Rotation self : ROTS)
            for (Direction d : DIRS)
                OUT.println("ROT_DIR\t" + ri(self) + "\t" + di(d) + "\t" + di(self.rotate(d)));

        // Rotation.rotate(int rotation, int steps)
        for (Rotation self : ROTS)
            for (int steps : new int[] { 4, 8, 16 })
                for (int rot = 0; rot < steps; rot++)
                    OUT.println("ROT_INT\t" + ri(self) + "\t" + rot + "\t" + steps + "\t" + self.rotate(rot, steps));

        // Mirror.mirror(int rotation, int steps)
        for (Mirror self : MIRS)
            for (int steps : new int[] { 4, 8, 16 })
                for (int rot = 0; rot < steps; rot++)
                    OUT.println("MIR_INT\t" + mi(self) + "\t" + rot + "\t" + steps + "\t" + self.mirror(rot, steps));

        // Mirror.getRotation(Direction) / Mirror.mirror(Direction)
        for (Mirror self : MIRS)
            for (Direction d : DIRS) {
                OUT.println("MIR_GETROT\t" + mi(self) + "\t" + di(d) + "\t" + ri(self.getRotation(d)));
                OUT.println("MIR_DIR\t" + mi(self) + "\t" + di(d) + "\t" + di(self.mirror(d)));
            }

        int[] gx = { -3, -1, 0, 2, 5 };
        int[] gy = { -2, 0, 7 };
        BlockPos[] pivots = { new BlockPos(0, 0, 0), new BlockPos(3, 0, 5), new BlockPos(-2, 0, 4) };

        // StructureTemplate.transform(BlockPos, Mirror, Rotation, pivot)
        for (int x : gx) for (int y : gy) for (int z : gx)
            for (Mirror m : MIRS) for (Rotation r : ROTS) for (BlockPos piv : pivots) {
                BlockPos in = new BlockPos(x, y, z);
                BlockPos o = StructureTemplate.transform(in, m, r, piv);
                OUT.println("XFORM\t" + x + "\t" + y + "\t" + z + "\t" + mi(m) + "\t" + ri(r)
                    + "\t" + piv.getX() + "\t" + piv.getY() + "\t" + piv.getZ()
                    + "\t" + o.getX() + "\t" + o.getY() + "\t" + o.getZ());
            }

        // StructureTemplate.transform(Vec3, Mirror, Rotation, pivot) — bit-exact
        double[] dx = { -1.5, 0.25, 3.75 };
        double[] dy = { 0.0, 2.5 };
        for (double x : dx) for (double y : dy) for (double z : dx)
            for (Mirror m : MIRS) for (Rotation r : ROTS) for (BlockPos piv : pivots) {
                Vec3 o = StructureTemplate.transform(new Vec3(x, y, z), m, r, piv);
                OUT.println("XFORMV\t" + hx(x) + "\t" + hx(y) + "\t" + hx(z) + "\t" + mi(m) + "\t" + ri(r)
                    + "\t" + piv.getX() + "\t" + piv.getY() + "\t" + piv.getZ()
                    + "\t" + hx(o.x) + "\t" + hx(o.y) + "\t" + hx(o.z));
            }

        // StructureTemplate.getZeroPositionWithTransform(BlockPos, Mirror, Rotation, sizeX, sizeZ)
        BlockPos[] zeros = { new BlockPos(0, 0, 0), new BlockPos(10, 5, -7) };
        int[][] sizesXZ = { {1,1}, {3,5}, {7,2}, {16,16} };
        for (BlockPos z0 : zeros) for (int[] s : sizesXZ)
            for (Mirror m : MIRS) for (Rotation r : ROTS) {
                BlockPos o = StructureTemplate.getZeroPositionWithTransform(z0, m, r, s[0], s[1]);
                OUT.println("ZEROPOS\t" + z0.getX() + "\t" + z0.getY() + "\t" + z0.getZ()
                    + "\t" + mi(m) + "\t" + ri(r) + "\t" + s[0] + "\t" + s[1]
                    + "\t" + o.getX() + "\t" + o.getY() + "\t" + o.getZ());
            }

        // StructureTemplate.getBoundingBox(position, rotation, pivot, mirror, size) — protected static
        Method getBB = StructureTemplate.class.getDeclaredMethod(
            "getBoundingBox", BlockPos.class, Rotation.class, BlockPos.class, Mirror.class, Vec3i.class);
        getBB.setAccessible(true);
        BlockPos[] positions = { new BlockPos(0, 0, 0), new BlockPos(4, -3, 9) };
        int[][] sizes3 = { {1,1,1}, {3,4,5}, {7,1,2}, {13,9,11} };
        BlockPos[] bbPivots = { new BlockPos(0, 0, 0), new BlockPos(2, 0, 3) };
        for (BlockPos pos : positions) for (int[] s : sizes3)
            for (Rotation r : ROTS) for (BlockPos piv : bbPivots) for (Mirror m : MIRS) {
                BoundingBox bb = (BoundingBox) getBB.invoke(null, pos, r, piv, m, new Vec3i(s[0], s[1], s[2]));
                OUT.println("BBOX\t" + pos.getX() + "\t" + pos.getY() + "\t" + pos.getZ()
                    + "\t" + ri(r) + "\t" + piv.getX() + "\t" + piv.getY() + "\t" + piv.getZ()
                    + "\t" + mi(m) + "\t" + s[0] + "\t" + s[1] + "\t" + s[2]
                    + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ()
                    + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ());
            }

        // BoundingBox.orientBox(footX,footY,footZ, offX,offY,offZ, w,h,d, dir)
        int[][] foots = { {0,0,0}, {5,2,-3} };
        int[][] offs = { {0,0,0}, {1,2,3} };
        int[][] dims = { {3,4,5}, {1,1,1}, {7,2,9} };
        Direction[] horiz = { Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST };
        for (int[] f : foots) for (int[] o : offs) for (int[] d : dims) for (Direction dir : horiz) {
            BoundingBox bb = BoundingBox.orientBox(f[0], f[1], f[2], o[0], o[1], o[2], d[0], d[1], d[2], dir);
            OUT.println("ORIENT\t" + f[0] + "\t" + f[1] + "\t" + f[2]
                + "\t" + o[0] + "\t" + o[1] + "\t" + o[2]
                + "\t" + d[0] + "\t" + d[1] + "\t" + d[2] + "\t" + di(dir)
                + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ()
                + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ());
        }

        // BoundingBox ctor normalization, fromCorners, intersects, isInside, center, span, encapsulating, moved, inflatedBy
        int[][] boxes = {
            {0,0,0,5,5,5}, {-3,2,-1,4,8,6}, {10,0,10,12,3,18}, {-20,-5,-20,-10,2,-12},
            {5,5,5,0,0,0} /* inverted */, {2,1,2,2,1,2} /* point */
        };
        for (int[] b : boxes) {
            BoundingBox bb = new BoundingBox(b[0], b[1], b[2], b[3], b[4], b[5]);
            OUT.println("BB_CTOR\t" + b[0] + "\t" + b[1] + "\t" + b[2] + "\t" + b[3] + "\t" + b[4] + "\t" + b[5]
                + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ()
                + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ());
            BlockPos c = bb.getCenter();
            OUT.println("BB_CENTER\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ() + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ()
                + "\t" + c.getX() + "\t" + c.getY() + "\t" + c.getZ());
            OUT.println("BB_SPAN\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ() + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ()
                + "\t" + bb.getXSpan() + "\t" + bb.getYSpan() + "\t" + bb.getZSpan());
            BoundingBox mv = bb.moved(3, -2, 7);
            OUT.println("BB_MOVED\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ() + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ()
                + "\t3\t-2\t7\t" + mv.minX() + "\t" + mv.minY() + "\t" + mv.minZ() + "\t" + mv.maxX() + "\t" + mv.maxY() + "\t" + mv.maxZ());
            BoundingBox inf = bb.inflatedBy(2, 1, 3);
            OUT.println("BB_INFLATE\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ() + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ()
                + "\t2\t1\t3\t" + inf.minX() + "\t" + inf.minY() + "\t" + inf.minZ() + "\t" + inf.maxX() + "\t" + inf.maxY() + "\t" + inf.maxZ());
        }

        // BoundingBox.fromCorners + encapsulating + intersects + isInside over pairs
        int[][] corners = { {0,0,0}, {5,3,8}, {-4,-2,-6}, {2,9,-1} };
        for (int[] a : corners) for (int[] b : corners) {
            BoundingBox bb = BoundingBox.fromCorners(new Vec3i(a[0], a[1], a[2]), new Vec3i(b[0], b[1], b[2]));
            OUT.println("BB_FROMCORNERS\t" + a[0] + "\t" + a[1] + "\t" + a[2] + "\t" + b[0] + "\t" + b[1] + "\t" + b[2]
                + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ() + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ());
        }
        for (int[] a : boxes) for (int[] b : boxes) {
            BoundingBox ba = new BoundingBox(a[0], a[1], a[2], a[3], a[4], a[5]);
            BoundingBox bb = new BoundingBox(b[0], b[1], b[2], b[3], b[4], b[5]);
            OUT.println("BB_INTERSECT\t" + ba.minX() + "\t" + ba.minY() + "\t" + ba.minZ() + "\t" + ba.maxX() + "\t" + ba.maxY() + "\t" + ba.maxZ()
                + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ() + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ()
                + "\t" + (ba.intersects(bb) ? 1 : 0));
            BoundingBox enc = BoundingBox.encapsulating(ba, bb);
            OUT.println("BB_ENCAPS\t" + ba.minX() + "\t" + ba.minY() + "\t" + ba.minZ() + "\t" + ba.maxX() + "\t" + ba.maxY() + "\t" + ba.maxZ()
                + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ() + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ()
                + "\t" + enc.minX() + "\t" + enc.minY() + "\t" + enc.minZ() + "\t" + enc.maxX() + "\t" + enc.maxY() + "\t" + enc.maxZ());
        }
        BoundingBox probe = new BoundingBox(-2, 0, -2, 4, 6, 4);
        for (int x = -4; x <= 6; x++) for (int y = -1; y <= 7; y += 2) for (int z = -4; z <= 6; z++)
            OUT.println("BB_ISINSIDE\t-2\t0\t-2\t4\t6\t4\t" + x + "\t" + y + "\t" + z + "\t" + (probe.isInside(x, y, z) ? 1 : 0));
    }
}

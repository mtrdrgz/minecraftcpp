// Ground-truth generator for the PURE placement-coordinate helpers in the REAL
// decompiled 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate
//     public        BlockPos calculateConnectedPosition(StructurePlaceSettings, BlockPos,
//                                                        StructurePlaceSettings, BlockPos)
//     public static BlockPos calculateRelativePosition(StructurePlaceSettings, BlockPos)
//
// These are the coordinate helpers jigsaw uses to connect two pool elements at
// their matching jigsaw markers. Pure math (no world/registry/random), but we
// still run the mandated Bootstrap so class init matches the rest of the harness.
//
//   tools/run_groundtruth.ps1 -Tool StructureConnectedPositionParity -Out mcpp/build/structure_connected_position.tsv
//
// The C++ test (StructureConnectedPositionParityTest) recomputes each row from the
// same inputs and must match exactly (ints compared in decimal).
//
// Enum ordinals exchanged as ints (match the C++ enums):
//   Rotation: NONE=0, CLOCKWISE_90=1, CLOCKWISE_180=2, COUNTERCLOCKWISE_90=3
//   Mirror:   NONE=0, LEFT_RIGHT=1, FRONT_BACK=2

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;

public class StructureConnectedPositionParity {
    static final java.io.PrintStream OUT = System.out;

    static final Rotation[] ROTS = { Rotation.NONE, Rotation.CLOCKWISE_90, Rotation.CLOCKWISE_180, Rotation.COUNTERCLOCKWISE_90 };
    static final Mirror[] MIRS = { Mirror.NONE, Mirror.LEFT_RIGHT, Mirror.FRONT_BACK };

    static int ri(Rotation r) { return java.util.Arrays.asList(ROTS).indexOf(r); }
    static int mi(Mirror m) { return java.util.Arrays.asList(MIRS).indexOf(m); }

    static StructurePlaceSettings settings(Mirror m, Rotation r, BlockPos pivot) {
        return new StructurePlaceSettings().setMirror(m).setRotation(r).setRotationPivot(pivot);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StructureTemplate template = new StructureTemplate();

        // Representative battery of connection points, pivots, and all 12
        // mirror x rotation combinations on each side. Coordinates span negative,
        // zero, and positive so the transform's signed pivot arithmetic and the
        // final element-wise subtract are all exercised.
        int[] cx = { -7, -1, 0, 3, 11 };
        int[] cy = { -4, 0, 6 };
        BlockPos[] pivots = { new BlockPos(0, 0, 0), new BlockPos(2, 0, 5), new BlockPos(-3, 0, 4), new BlockPos(8, 0, -6) };

        // calculateRelativePosition(settings, pos) — the half each side runs.
        for (int x : cx) for (int y : cy) for (int z : cx)
            for (Mirror m : MIRS) for (Rotation r : ROTS) for (BlockPos piv : pivots) {
                BlockPos in = new BlockPos(x, y, z);
                BlockPos o = StructureTemplate.calculateRelativePosition(settings(m, r, piv), in);
                OUT.println("RELPOS\t" + x + "\t" + y + "\t" + z
                    + "\t" + mi(m) + "\t" + ri(r)
                    + "\t" + piv.getX() + "\t" + piv.getY() + "\t" + piv.getZ()
                    + "\t" + o.getX() + "\t" + o.getY() + "\t" + o.getZ());
            }

        // calculateConnectedPosition(s1, c1, s2, c2). Cross the two sides over a
        // compact-but-representative grid (different mirror/rotation/pivot per side)
        // so the subtract order (markerPos1 - markerPos2) is locked down.
        int[] gx = { -5, 0, 4 };
        int[] gy = { -2, 0, 3 };
        Mirror[] mPick = { Mirror.NONE, Mirror.LEFT_RIGHT, Mirror.FRONT_BACK };
        Rotation[] rPick = { Rotation.NONE, Rotation.CLOCKWISE_90, Rotation.COUNTERCLOCKWISE_90, Rotation.CLOCKWISE_180 };
        BlockPos[] pPick = { new BlockPos(0, 0, 0), new BlockPos(3, 0, -2), new BlockPos(-4, 0, 6) };

        int i = 0;
        for (int x1 : gx) for (int y1 : gy) for (int z1 : gx)
            for (int x2 : gx) for (int z2 : gx) {
                // Rotate the (mirror, rotation, pivot) selection independently per
                // side as we walk the grid, giving asymmetric settings without an
                // explosive row count.
                Mirror m1 = mPick[i % mPick.length];
                Rotation r1 = rPick[i % rPick.length];
                BlockPos p1 = pPick[i % pPick.length];
                Mirror m2 = mPick[(i + 1) % mPick.length];
                Rotation r2 = rPick[(i + 2) % rPick.length];
                BlockPos p2 = pPick[(i + 1) % pPick.length];
                i++;

                BlockPos c1 = new BlockPos(x1, y1, z1);
                BlockPos c2 = new BlockPos(x2, y1, z2);
                BlockPos o = template.calculateConnectedPosition(settings(m1, r1, p1), c1, settings(m2, r2, p2), c2);
                OUT.println("CONNPOS"
                    + "\t" + x1 + "\t" + y1 + "\t" + z1 + "\t" + mi(m1) + "\t" + ri(r1)
                    + "\t" + p1.getX() + "\t" + p1.getY() + "\t" + p1.getZ()
                    + "\t" + x2 + "\t" + y1 + "\t" + z2 + "\t" + mi(m2) + "\t" + ri(r2)
                    + "\t" + p2.getX() + "\t" + p2.getY() + "\t" + p2.getZ()
                    + "\t" + o.getX() + "\t" + o.getY() + "\t" + o.getZ());
            }

        // Int-wrap boundary cases: feed Integer.MIN/MAX coords/pivots through the
        // real method so the C++ uint32 wrap path is verified against the JVM's
        // two's-complement wrap (negate of MIN_VALUE, add overflow).
        int MIN = Integer.MIN_VALUE, MAX = Integer.MAX_VALUE;
        int[][] edge = {
            { MIN, 0, 0,   0, 0, 0 },
            { MAX, 0, 0,   0, 0, 0 },
            { 0, 0, 0,     MIN, 0, 0 },
            { 0, 0, 0,     MAX, 0, 0 },
            { MIN, MIN, MIN,  MAX, MAX, MAX },
            { MAX, MAX, MAX,  MIN, MIN, MIN },
            { MIN, 0, MAX,    MAX, 0, MIN },
        };
        for (int[] e : edge)
            for (Mirror m : MIRS) for (Rotation r : ROTS) {
                BlockPos c1 = new BlockPos(e[0], e[1], e[2]);
                BlockPos c2 = new BlockPos(e[3], e[4], e[5]);
                // pivot 0 keeps transform identity-ish so the subtract wrap is the
                // thing under test; mirror/rotation still flip signs (another wrap
                // source at the extremes).
                BlockPos o = template.calculateConnectedPosition(
                    settings(m, r, BlockPos.ZERO), c1, settings(Mirror.NONE, Rotation.NONE, BlockPos.ZERO), c2);
                OUT.println("CONNEDGE"
                    + "\t" + e[0] + "\t" + e[1] + "\t" + e[2] + "\t" + mi(m) + "\t" + ri(r)
                    + "\t" + e[3] + "\t" + e[4] + "\t" + e[5]
                    + "\t" + o.getX() + "\t" + o.getY() + "\t" + o.getZ());
            }
    }
}

// Ground truth for net.minecraft.client.renderer.Octree — the section-render octree
// used by LevelRenderer for frustum-ordered chunk traversal.
//
// Octree is GL-free (it imports only SectionRenderDispatcher, Frustum, core pos
// types, Mth, BoundingBox, AABB — nothing from com.mojang.blaze3d), so this harness
// drives the REAL class directly: it constructs Octree(SectionPos, renderDistance,
// sectionsPerChunk, minBlockY), then reflects the private `root` Branch + the private
// AxisSorting enum + the private static Branch.getNodeIndex + the private
// Branch.createChildBoundingBox + the private Octree.isClose. No GPU is touched.
//
//   mcpp/tools/run_groundtruth.ps1 -Tool OctreeParity -Out mcpp/build/octree.tsv
//
// Row formats (tab-separated, all decimal ints unless noted):
//   ROOT secX secY secZ renderDistance sectionsPerChunk minBlockY  minX minY minZ maxX maxY maxZ
//   BRANCH secX secY secZ renderDistance sectionsPerChunk minBlockY  bbCenterX bbCenterY bbCenterZ \
//          sortingOrdinal  xDiffNeg yDiffNeg zDiffNeg
//   SORT absX absY absZ  ordinal
//   NODE sortingOrdinal xOpp yOpp zOpp  index
//   CHILD secX secY secZ renderDistance sectionsPerChunk minBlockY  xNeg yNeg zNeg \
//          cminX cminY cminZ cmaxX cmaxY cmaxZ
//   CLOSE secX secY secZ  minX minY minZ maxX maxY maxZ closeDistance  result(0|1)

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

import net.minecraft.client.renderer.Octree;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.levelgen.structure.BoundingBox;

public class OctreeParity {
    static final java.io.PrintStream O = System.out;

    static Class<?> OCTREE;
    static Class<?> BRANCH;       // Octree$Branch
    static Class<?> AXIS_SORTING; // Octree$AxisSorting
    static Constructor<?> OCTREE_CTOR;
    static Field F_ROOT;          // Octree.root
    static Field F_BB;            // Branch.boundingBox
    static Field F_CX, F_CY, F_CZ;        // Branch.bbCenterX/Y/Z
    static Field F_SORTING;       // Branch.sorting
    static Field F_XNEG, F_YNEG, F_ZNEG;  // Branch.camera{X,Y,Z}DiffNegative
    static Method M_GET_AXIS_SORTING;     // AxisSorting.getAxisSorting(int,int,int)
    static Method M_GET_NODE_INDEX;       // Branch.getNodeIndex(AxisSorting,bool,bool,bool)
    static Method M_CREATE_CHILD;         // Branch.createChildBoundingBox(bool,bool,bool)
    static Method M_IS_CLOSE;             // Octree.isClose(d,d,d,d,d,d,int)
    static Object[] AXIS_CONSTANTS;       // AxisSorting.values() (index == ordinal)

    static void setup() throws Exception {
        OCTREE = Octree.class;
        OCTREE_CTOR = OCTREE.getDeclaredConstructor(SectionPos.class, int.class, int.class, int.class);
        OCTREE_CTOR.setAccessible(true);

        F_ROOT = OCTREE.getDeclaredField("root");
        F_ROOT.setAccessible(true);
        BRANCH = F_ROOT.getType(); // Octree$Branch

        F_BB = BRANCH.getDeclaredField("boundingBox"); F_BB.setAccessible(true);
        F_CX = BRANCH.getDeclaredField("bbCenterX"); F_CX.setAccessible(true);
        F_CY = BRANCH.getDeclaredField("bbCenterY"); F_CY.setAccessible(true);
        F_CZ = BRANCH.getDeclaredField("bbCenterZ"); F_CZ.setAccessible(true);
        F_SORTING = BRANCH.getDeclaredField("sorting"); F_SORTING.setAccessible(true);
        AXIS_SORTING = F_SORTING.getType(); // Octree$AxisSorting
        F_XNEG = BRANCH.getDeclaredField("cameraXDiffNegative"); F_XNEG.setAccessible(true);
        F_YNEG = BRANCH.getDeclaredField("cameraYDiffNegative"); F_YNEG.setAccessible(true);
        F_ZNEG = BRANCH.getDeclaredField("cameraZDiffNegative"); F_ZNEG.setAccessible(true);

        M_GET_AXIS_SORTING = AXIS_SORTING.getDeclaredMethod("getAxisSorting", int.class, int.class, int.class);
        M_GET_AXIS_SORTING.setAccessible(true);
        M_GET_NODE_INDEX = BRANCH.getDeclaredMethod("getNodeIndex", AXIS_SORTING, boolean.class, boolean.class, boolean.class);
        M_GET_NODE_INDEX.setAccessible(true);
        M_CREATE_CHILD = BRANCH.getDeclaredMethod("createChildBoundingBox", boolean.class, boolean.class, boolean.class);
        M_CREATE_CHILD.setAccessible(true);
        M_IS_CLOSE = OCTREE.getDeclaredMethod("isClose", double.class, double.class, double.class,
                double.class, double.class, double.class, int.class);
        M_IS_CLOSE.setAccessible(true);

        Method values = AXIS_SORTING.getDeclaredMethod("values");
        values.setAccessible(true);
        AXIS_CONSTANTS = (Object[]) values.invoke(null);
    }

    static Object newOctree(int secX, int secY, int secZ, int rd, int spc, int minBlockY) throws Exception {
        return OCTREE_CTOR.newInstance(SectionPos.of(secX, secY, secZ), rd, spc, minBlockY);
    }

    static int ordinal(Object axisEnum) {
        return ((Enum<?>) axisEnum).ordinal();
    }

    // Emit ROOT (root BoundingBox) + BRANCH (root branch derived geometry).
    static void emitTree(int secX, int secY, int secZ, int rd, int spc, int minBlockY) throws Exception {
        Object oct = newOctree(secX, secY, secZ, rd, spc, minBlockY);
        Object root = F_ROOT.get(oct);
        BoundingBox bb = (BoundingBox) F_BB.get(root);
        String key = secX + "\t" + secY + "\t" + secZ + "\t" + rd + "\t" + spc + "\t" + minBlockY;
        O.println("ROOT\t" + key + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ() + "\t"
                + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ());
        O.println("BRANCH\t" + key + "\t" + F_CX.getInt(root) + "\t" + F_CY.getInt(root) + "\t" + F_CZ.getInt(root)
                + "\t" + ordinal(F_SORTING.get(root))
                + "\t" + (F_XNEG.getBoolean(root) ? 1 : 0)
                + "\t" + (F_YNEG.getBoolean(root) ? 1 : 0)
                + "\t" + (F_ZNEG.getBoolean(root) ? 1 : 0));

        // CHILD subdivisions of the root branch for all 8 octants.
        for (int m = 0; m < 8; m++) {
            boolean xn = (m & 1) != 0, yn = (m & 2) != 0, zn = (m & 4) != 0;
            BoundingBox cb = (BoundingBox) M_CREATE_CHILD.invoke(root, xn, yn, zn);
            O.println("CHILD\t" + key + "\t" + (xn ? 1 : 0) + "\t" + (yn ? 1 : 0) + "\t" + (zn ? 1 : 0)
                    + "\t" + cb.minX() + "\t" + cb.minY() + "\t" + cb.minZ()
                    + "\t" + cb.maxX() + "\t" + cb.maxY() + "\t" + cb.maxZ());
        }
    }

    static void emitSort(int ax, int ay, int az) throws Exception {
        Object s = M_GET_AXIS_SORTING.invoke(null, ax, ay, az);
        O.println("SORT\t" + ax + "\t" + ay + "\t" + az + "\t" + ordinal(s));
    }

    static void emitNode(int sortingOrdinal, boolean xo, boolean yo, boolean zo) throws Exception {
        Object sortingEnum = AXIS_CONSTANTS[sortingOrdinal];
        int idx = (int) M_GET_NODE_INDEX.invoke(null, sortingEnum, xo, yo, zo);
        O.println("NODE\t" + sortingOrdinal + "\t" + (xo ? 1 : 0) + "\t" + (yo ? 1 : 0) + "\t" + (zo ? 1 : 0)
                + "\t" + idx);
    }

    static void emitClose(int secX, int secY, int secZ, int rd, int spc, int minBlockY,
                          double minX, double minY, double minZ, double maxX, double maxY, double maxZ,
                          int closeDistance) throws Exception {
        // isClose() reads this.cameraSectionCenter; construct an Octree with the section
        // coords so center() = origin().offset(8,8,8) is set, then invoke the private method.
        Object oct = newOctree(secX, secY, secZ, rd, spc, minBlockY);
        boolean r = (boolean) M_IS_CLOSE.invoke(oct, minX, minY, minZ, maxX, maxY, maxZ, closeDistance);
        O.println("CLOSE\t" + secX + "\t" + secY + "\t" + secZ
                + "\t" + (long) minX + "\t" + (long) minY + "\t" + (long) minZ
                + "\t" + (long) maxX + "\t" + (long) maxY + "\t" + (long) maxZ
                + "\t" + closeDistance + "\t" + (r ? 1 : 0));
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        setup();

        // --- ROOT + BRANCH + CHILD: representative camera sections / render distances /
        //     sectionsPerChunk / minBlockY (the world-height min section influences minY).
        int[] sxs = {0, 1, -1, 7, -8, 31, -32, 100, -100};
        int[] sys = {0, 4, -4, 15, -16, 19, -20};
        int[] szs = {0, 1, -1, 7, -8, 60, -60};
        int[] rds = {2, 3, 4, 8, 12, 16, 32};
        int[] spcs = {16, 24};          // sectionsPerChunk: 16 (256-block) / 24 (384-block) worlds
        int[] minYs = {-64, 0, -2032};  // minBlockY of the world bottom

        // Full-ish sweep but bounded: vary one dimension at a time around a base, plus a grid.
        for (int rd : rds) {
            for (int spc : spcs) {
                for (int my : minYs) {
                    emitTree(0, 0, 0, rd, spc, my);
                }
            }
        }
        for (int sx : sxs) for (int sy : sys) for (int sz : szs) {
            emitTree(sx, sy, sz, 8, 24, -64);
        }
        for (int sx : sxs) for (int rd : rds) {
            emitTree(sx, 4, -7, rd, 24, -64);
        }

        // --- SORT: getAxisSorting over all tie / strict-greater orderings.
        int[] vals = {0, 1, 2, 3, 5, 7, 8, 100, 1000};
        for (int a : vals) for (int b : vals) for (int c : vals) emitSort(a, b, c);

        // --- NODE: getNodeIndex for every (sorting ordinal 0..5) x (xo,yo,zo) combo.
        for (int s = 0; s < 6; s++)
            for (int m = 0; m < 8; m++)
                emitNode(s, (m & 1) != 0, (m & 2) != 0, (m & 4) != 0);

        // --- CLOSE: isClose for a camera at section (0,0,0) center (8,8,8) and others,
        //     boxes that straddle / sit inside / outside, with several closeDistances.
        int[][] cams = {{0, 0, 0}, {2, 1, -3}, {-5, 4, 6}};
        long[][] boxes = {
            {-16, -16, -16, 16, 16, 16},
            {0, 0, 0, 0, 0, 0},
            {8, 8, 8, 8, 8, 8},
            {-100, -100, -100, -50, -50, -50},
            {50, 50, 50, 100, 100, 100},
            {-1, -1, -1, 1, 1, 1},
            {7, 7, 7, 9, 9, 9},
        };
        int[] dists = {0, 1, 8, 16, 64, 200};
        for (int[] cam : cams)
            for (long[] bx : boxes)
                for (int d : dists)
                    emitClose(cam[0], cam[1], cam[2], 8, 24, -64,
                            bx[0], bx[1], bx[2], bx[3], bx[4], bx[5], d);
    }
}

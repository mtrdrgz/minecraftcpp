// Ground-truth generator for the PURE block-ordering helper in the REAL decompiled
// 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate
//     private static List<StructureBlockInfo> buildInfoList(
//         List<StructureBlockInfo> fullBlockList,
//         List<StructureBlockInfo> blockEntitiesList,
//         List<StructureBlockInfo> otherBlocksList)
//
// This is the canonical-ordering routine fillFromWorld() runs at capture time:
// it sorts each of the three input lists by (Y, then X, then Z) and concatenates
// them as  full ++ other ++ blockEntities.  Pure ordering over BlockPos; the
// BlockState/NBT ride along untouched. We tag every input block with a unique id
// (carried in its NBT) so the resulting permutation is fully observable, and we
// emit both the inputs and the produced id order so the C++ test recomputes from
// the same inputs and compares.
//
//   tools/run_groundtruth.ps1 -Tool StructureTemplateBuildInfoListParity -Out mcpp/build/structure_template_build_info_list.tsv
//
// Reflection: buildInfoList is `private static`, so we resolve it with
// getDeclaredMethod + setAccessible and invoke it (no Unsafe needed — the inputs
// are public StructureBlockInfo records built via their public constructor). Any
// InvocationTargetException is unwrapped via getCause(). javac must emit ZERO
// stderr (run_groundtruth.ps1 treats any javac note/warning as fatal), hence the
// class-level @SuppressWarnings.
//
// TSV rows (leading TAG, all ints decimal):
//   IN  \t caseId \t listIdx(0=full,1=blockEntities,2=other) \t x \t y \t z \t id
//   OUT \t caseId \t n \t id0 \t id1 \t ... \t id(n-1)
// The REAL Bootstrap can log to stdout, so the C++ side skips any line whose first
// field is not IN/OUT.

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;

@SuppressWarnings({"deprecation", "unchecked"})
public class StructureTemplateBuildInfoListParity {
    static final java.io.PrintStream OUT = System.out;

    // A running unique id assigned to each input block as it is created.
    static int nextId = 0;

    // The single shared (irrelevant-to-ordering) BlockState all inputs carry.
    static BlockState AIR;

    // Reflective handle to the private static buildInfoList.
    static Method BUILD;

    // Class object for StructureBlockInfo (inner record of StructureTemplate).
    static Class<?> INFO_CLS;
    static java.lang.reflect.Constructor<?> INFO_CTOR;
    static Method INFO_POS;
    static Method INFO_NBT;

    static Object makeInfo(int x, int y, int z) throws Exception {
        int id = nextId++;
        CompoundTag nbt = new CompoundTag();
        nbt.putInt("id", id);
        Object info = INFO_CTOR.newInstance(new BlockPos(x, y, z), AIR, nbt);
        return new Object[] {info, id, x, y, z};
    }

    // Emit the inputs for a case, run the real method, emit the resulting id order.
    static void emitCase(int caseId, int[][] full, int[][] blockEntities, int[][] other) throws Exception {
        nextId = 0; // ids are per-case so the C++ side can reconstruct deterministically
        List<Object> fullList = new ArrayList<>();
        List<Object> beList = new ArrayList<>();
        List<Object> otherList = new ArrayList<>();

        emitList(caseId, 0, full, fullList);
        emitList(caseId, 1, blockEntities, beList);
        emitList(caseId, 2, other, otherList);

        // Drive the REAL private static buildInfoList(full, blockEntities, other).
        List<Object> result;
        try {
            result = (List<Object>) BUILD.invoke(null, fullList, beList, otherList);
        } catch (InvocationTargetException e) {
            Throwable c = e.getCause();
            if (c instanceof Exception) throw (Exception) c;
            throw new RuntimeException(c);
        }

        StringBuilder sb = new StringBuilder();
        sb.append("OUT\t").append(caseId).append('\t').append(result.size());
        for (Object info : result) {
            CompoundTag nbt = (CompoundTag) INFO_NBT.invoke(info);
            int id = nbt.getIntOr("id", -1);
            sb.append('\t').append(id);
        }
        OUT.println(sb.toString());
    }

    static void emitList(int caseId, int listIdx, int[][] blocks, List<Object> dst) throws Exception {
        for (int[] b : blocks) {
            Object[] made = (Object[]) makeInfo(b[0], b[1], b[2]);
            dst.add(made[0]);
            int id = (Integer) made[1];
            // IN \t caseId \t listIdx \t x \t y \t z \t id
            OUT.println("IN\t" + caseId + "\t" + listIdx + "\t" + b[0] + "\t" + b[1] + "\t" + b[2] + "\t" + id);
        }
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        AIR = Blocks.AIR.defaultBlockState();

        // Resolve the StructureBlockInfo record + its accessors and the private
        // static buildInfoList(List,List,List).
        INFO_CLS = Class.forName(
            "net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate$StructureBlockInfo");
        INFO_CTOR = INFO_CLS.getDeclaredConstructor(BlockPos.class, BlockState.class, CompoundTag.class);
        INFO_CTOR.setAccessible(true);
        INFO_POS = INFO_CLS.getMethod("pos");
        INFO_NBT = INFO_CLS.getMethod("nbt");

        BUILD = StructureTemplate.class.getDeclaredMethod("buildInfoList", List.class, List.class, List.class);
        BUILD.setAccessible(true);

        int caseId = 0;

        // Case 0: empty lists.
        emitCase(caseId++, new int[][] {}, new int[][] {}, new int[][] {});

        // Case 1: single block in each list (exercises the concat order
        // full ++ other ++ blockEntities directly).
        emitCase(caseId++,
            new int[][] {{5, 1, 5}},            // full
            new int[][] {{0, 0, 0}},            // blockEntities
            new int[][] {{9, 2, 3}});           // other

        // Case 2: in-list reordering by (Y,X,Z). Same list, scrambled input order
        // covering distinct Y, then ties on Y differing in X, then ties on (Y,X)
        // differing in Z.
        emitCase(caseId++,
            new int[][] {
                {3, 5, 7},
                {1, 5, 2},   // same Y=5 as above, smaller X -> sorts before
                {1, 5, 9},   // same (Y=5,X=1), larger Z -> after {1,5,2}
                {0, 0, 0},
                {8, -2, 4},  // smallest Y -> sorts first
                {2, 5, 2},   // Y=5, X between 1 and 3
            },
            new int[][] {},
            new int[][] {});

        // Case 3: STABILITY — duplicate (Y,X,Z) keys within one list. The three
        // entries at (4,4,4) must keep their input order (ids in increasing order),
        // which only a stable sort preserves.
        emitCase(caseId++,
            new int[][] {
                {4, 4, 4},
                {1, 1, 1},
                {4, 4, 4},
                {4, 4, 4},
                {1, 1, 1},
            },
            new int[][] {},
            new int[][] {});

        // Case 4: all three lists non-empty and individually scrambled, so the
        // full+other+blockEntities concat AND each per-list sort are both checked.
        emitCase(caseId++,
            new int[][] {{2, 3, 1}, {2, 1, 1}, {2, 3, 0}},                  // full
            new int[][] {{-5, 0, 0}, {7, 0, 0}, {0, 0, 0}},                 // blockEntities
            new int[][] {{1, 9, 9}, {1, 9, 8}, {1, 8, 9}, {0, 9, 9}});      // other

        // Case 5: negative coordinates and signed ordering (negative Y sorts below
        // positive Y; negative X/Z tie-breaks signed).
        emitCase(caseId++,
            new int[][] {{0, 1, 0}, {0, -1, 0}, {-3, -1, -3}, {-3, -1, 2}, {5, -1, -8}},
            new int[][] {},
            new int[][] {});

        // Case 6: Integer.MIN/MAX boundary coordinates feed the signed comparator
        // at its extremes (MIN sorts first, MAX last) — verifies no unsigned slip.
        int MIN = Integer.MIN_VALUE, MAX = Integer.MAX_VALUE;
        emitCase(caseId++,
            new int[][] {
                {0, MAX, 0},
                {0, MIN, 0},
                {MIN, 0, MAX},
                {MAX, 0, MIN},
                {0, 0, MIN},
                {0, 0, MAX},
                {MIN, 0, MIN},
            },
            new int[][] {{0, MIN, 0}, {0, MAX, 0}},
            new int[][] {{MAX, MAX, MAX}, {MIN, MIN, MIN}, {0, 0, 0}});

        // Case 7: larger pseudo-random battery to stress stability + ordering over
        // many ties; coordinates drawn from a small range so duplicates are common.
        java.util.Random rng = new java.util.Random(0x5DEECE66DL);
        int[][] fullR = randBlocks(rng, 60, 4);
        int[][] beR = randBlocks(rng, 30, 4);
        int[][] otherR = randBlocks(rng, 45, 4);
        emitCase(caseId++, fullR, beR, otherR);

        // Case 8: another random battery with a wider coordinate range.
        int[][] fullR2 = randBlocks(rng, 80, 12);
        int[][] beR2 = randBlocks(rng, 20, 12);
        int[][] otherR2 = randBlocks(rng, 50, 12);
        emitCase(caseId++, fullR2, beR2, otherR2);
    }

    // n blocks with each coordinate in [-range, range] (so ties are frequent).
    static int[][] randBlocks(java.util.Random rng, int n, int range) {
        int span = 2 * range + 1;
        int[][] out = new int[n][3];
        for (int i = 0; i < n; i++) {
            out[i][0] = rng.nextInt(span) - range;
            out[i][1] = rng.nextInt(span) - range;
            out[i][2] = rng.nextInt(span) - range;
        }
        return out;
    }
}

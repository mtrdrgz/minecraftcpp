// Ground-truth generator for the PURE cluster-ruin placement geometry in the REAL
// decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.OceanRuinPieces
//
// Covered REAL methods (never reimplemented on the Java side — always invoked):
//   (A) OceanRuinPieces.allPositions(RandomSource, BlockPos)   [private static]
//         The 8 candidate cluster offsets. We drive it through a RECORDING
//         RandomSource proxy that delegates to the real SingleThreadedRandomSource
//         and logs every nextInt(bound) return — those 16 raw draws are emitted so
//         the C++ port can rebuild the positions and we compare against the REAL
//         returned BlockPos list bit-for-bit.
//   (B) StructureTemplate.transform(BlockPos, Mirror, Rotation, BlockPos)
//       Vec3i.offset(int,int,int)
//       BoundingBox.fromCorners(Vec3i, Vec3i)
//       BoundingBox.intersects(BoundingBox)
//         The parent-box + per-candidate fit test that addClusterRuins performs
//         (OceanRuinPieces.java:176-192), assembled from the REAL primitives.
//
//   tools/run_groundtruth.ps1 -Tool OceanRuinClusterParity -Out mcpp/build/ocean_ruin_cluster.tsv
//
// TSV rows (all ints decimal; leading TAG):
//   ALLPOS  ox oy oz seed  r0..r15  | b0x b0y b0z ... b7x b7y b7z
//           -- origin (ox,oy,oz); seed for the recording RandomSource; r0..r15 the
//              16 raw random.nextInt(bound) returns in draw order; then the 8 REAL
//              result BlockPos (3 ints each).
//   PARENT  px pz rot  | parentCornerX parentCornerY parentCornerZ
//                        parentMinX parentMinY parentMinZ parentMaxX parentMaxY parentMaxZ
//                        botLX botLY botLZ
//           -- parentBox geometry for piece origin (px,?,pz)+y90 and rotation rot.
//   FIT     posX posY posZ nextRot  pMinX pMinY pMinZ pMaxX pMaxY pMaxZ
//                        | nextCornerX nextCornerY nextCornerZ
//                          nextMinX nextMinY nextMinZ nextMaxX nextMaxY nextMaxZ fits
//           -- candidate-fit test: nextCorner/ nextBB and whether it clears the
//              given parent BB (parent bb fed explicitly to decouple from PARENT).

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.List;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;

public class OceanRuinClusterParity {
    static final java.io.PrintStream OUT = System.out;
    static Method M_allPositions;

    // Records every nextInt(bound) return from the delegate, in call order.
    static final class Recorder {
        final java.util.ArrayList<Integer> draws = new java.util.ArrayList<>();
    }

    // A RandomSource that delegates to a real one but records nextInt(int) results.
    static RandomSource recordingRandom(final RandomSource delegate, final Recorder rec) {
        return (RandomSource) Proxy.newProxyInstance(
            RandomSource.class.getClassLoader(),
            new Class<?>[]{RandomSource.class},
            new InvocationHandler() {
                @Override
                public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
                    Object result = method.invoke(delegate, args);
                    if (method.getName().equals("nextInt")
                        && method.getParameterCount() == 1
                        && method.getParameterTypes()[0] == int.class) {
                        rec.draws.add((Integer) result);
                    }
                    return result;
                }
            });
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> cls = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.OceanRuinPieces");
        M_allPositions = cls.getDeclaredMethod("allPositions", RandomSource.class, BlockPos.class);
        M_allPositions.setAccessible(true);

        // ---- (A) allPositions battery -----------------------------------------
        // origins: the call site passes parentBottomLeft (a small-magnitude pos),
        // but the math is pure int offset, so probe origin and a wide spread.
        int[][] origins = {
            {0, 0, 0},
            {1, 90, 1}, {-1, 90, -1}, {7, 64, 11}, {-8, 70, -13},
            {16, 0, 16}, {-16, 0, -16}, {100, -64, -100},
            {123456, 200, -654321},
            {2000000000, 0, -2000000000}, {-2000000000, 0, 2000000000},
        };
        // seeds: a spread that exercises all 16 draws across their varied bounds.
        long[] seeds = {
            0L, 1L, -1L, 2L, 42L, 1234L, -9999L, 9007199254740991L,
            123456789L, -123456789L, 0x5DEECE66DL, 987654321987654321L,
            -8888888888888888L, 7L, 13L, 99L, 555L, -555L, 314159L, 271828L,
        };
        for (int[] o : origins) {
            for (long seed : seeds) {
                emitAllPos(o[0], o[1], o[2], seed);
            }
        }

        // ---- (B) parentBox + candidate fit batteries --------------------------
        int[][] pxz = {
            {0, 0}, {1, 1}, {-1, -1}, {7, 11}, {-8, -13},
            {16, 16}, {-16, -16}, {100, -100},
            {123456, -654321}, {2000000000, -2000000000}, {-2000000000, 2000000000},
        };
        Rotation[] rots = Rotation.values();
        for (int[] pp : pxz) {
            for (Rotation r : rots) {
                emitParent(pp[0], pp[1], r);
            }
        }

        // candidate-fit: place pos near and far from a parent BB, all rotations.
        // Use a representative parent BB and a battery of candidate positions.
        int[][] candPos = {
            {0, 90, 0}, {3, 90, 3}, {20, 90, 20}, {-20, 90, -20},
            {6, 90, 7}, {1, 0, 1}, {-30, 90, 40}, {1000, 90, -1000},
            {15, 90, 15}, {16, 90, 16}, {21, 90, 22},
            {2000000000, 90, -2000000000},
        };
        // Several parent BBs (min..max) to vary the intersect outcome.
        int[][] parentBBs = {
            {0, 90, 0, 15, 90, 15},
            {-15, 90, -15, 0, 90, 0},
            {10, 88, 10, 25, 92, 25},
            {-2000000000, 0, -2000000000, 2000000000, 255, 2000000000},
            {5, 90, 6, 5, 90, 6},
        };
        for (int[] bb : parentBBs) {
            BoundingBox parent = new BoundingBox(bb[0], bb[1], bb[2], bb[3], bb[4], bb[5]);
            for (int[] cp : candPos) {
                for (Rotation r : rots) {
                    emitFit(cp[0], cp[1], cp[2], r, parent);
                }
            }
        }

        OUT.flush();
    }

    @SuppressWarnings("unchecked")
    static void emitAllPos(int ox, int oy, int oz, long seed) throws Exception {
        Recorder rec = new Recorder();
        RandomSource real = RandomSource.create(seed);
        RandomSource rng = recordingRandom(real, rec);
        List<BlockPos> result = (List<BlockPos>) M_allPositions.invoke(null, rng, new BlockPos(ox, oy, oz));
        if (rec.draws.size() != 16 || result.size() != 8) {
            // Should never happen for the real method; skip rather than corrupt.
            return;
        }
        StringBuilder sb = new StringBuilder();
        sb.append("ALLPOS\t").append(ox).append('\t').append(oy).append('\t').append(oz)
          .append('\t').append(seed);
        for (int i = 0; i < 16; i++) sb.append('\t').append(rec.draws.get(i));
        for (BlockPos b : result) {
            sb.append('\t').append(b.getX()).append('\t').append(b.getY()).append('\t').append(b.getZ());
        }
        OUT.println(sb.toString());
    }

    static void emitParent(int px, int pz, Rotation rot) {
        // OceanRuinPieces.java:176-181 assembled from REAL primitives.
        BlockPos parentPos = new BlockPos(px, 90, pz);
        BlockPos parentCorner =
            StructureTemplate.transform(new BlockPos(15, 0, 15), Mirror.NONE, rot, BlockPos.ZERO)
                .offset(parentPos);
        BoundingBox parentBB = BoundingBox.fromCorners(parentPos, parentCorner);
        BlockPos parentBottomLeft = new BlockPos(
            Math.min(parentPos.getX(), parentCorner.getX()), parentPos.getY(),
            Math.min(parentPos.getZ(), parentCorner.getZ()));
        OUT.println("PARENT\t" + px + "\t" + pz + "\t" + rot.ordinal()
            + "\t" + parentCorner.getX() + "\t" + parentCorner.getY() + "\t" + parentCorner.getZ()
            + "\t" + parentBB.minX() + "\t" + parentBB.minY() + "\t" + parentBB.minZ()
            + "\t" + parentBB.maxX() + "\t" + parentBB.maxY() + "\t" + parentBB.maxZ()
            + "\t" + parentBottomLeft.getX() + "\t" + parentBottomLeft.getY() + "\t" + parentBottomLeft.getZ());
    }

    static void emitFit(int x, int y, int z, Rotation nextRot, BoundingBox parentBB) {
        // OceanRuinPieces.java:190-192 assembled from REAL primitives.
        BlockPos pos = new BlockPos(x, y, z);
        BlockPos nextCorner =
            StructureTemplate.transform(new BlockPos(5, 0, 6), Mirror.NONE, nextRot, BlockPos.ZERO)
                .offset(pos);
        BoundingBox nextBB = BoundingBox.fromCorners(pos, nextCorner);
        boolean fits = !nextBB.intersects(parentBB);
        OUT.println("FIT\t" + x + "\t" + y + "\t" + z + "\t" + nextRot.ordinal()
            + "\t" + parentBB.minX() + "\t" + parentBB.minY() + "\t" + parentBB.minZ()
            + "\t" + parentBB.maxX() + "\t" + parentBB.maxY() + "\t" + parentBB.maxZ()
            + "\t" + nextCorner.getX() + "\t" + nextCorner.getY() + "\t" + nextCorner.getZ()
            + "\t" + nextBB.minX() + "\t" + nextBB.minY() + "\t" + nextBB.minZ()
            + "\t" + nextBB.maxX() + "\t" + nextBB.maxY() + "\t" + nextBB.maxZ()
            + "\t" + (fits ? 1 : 0));
    }
}

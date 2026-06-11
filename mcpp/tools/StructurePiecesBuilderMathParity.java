// Ground-truth generator for the PURE vertical-placement integer math of
//   net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder (26.1.2):
//     offsetPiecesVertically(int dy)
//     moveBelowSeaLevel(int seaLevel, int minY, RandomSource random, int offset) -> int dy
//     moveInsideHeights(RandomSource random, int lowestAllowed, int highestAllowed) -> void
//
//   tools/run_groundtruth.ps1 -Tool StructurePiecesBuilderMathParity -Out mcpp/build/structure_pieces_builder_math.tsv
//
// HOW THIS DRIVES THE REAL CLASS (no math reimplemented Java-side):
//   * The pieces list is a List<StructurePiece> of TestPiece instances. The
//     StructurePiece ctor merely stores (type,genDepth,boundingBox) — a null type is
//     fine (never dereferenced by the math), so `new TestPiece(bb)` works after
//     Bootstrap (StructurePiece's static SHAPE_CHECK_BLOCKS init needs Blocks).
//     getBoundingBox()/move() only touch `boundingBox` — both faithful.
//   * The real StructurePiecesBuilder is constructed normally; its private `pieces`
//     field is replaced (reflection) with our TestPiece list, then the REAL
//     offsetPiecesVertically / moveBelowSeaLevel / moveInsideHeights run.
//   * RandomSource is a java.lang.reflect.Proxy over a real RandomSource.create(seed):
//     it delegates every call and RECORDS each nextInt(int) (bound,result). The math
//     draws at most one nextInt per call (under a guard that makes bound>0). The C++
//     test replays the recorded draw, so the integer math is verified independently of
//     RNG-stream certification (which is gilded elsewhere).
//
// TSV rows (whitespace-separated), dispatched by leading TAG in the C++ test:
//   OFFSET   <nb> <6*nb input> <dy> <6*nb result>
//   BELOWSEA <nb> <6*nb input> <seaLevel> <minY> <offset> <ndraw> [draw] <retDy> <6*nb result>
//   INSIDE   <nb> <6*nb input> <lowest> <highest> <ndraw> [draw] <dy> <6*nb result>

import java.lang.reflect.*;
import java.util.*;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.core.BlockPos;

public class StructurePiecesBuilderMathParity {
    static final java.io.PrintStream O = System.out;
    static Field PIECES_FIELD;     // StructurePiecesBuilder.pieces

    // Concrete StructurePiece: getBoundingBox()/move() only touch boundingBox; the
    // abstract methods are never called by the math under test. Null type is never
    // dereferenced; the ctor merely stores its three args.
    public static final class TestPiece extends StructurePiece {
        private TestPiece(BoundingBox bb) { super(null, 0, bb); }
        protected void addAdditionalSaveData(StructurePieceSerializationContext c, CompoundTag t) {}
        public void postProcess(WorldGenLevel l, StructureManager s, ChunkGenerator g, RandomSource r,
                                BoundingBox b, ChunkPos cp, BlockPos rp) {}
    }

    static StructurePiece makePiece(int[] box) throws Exception {
        return new TestPiece(new BoundingBox(box[0], box[1], box[2], box[3], box[4], box[5]));
    }

    @SuppressWarnings("unchecked")
    static StructurePiecesBuilder makeBuilder(List<int[]> boxes) throws Exception {
        StructurePiecesBuilder b = new StructurePiecesBuilder();
        List<StructurePiece> list = (List<StructurePiece>) PIECES_FIELD.get(b);
        list.clear();
        for (int[] box : boxes) list.add(makePiece(box));
        return b;
    }

    // RandomSource proxy: delegates to a real source, records nextInt(int) draws.
    static RandomSource recordingRandom(long seed, final List<Integer> recorded) {
        final RandomSource real = RandomSource.create(seed);
        return (RandomSource) Proxy.newProxyInstance(
            RandomSource.class.getClassLoader(),
            new Class[]{RandomSource.class},
            (proxy, method, args) -> {
                final Object res;
                try {
                    res = method.invoke(real, args);
                } catch (InvocationTargetException e) {
                    // Unwrap so nextInt(bound<=0)'s IllegalArgumentException (overflow
                    // param combos the REAL game would also crash on) reaches the
                    // per-case catch and skips the row, instead of an UndeclaredThrowable.
                    throw e.getCause();
                }
                if (method.getName().equals("nextInt") && args != null && args.length == 1) {
                    recorded.add((Integer) res);
                }
                return res;
            });
    }

    static String boxesToString(List<StructurePiece> pieces) {
        StringBuilder sb = new StringBuilder();
        for (StructurePiece p : pieces) {
            BoundingBox bb = p.getBoundingBox();
            sb.append(' ').append(bb.minX()).append(' ').append(bb.minY()).append(' ').append(bb.minZ())
              .append(' ').append(bb.maxX()).append(' ').append(bb.maxY()).append(' ').append(bb.maxZ());
        }
        return sb.toString();
    }

    static String inputToString(List<int[]> boxes) {
        StringBuilder sb = new StringBuilder();
        for (int[] b : boxes)
            for (int v : b) sb.append(' ').append(v);
        return sb.toString();
    }

    @SuppressWarnings({"deprecation", "unchecked"})  // @Deprecated movers + unchecked reflective list casts
    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) { /* not required for this pure math */ }

        PIECES_FIELD = StructurePiecesBuilder.class.getDeclaredField("pieces");
        PIECES_FIELD.setAccessible(true);

        // Box configurations (each list of valid min<=max boxes). Chosen so aggregate
        // minY/maxY/getYSpan differ across configs and exercise multi-piece encapsulation.
        List<List<int[]>> configs = new ArrayList<>();
        configs.add(Arrays.asList(new int[]{0, 64, 0, 10, 80, 10}));                       // ySpan 17
        configs.add(Arrays.asList(new int[]{-5, -64, -5, 5, -40, 5}));                      // negative Y, ySpan 25
        configs.add(Arrays.asList(new int[]{0, 0, 0, 0, 0, 0}));                            // degenerate ySpan 1
        configs.add(Arrays.asList(new int[]{-20, 30, -20, 20, 200, 20}));                   // tall ySpan 171
        configs.add(Arrays.asList(new int[]{0, 64, 0, 4, 70, 4}, new int[]{8, 50, 8, 12, 90, 12})); // two boxes -> agg minY50 maxY90
        configs.add(Arrays.asList(new int[]{-100, -200, -100, -90, -150, -90},
                                  new int[]{50, 100, 50, 60, 300, 60}));                    // two, wide aggregate

        // Parameter batteries — include both branches and large magnitudes for wrap.
        int[] seaLevels = {63, 0, -64, 320, 100000, -100000, Integer.MAX_VALUE - 5, Integer.MIN_VALUE + 5};
        int[] minYs     = {0, -64, 64, 1, -2000000000, 2000000000};
        int[] offsets   = {0, 1, 7, -7, 50, 1000000000, -1000000000};
        int[] lowests   = {-64, 0, 60, -2000000000, 1};
        int[] highests  = {320, 100, 60, 2000000000, 64, -2000000000};
        int[] dys        = {0, 1, -1, 5, -5, 100, -100, 1000000000, -1000000000,
                            Integer.MAX_VALUE, Integer.MIN_VALUE};

        long seedCounter = 0x9E3779B97F4A7C15L;

        for (List<int[]> cfg : configs) {
            // OFFSET
            for (int dy : dys) {
                StructurePiecesBuilder b = makeBuilder(cfg);
                List<StructurePiece> pieces = (List<StructurePiece>) PIECES_FIELD.get(b);
                b.offsetPiecesVertically(dy);
                O.println("OFFSET " + cfg.size() + inputToString(cfg) + " " + dy + boxesToString(pieces));
            }
            // BELOWSEA
            for (int seaLevel : seaLevels)
                for (int minY : minYs)
                    for (int offset : offsets) {
                        List<Integer> rec = new ArrayList<>();
                        RandomSource rnd = recordingRandom(seedCounter++, rec);
                        StructurePiecesBuilder b = makeBuilder(cfg);
                        List<StructurePiece> pieces = (List<StructurePiece>) PIECES_FIELD.get(b);
                        int dy;
                        try {
                            dy = b.moveBelowSeaLevel(seaLevel, minY, rnd, offset);
                        } catch (IllegalArgumentException ex) {
                            // nextInt(bound<=0) — guard should prevent; skip if it ever happens.
                            continue;
                        }
                        StringBuilder draws = new StringBuilder();
                        for (int d : rec) draws.append(' ').append(d);
                        O.println("BELOWSEA " + cfg.size() + inputToString(cfg)
                                + " " + seaLevel + " " + minY + " " + offset
                                + " " + rec.size() + draws + " " + dy + boxesToString(pieces));
                    }
            // INSIDE
            for (int lowest : lowests)
                for (int highest : highests) {
                    List<Integer> rec = new ArrayList<>();
                    RandomSource rnd = recordingRandom(seedCounter++, rec);
                    StructurePiecesBuilder b = makeBuilder(cfg);
                    List<StructurePiece> pieces = (List<StructurePiece>) PIECES_FIELD.get(b);
                    // capture pre-move minY of first piece to derive dy (Java returns void)
                    int preMinY = pieces.get(0).getBoundingBox().minY();
                    try {
                        b.moveInsideHeights(rnd, lowest, highest);
                    } catch (IllegalArgumentException ex) {
                        continue;
                    }
                    int dy = pieces.get(0).getBoundingBox().minY() - preMinY;
                    StringBuilder draws = new StringBuilder();
                    for (int d : rec) draws.append(' ').append(d);
                    O.println("INSIDE " + cfg.size() + inputToString(cfg)
                            + " " + lowest + " " + highest
                            + " " + rec.size() + draws + " " + dy + boxesToString(pieces));
                }
        }
    }
}

// Reference generator for the C++ net.minecraft.world.level.levelgen.VerticalAnchor
// port. Runs against the REAL decompiled 26.1.2 classes (bootstrapped) and emits the
// resolved Y for each (anchor kind, parameter, resolved vertical bounds) combination.
//
//   javac -cp 26.1.2/client.jar:26.1.2/libs/* -d <out> mcpp/tools/VerticalAnchorParity.java
//   java  -cp <out>:26.1.2/client.jar:26.1.2/libs/* VerticalAnchorParity vertical_anchor.tsv
//
// VerticalAnchor.{Absolute,AboveBottom,BelowTop}.resolveY(WorldGenerationContext) are pure
// int math reading ONLY the context's two resolved fields getMinGenY()/getGenDepth()
// (VerticalAnchor.java):
//   Absolute.resolveY     (:71-73) = this.y
//   AboveBottom.resolveY  (:54-56) = heightAccessor.getMinGenY() + this.offset
//   BelowTop.resolveY     (:88-90) = heightAccessor.getGenDepth() - 1 + heightAccessor.getMinGenY() - this.offset
//
// Driving the REAL resolveY needs a real WorldGenerationContext, whose ONLY constructor is
// WorldGenerationContext(ChunkGenerator, LevelHeightAccessor) (WorldGenerationContext.java:10),
// with minY=max(heightAccessor.getMinY(), generator.getMinY()) and
// height=min(heightAccessor.getHeight(), generator.getGenDepth()). ChunkGenerator is abstract
// (BiomeSource-bearing ctor + 11 abstract methods) and allocating WorldGenerationContext
// without a ctor needs sun.misc.Unsafe / sun.reflect.ReflectionFactory, whose "internal
// proprietary API" javac warning trips the strict ground-truth runner. Per the repo precedent
// for impractical real calls (ExplosionMathParity, Entity.calculateViewVector), we EMIT the
// result by replicating the three one-line resolveY bodies VERBATIM against the resolved
// (minY, height) pair that the C++ WorldGenerationContext(minGenY, genDepth) two-int ctor takes
// directly; the independently-ported C++ resolveY is the cross-check. The (minY,height) used
// here ARE the resolved fields, so the constructor's max/min is identity for these inputs.
//
// Rows are tab-separated: ANCHOR <kind> <param> <minGenY> <genDepth> <resolvedY>
// All integers decimal.

public class VerticalAnchorParity {
    static final java.io.PrintStream O = System.out;

    // --- verbatim replicas of VerticalAnchor.*.resolveY (see header citations) ---
    static int resolveAbsolute(int y) { return y; }                                  // :73
    static int resolveAboveBottom(int minGenY, int offset) { return minGenY + offset; } // :56
    static int resolveBelowTop(int minGenY, int genDepth, int offset) {              // :90
        return genDepth - 1 + minGenY - offset;
    }

    public static void main(String[] args) throws Exception {
        // Bootstrap so the real client.jar classes (and version) are confirmed loadable;
        // resolveY itself is pure int math (replicated above) and needs no registries.
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // FINITE / PHYSICAL contexts: real overworld/nether/end-ish vertical bounds plus a
        // few extra resolved (minGenY, genDepth) pairs. These are the values produced by
        // WorldGenerationContext = (max(minY), min(height)).
        int[][] ctxs = {
            { -64, 384 },  // overworld
            { 0, 256 },    // legacy / nether
            { 0, 128 },    // nether logical-height-ish
            { -64, 320 },  // clamped height
            { -32, 192 },
            { 16, 64 },
            { -2032, 4064 } // codec extreme bounds [MIN_Y .. Y_SIZE]
        };

        // Codec int range for absolute(y) / aboveBottom(offset) / belowTop(offset)
        // is [DimensionType.MIN_Y, DimensionType.MAX_Y] = [-2032, 2031]. We sample
        // realistic and boundary parameters across that legal range.
        int[] params = { -2032, -64, -1, 0, 1, 10, 16, 50, 64, 100, 256, 319, 320, 383, 2031 };

        // Emit rows to STDOUT (run_groundtruth.ps1 captures stdout into the .tsv; it passes
        // no args). Only ANCHOR rows — no status line, which the C++ test would read as an
        // unknown-tag mismatch.
        for (int[] cd : ctxs) {
            int minY = cd[0];
            int height = cd[1];
            for (int p : params) {
                O.println("ANCHOR\tabsolute\t" + p + "\t" + minY + "\t" + height + "\t" + resolveAbsolute(p));
                O.println("ANCHOR\taboveBottom\t" + p + "\t" + minY + "\t" + height + "\t" + resolveAboveBottom(minY, p));
                O.println("ANCHOR\tbelowTop\t" + p + "\t" + minY + "\t" + height + "\t" + resolveBelowTop(minY, height, p));
            }
            // bottom() == aboveBottom(0), top() == belowTop(0)
            O.println("ANCHOR\tbottom\t0\t" + minY + "\t" + height + "\t" + resolveAboveBottom(minY, 0));
            O.println("ANCHOR\ttop\t0\t" + minY + "\t" + height + "\t" + resolveBelowTop(minY, height, 0));
        }
    }
}

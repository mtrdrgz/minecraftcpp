// Reference value generator for the C++ RandomBlockMatchTest port
//   (mcpp/src/world/level/levelgen/structure/templatesystem/RandomBlockMatchTest.h).
//
// Drives the REAL decompiled class
//   net.minecraft.world.level.levelgen.structure.templatesystem.RandomBlockMatchTest
// from client.jar. Its behavioural method is:
//
//   public boolean test(BlockState blockState, RandomSource random) {
//      return blockState.is(this.block) && random.nextFloat() < this.probability;
//   }
//
// For each case the driver:
//   1. picks two real BuiltInRegistries.BLOCK blocks (test-block and state-block)
//      by index into a fixed name list — block identity drives `state.is(block)`,
//   2. seeds RandomSource.create(seed) (a LegacyRandomSource, the production type),
//   3. calls the REAL test() and records the boolean result,
//   4. draws one more random.nextLong() and records it. That post-call draw value
//      is the witness for the && short-circuit: when the block does NOT match,
//      test() consumes ZERO floats so the next draw differs from the matching case.
//      A port that wrongly always drew nextFloat() would change this value.
//
// The C++ side certifies the predicate LOGIC (identity equality + nextFloat<p +
// short-circuit), not the registry table: it receives the two block ids as ints.
// Block ids here are the fixed list indices (0-based), shared with the C++ test.
//
//   javac -cp 26.1.2/client.jar;26.1.2/libs/* -d <out> RandomBlockMatchTestParity.java
//   java  -cp <out>;26.1.2/client.jar;26.1.2/libs/* RandomBlockMatchTestParity > rbmt.tsv
//
// Rows (tab-separated):
//   RBMT  <seed>  <stateBlockIdx>  <testBlockIdx>  <probBits>  <result0|1>  <afterLong>
//     probBits  = Float.floatToRawIntBits(probability)
//     afterLong = random.nextLong() drawn immediately after test()
//
// O is captured at class load so any bootstrap chatter on stdout stays out of the TSV.

import net.minecraft.SharedConstants;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.server.Bootstrap;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.structure.templatesystem.RandomBlockMatchTest;

public class RandomBlockMatchTestParity {
    static final java.io.PrintStream O = System.out;

    // Fixed list of distinct real blocks. The TSV index column maps into this
    // list; the same index ordering is mirrored as opaque ids on the C++ side.
    static final String[] BLOCK_NAMES = {
        "stone", "dirt", "grass_block", "cobblestone", "oak_planks",
        "sand", "gravel", "gold_ore", "iron_ore", "coal_ore",
        "air", "water", "lava", "bedrock", "obsidian", "diamond_ore",
    };

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        Block[] blocks = new Block[BLOCK_NAMES.length];
        BlockState[] states = new BlockState[BLOCK_NAMES.length];
        for (int i = 0; i < BLOCK_NAMES.length; i++) {
            Block b = BuiltInRegistries.BLOCK.getValue(Identifier.withDefaultNamespace(BLOCK_NAMES[i]));
            blocks[i] = b;
            states[i] = b.defaultBlockState();
        }

        // Probabilities: boundary values (never/always draw), exact-representable
        // fractions, and a value that exercises the strict `<` near a draw.
        float[] probs = {
            0.0f, 1.0f, 0.5f, 0.25f, 0.75f, 0.1f, 0.9f,
            0.0078125f,            // 1/128, exactly representable
            0.99999994f,           // just below 1 (largest float < 1)
            -1.0f,                 // negative: nextFloat() (in [0,1)) is never < p
            2.0f,                  // >1: any draw < p once block matches
        };

        long[] seeds = {
            0L, 1L, 2L, 42L, 7L, 123456789L,
            -1L, -987654321L, 2147483647L, -2147483648L,
            1234567890123456789L, -1234567890123456789L, 8675309L,
        };

        for (long seed : seeds) {
            for (int stateIdx = 0; stateIdx < blocks.length; stateIdx++) {
                for (int testIdx = 0; testIdx < blocks.length; testIdx++) {
                    for (float p : probs) {
                        RandomBlockMatchTest rule = new RandomBlockMatchTest(blocks[testIdx], p);
                        RandomSource random = RandomSource.create(seed);
                        boolean result = rule.test(states[stateIdx], random);
                        long after = random.nextLong();
                        O.println("RBMT\t" + seed + "\t" + stateIdx + "\t" + testIdx
                                  + "\t" + Float.floatToRawIntBits(p)
                                  + "\t" + (result ? 1 : 0)
                                  + "\t" + after);
                    }
                }
            }
        }
    }
}

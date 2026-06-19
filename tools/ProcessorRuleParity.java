// Reference value generator for the C++ ProcessorRule port
//   (mcpp/src/world/level/levelgen/structure/templatesystem/ProcessorRule.h).
//
// Drives the REAL decompiled class
//   net.minecraft.world.level.levelgen.structure.templatesystem.ProcessorRule
// from client.jar. Its sole behavioural method is the three-armed short-circuit
// AND (ProcessorRule.java:53):
//
//   public boolean test(BlockState inputState, BlockState locState,
//                       BlockPos inTemplatePos, BlockPos worldPos,
//                       BlockPos reference, RandomSource random) {
//      return this.inputPredicate.test(inputState, random)
//         && this.locPredicate.test(locState, random)
//         && this.posPredicate.test(inTemplatePos, worldPos, reference, random);
//   }
//
// The rule is built from REAL sub-predicates:
//   * inputPredicate, locPredicate = real RandomBlockStateMatchTest
//         (state == this.blockState && random.nextFloat() < this.probability)
//   * posPredicate = real PosAlwaysTrueTest (always true, draws nothing)
// so this.test() exercises the genuine vanilla `&&` short-circuit across the
// two RNG-driven RuleTest arms plus the pass-through PosRuleTest arm.
//
// For each case the driver:
//   1. picks input/loc BlockStates (the values handed to test) and the two
//      RandomBlockStateMatchTest target states, all from a fixed list of real
//      BuiltInRegistries.BLOCK default states — `==` over these interned
//      singletons is genuine vanilla BlockState identity,
//   2. seeds RandomSource.create(seed) (a LegacyRandomSource, the production type),
//   3. calls the REAL ProcessorRule.test() and records the boolean result,
//   4. draws one more random.nextLong(). That post-call draw is the witness for
//      the && short-circuit: arm1 draws iff its state matches; arm2 draws iff
//      arm1 passed AND arm2's state matches; arm3 (PosAlwaysTrueTest) never draws.
//      A port that drew the wrong number of floats would change this value.
//
// The C++ side certifies the COMPOSITE logic (the two RandomBlockStateMatchTest
// arms + PosAlwaysTrueTest, chained by &&), not the registry table: it receives
// state ids as ints (fixed list indices, shared with the C++ test).
//
//   javac -cp 26.1.2/client.jar;26.1.2/libs/* -d <out> ProcessorRuleParity.java
//   java  -cp <out>;26.1.2/client.jar;26.1.2/libs/* ProcessorRuleParity > pr.tsv
//
// Rows (tab-separated):
//   PR  <seed>  <inIdx>  <locIdx>  <inTgtIdx>  <inProbBits>  <locTgtIdx>  <locProbBits>  <result0|1>  <afterLong>
//     inProbBits/locProbBits = Float.floatToRawIntBits(probability)
//     afterLong              = random.nextLong() drawn immediately after test()
//
// O is captured at class load so any bootstrap chatter on stdout stays out of the TSV.

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.server.Bootstrap;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.structure.templatesystem.PosAlwaysTrueTest;
import net.minecraft.world.level.levelgen.structure.templatesystem.ProcessorRule;
import net.minecraft.world.level.levelgen.structure.templatesystem.RandomBlockStateMatchTest;

public class ProcessorRuleParity {
    static final java.io.PrintStream O = System.out;

    // Fixed list of distinct real blocks. The TSV index columns map into the
    // default states of this list; the same index ordering is mirrored as opaque
    // state ids on the C++ side.
    static final String[] BLOCK_NAMES = {
        "stone", "dirt", "cobblestone", "oak_planks",
    };

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        BlockState[] states = new BlockState[BLOCK_NAMES.length];
        for (int i = 0; i < BLOCK_NAMES.length; i++) {
            Block b = BuiltInRegistries.BLOCK.getValue(Identifier.withDefaultNamespace(BLOCK_NAMES[i]));
            states[i] = b.defaultBlockState();
        }

        // Probabilities: boundary values (never/always draw), exact fractions, and
        // a value exercising the strict `<` near a draw.
        float[] probs = {
            0.0f, 0.5f,
            0.99999994f,  // largest float < 1
            -1.0f,        // negative: nextFloat() (in [0,1)) is never < p
            2.0f,         // >1: any draw < p once the state matches
            1.0f,         // boundary: nextFloat() (in [0,1)) is always < 1
        };

        long[] seeds = {
            0L, 1L, 42L, 7L, 123456789L,
            -1L, 2147483647L, -2147483648L,
            1234567890123456789L, -1234567890123456789L,
        };

        // Position predicate is the default PosAlwaysTrueTest (draws nothing).
        PosAlwaysTrueTest posPredicate = PosAlwaysTrueTest.INSTANCE;
        // Position arguments are irrelevant to PosAlwaysTrueTest; use a fixed one.
        BlockPos inTemplatePos = new BlockPos(1, 2, 3);
        BlockPos worldPos = new BlockPos(4, 5, 6);
        BlockPos reference = new BlockPos(7, 8, 9);
        // outputState is stored by ProcessorRule but never read by test(); fix it.
        BlockState outputState = states[0];

        int n = BLOCK_NAMES.length;
        for (long seed : seeds) {
            for (int inIdx = 0; inIdx < n; inIdx++) {
                for (int locIdx = 0; locIdx < n; locIdx++) {
                    for (int inTgt = 0; inTgt < n; inTgt++) {
                        for (int locTgt = 0; locTgt < n; locTgt++) {
                            for (float inProb : probs) {
                                for (float locProb : probs) {
                                    RandomBlockStateMatchTest inputPredicate =
                                        new RandomBlockStateMatchTest(states[inTgt], inProb);
                                    RandomBlockStateMatchTest locPredicate =
                                        new RandomBlockStateMatchTest(states[locTgt], locProb);
                                    ProcessorRule rule = new ProcessorRule(
                                        inputPredicate, locPredicate, posPredicate, outputState);

                                    RandomSource random = RandomSource.create(seed);
                                    boolean result = rule.test(
                                        states[inIdx], states[locIdx],
                                        inTemplatePos, worldPos, reference, random);
                                    long after = random.nextLong();

                                    O.println("PR\t" + seed
                                        + "\t" + inIdx + "\t" + locIdx
                                        + "\t" + inTgt + "\t" + Float.floatToRawIntBits(inProb)
                                        + "\t" + locTgt + "\t" + Float.floatToRawIntBits(locProb)
                                        + "\t" + (result ? 1 : 0)
                                        + "\t" + after);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

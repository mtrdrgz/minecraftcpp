#pragma once

// 1:1 port of
//   net.minecraft.world.level.levelgen.structure.templatesystem.ProcessorRule
//
// ProcessorRule is the per-rule predicate that drives RuleProcessor: for each
// block the template wants to place it asks whether the rule fires. Its sole
// behavioural method is a three-armed short-circuit AND (ProcessorRule.java:53):
//
//   public boolean test(BlockState inputState, BlockState locState,
//                       BlockPos inTemplatePos, BlockPos worldPos,
//                       BlockPos reference, RandomSource random) {
//      return this.inputPredicate.test(inputState, random)
//         && this.locPredicate.test(locState, random)
//         && this.posPredicate.test(inTemplatePos, worldPos, reference, random);
//   }
//
// The whole gate is the short-circuit RNG accounting:
//   * inputPredicate is always evaluated.
//   * locPredicate is evaluated ONLY if inputPredicate returned true.
//   * posPredicate is evaluated ONLY if both prior arms returned true.
// Any RNG-driven sub-predicate (e.g. RandomBlockStateMatchTest) that is skipped
// by the `&&` consumes ZERO draws. A port that eagerly evaluated all three arms
// would desync every downstream feature placement.
//
// This header certifies the COMPOSITE logic, not the sub-predicate registry or
// the BlockState/BlockPos value types. The sub-predicates are modelled exactly
// as the ungated certified siblings:
//   * the two RuleTest arms as RandomBlockStateMatchTest:
//         state == blockState && random.nextFloat() < probability
//     (RandomBlockStateMatchTest.java:24) — BlockState reference identity is an
//     opaque integer state id, supplied by the Java driver from real
//     BuiltInRegistries blocks, so the `==` exercised is genuine vanilla identity.
//   * the PosRuleTest arm as PosAlwaysTrueTest: always true, draws nothing
//     (PosAlwaysTrueTest.java:16) — the default position predicate ProcessorRule
//     installs when none is configured (ProcessorRule.java:19,32).
//
// The RNG must advance bit-identically, so this links the certified
// mc::levelgen::RandomSource (LegacyRandomSource), matching RandomSource.create(seed)
// used in production.

#include "world/level/levelgen/RandomSource.h"

namespace mc::levelgen::structure::templatesystem {

// 1:1 of RandomBlockStateMatchTest.test(BlockState, RandomSource):
//   return state == this.blockState && random.nextFloat() < this.probability;
// BlockState reference identity is modelled as equality of an opaque int state id.
struct RandomBlockStateMatchTest {
    int stateId;       // id of this.blockState
    float probability; // this.probability

    bool test(int argStateId, mc::levelgen::RandomSource& random) const {
        return argStateId == stateId && random.nextFloat() < probability;
    }
};

// 1:1 of PosAlwaysTrueTest.test(...): always true, draws no RNG.
struct PosAlwaysTrueTest {
    bool test(int /*inTemplatePos*/, int /*worldPos*/, int /*reference*/,
              mc::levelgen::RandomSource& /*random*/) const {
        return true;
    }
};

// 1:1 of ProcessorRule. inputPredicate and locPredicate are the two RuleTest
// arms; posPredicate is the PosRuleTest arm. This models the configuration used
// by RuleProcessor rule lists whose input/location predicates are
// RandomBlockStateMatchTest and whose position predicate is the default
// PosAlwaysTrueTest.
class ProcessorRule {
public:
    ProcessorRule(const RandomBlockStateMatchTest& inputPredicate,
                  const RandomBlockStateMatchTest& locPredicate,
                  const PosAlwaysTrueTest& posPredicate)
        : m_inputPredicate(inputPredicate),
          m_locPredicate(locPredicate),
          m_posPredicate(posPredicate) {}

    // test(inputState, locState, inTemplatePos, worldPos, reference, random).
    // Block states are opaque int ids; positions are opaque ints (the
    // PosAlwaysTrueTest arm ignores them and draws nothing). Java's `&&`
    // short-circuits, so each later arm — and its RNG draw — is reached iff all
    // earlier arms returned true.
    bool test(int inputStateId, int locStateId, int inTemplatePos, int worldPos,
              int reference, mc::levelgen::RandomSource& random) const {
        return m_inputPredicate.test(inputStateId, random)
            && m_locPredicate.test(locStateId, random)
            && m_posPredicate.test(inTemplatePos, worldPos, reference, random);
    }

private:
    RandomBlockStateMatchTest m_inputPredicate;
    RandomBlockStateMatchTest m_locPredicate;
    PosAlwaysTrueTest m_posPredicate;
};

}  // namespace mc::levelgen::structure::templatesystem

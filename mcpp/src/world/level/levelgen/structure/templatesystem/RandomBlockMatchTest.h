#pragma once

// 1:1 port of
//   net.minecraft.world.level.levelgen.structure.templatesystem.RandomBlockMatchTest
//
// The real class is a structure-processor RuleTest. Its single behavioural
// method is:
//
//   public boolean test(BlockState blockState, RandomSource random) {
//      return blockState.is(this.block) && random.nextFloat() < this.probability;
//   }
//
// Two fidelity points carry the whole gate:
//   1. SHORT-CIRCUIT: Java `&&` only evaluates `random.nextFloat()` when the
//      block matches. When the block does NOT match, the RNG is left untouched —
//      a wrong port that always drew would desync every downstream processor.
//   2. `nextFloat() < probability` is an exact IEEE-754 float comparison.
//
// `BlockState.is(Block)` reduces to block-identity (`state.getBlock() == block`).
// This header certifies the predicate LOGIC, not the block registry table, so
// block identity is modelled as equality of an opaque integer block id. The Java
// ground-truth driver supplies the ids by indexing real BuiltInRegistries blocks,
// so the equality being exercised is genuine vanilla identity.
//
// The RNG must advance bit-identically, so this links the certified
// mc::levelgen::RandomSource (LegacyRandomSource), matching
// `RandomSource.create(seed)` used in production.

#include "world/level/levelgen/RandomSource.h"

namespace mc::levelgen::structure::templatesystem {

class RandomBlockMatchTest {
public:
    RandomBlockMatchTest(int blockId, float probability)
        : m_blockId(blockId), m_probability(probability) {}

    // test(BlockState, RandomSource): stateBlockId is the block id of the
    // BlockState argument; m_blockId is the id of `this.block`. Java's `&&`
    // short-circuits, so random.nextFloat() is drawn iff the block matches.
    bool test(int stateBlockId, mc::levelgen::RandomSource& random) const {
        return stateBlockId == m_blockId && random.nextFloat() < m_probability;
    }

    int block() const { return m_blockId; }
    float probability() const { return m_probability; }

private:
    int m_blockId;
    float m_probability;
};

}  // namespace mc::levelgen::structure::templatesystem

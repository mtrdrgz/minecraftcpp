#pragma once

// 1:1 port of the block selector used by the jungle temple (jungle pyramid)
// cobblestone maze:
//   net.minecraft.world.level.levelgen.structure.structures.JungleTemplePiece
//      .MossStoneSelector  (JungleTemplePiece.java:358-367)
//
// MossStoneSelector is a StructurePiece.BlockSelector. Its only behavioural
// method is:
//
//   public void next(RandomSource random, int worldX, int worldY, int worldZ,
//                    boolean isEdge) {
//      if (random.nextFloat() < 0.4F) {
//         this.next = Blocks.COBBLESTONE.defaultBlockState();
//      } else {
//         this.next = Blocks.MOSSY_COBBLESTONE.defaultBlockState();
//      }
//   }
//
//   public BlockState getNext() { return this.next; }   // StructurePiece.java:576
//
// This is the per-block randomiser that StructurePiece.generateBox(...,
// BlockSelector) calls for every cell of the temple walls/floor; it draws ONE
// nextFloat() per cell and selects between plain and mossy cobblestone. The
// worldX/worldY/worldZ/isEdge arguments are IGNORED by this selector (only
// SmoothStoneSelector in StrongholdPieces uses isEdge); they are accepted for
// signature fidelity and must not influence the result.
//
// Two fidelity points carry the whole gate:
//   1. Exactly ONE random.nextFloat() is consumed per next() call, so the RNG
//      advances bit-identically across a sequence of cells. A port that drew a
//      different number (or a nextInt/nextDouble) would desync the whole maze.
//   2. `nextFloat() < 0.4F` is an exact IEEE-754 float comparison against the
//      literal 0.4f. The strict `<` and the exact constant decide the block.
//
// This header certifies the SELECTION LOGIC, not the block registry table.
// `getNext()` returns a BlockState; the production result is one of two real
// blocks. We model that identity as an opaque int "block code" shared with the
// Java ground-truth driver, which derives it from the real
// BuiltInRegistries.BLOCK key of Blocks.COBBLESTONE / Blocks.MOSSY_COBBLESTONE:
//   kCobblestone (0)  <- nextFloat() <  0.4f
//   kMossy       (1)  <- nextFloat() >= 0.4f
//
// The RNG must advance bit-identically, so this links the certified
// mc::levelgen::RandomSource (LegacyRandomSource), matching
// RandomSource.create(seed) used in production.

#include "world/level/levelgen/RandomSource.h"
#include "world/level/levelgen/structure/StructurePieceBase.h"
#include "world/level/block/Blocks.h"

namespace mc::levelgen::structure::structures {

// Opaque block codes shared with JungleTempleStoneSelectorParity.java. They map
// to the real Blocks.COBBLESTONE / Blocks.MOSSY_COBBLESTONE identities; the C++
// side certifies which branch is taken, not the registry contents.
enum class JungleTempleBlock : int {
    Cobblestone = 0,       // Blocks.COBBLESTONE
    MossyCobblestone = 1,  // Blocks.MOSSY_COBBLESTONE
};

// Parity-test selector: returns opaque JungleTempleBlock codes for
// certification against the Java ground truth. The RNG logic (one nextFloat()
// < 0.4F per cell) is IDENTICAL to Java's MossStoneSelector.
class JungleTempleStoneSelector {
public:
    void next(mc::levelgen::RandomSource& random, int /*worldX*/, int /*worldY*/,
              int /*worldZ*/, bool /*isEdge*/) {
        if (random.nextFloat() < 0.4F) {
            m_next = JungleTempleBlock::Cobblestone;
        } else {
            m_next = JungleTempleBlock::MossyCobblestone;
        }
    }
    JungleTempleBlock getNext() const { return m_next; }
private:
    JungleTempleBlock m_next = JungleTempleBlock::Cobblestone;
};

// Engine-integrated selector: inherits from StructurePieceBase::BlockSelector
// and resolves to REAL block state IDs via getDefaultBlockStateId. Used by
// JungleTemplePiece.postProcess via generateBox(skipAir, random, selector).
// The RNG logic is IDENTICAL to the parity version (one nextFloat() < 0.4F
// per cell), certified by jungle_temple_stone_selector_parity.
class JungleTempleStoneSelectorEngine : public mc::levelgen::structure::BlockSelector {
public:
    void next(mc::levelgen::RandomSource& random, int /*worldX*/, int /*worldY*/,
              int /*worldZ*/, bool /*isEdge*/) override {
        if (random.nextFloat() < 0.4F) {
            m_next = mc::getDefaultBlockStateId("cobblestone", 0);
        } else {
            m_next = mc::getDefaultBlockStateId("mossy_cobblestone", 0);
        }
    }
};

}  // namespace mc::levelgen::structure::structures

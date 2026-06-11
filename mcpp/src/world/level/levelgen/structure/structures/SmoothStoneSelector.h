#pragma once

// 1:1 header-only port of the block-selection LOGIC of
//   net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces
//        .SmoothStoneSelector   (a private static StructurePiece.BlockSelector)
// from decompiled Minecraft Java 26.1.2.
//
// The real class (StrongholdPieces.java, lines 1169-1187):
//
//   private static class SmoothStoneSelector extends StructurePiece.BlockSelector {
//      @Override
//      public void next(RandomSource random, int worldX, int worldY, int worldZ, boolean isEdge) {
//         if (isEdge) {
//            float selection = random.nextFloat();
//            if (selection < 0.2F) {
//               this.next = Blocks.CRACKED_STONE_BRICKS.defaultBlockState();
//            } else if (selection < 0.5F) {
//               this.next = Blocks.MOSSY_STONE_BRICKS.defaultBlockState();
//            } else if (selection < 0.55F) {
//               this.next = Blocks.INFESTED_STONE_BRICKS.defaultBlockState();
//            } else {
//               this.next = Blocks.STONE_BRICKS.defaultBlockState();
//            }
//         } else {
//            this.next = Blocks.CAVE_AIR.defaultBlockState();
//         }
//      }
//   }
//
// And the base StructurePiece.BlockSelector (StructurePiece.java, lines 571-579):
//
//   public abstract static class BlockSelector {
//      protected BlockState next = Blocks.AIR.defaultBlockState();
//      public abstract void next(RandomSource, int, int, int, boolean);
//      public BlockState getNext() { return this.next; }
//   }
//
// BlockState identity is the only thing that crosses the language boundary here.
// We certify the *selection LOGIC* (float thresholds, isEdge branch, the AIR
// initial value), NOT the registry table. Each of the six distinct BlockStates
// the selector can hold is represented by a stable enum kind; the Java driver
// emits the registry resource-location string for each, which the C++ test maps
// to the same enum.  The float draw itself is sourced from the certified
// mc::levelgen RandomSource (XoroshiroRandomSource), so the comparison is purely
// of this class's branching arithmetic.
//
// 1:1 NOTES
//  * `selection < 0.2F` etc. are float comparisons against float literals — the
//    C++ literals are written with the F suffix so the constants are float, not
//    double, matching Java exactly (a double comparison could differ at the
//    boundary).  random.nextFloat() returns a float; the comparison is float<float.
//  * The branch order is preserved exactly: 0.2F, then 0.5F, then 0.55F, else.
//  * When !isEdge the selector consumes NO RNG and yields CAVE_AIR.

#include "world/level/levelgen/RandomSource.h"

namespace mc::levelgen::structure {

// Stable identity for each BlockState the selector can produce, plus the base
// class's initial value. Values are arbitrary but fixed; the Java driver maps
// the real registry key strings onto these same kinds.
enum class StrongholdBlock {
    AIR,                    // Blocks.AIR              — base BlockSelector initial value
    CAVE_AIR,               // Blocks.CAVE_AIR         — !isEdge
    CRACKED_STONE_BRICKS,   // Blocks.CRACKED_STONE_BRICKS
    MOSSY_STONE_BRICKS,     // Blocks.MOSSY_STONE_BRICKS
    INFESTED_STONE_BRICKS,  // Blocks.INFESTED_STONE_BRICKS
    STONE_BRICKS            // Blocks.STONE_BRICKS
};

// Mirrors StructurePiece.BlockSelector (the abstract base) holding the `next`
// BlockState, plus SmoothStoneSelector's override.
class SmoothStoneSelector {
public:
    // protected BlockState next = Blocks.AIR.defaultBlockState();
    StrongholdBlock next = StrongholdBlock::AIR;

    // Exact translation of SmoothStoneSelector.next(...). worldX/Y/Z are unused
    // by this selector (kept for signature fidelity).
    void nextBlock(RandomSource& random, int worldX, int worldY, int worldZ, bool isEdge) {
        (void)worldX;
        (void)worldY;
        (void)worldZ;
        if (isEdge) {
            float selection = random.nextFloat();
            if (selection < 0.2F) {
                this->next = StrongholdBlock::CRACKED_STONE_BRICKS;
            } else if (selection < 0.5F) {
                this->next = StrongholdBlock::MOSSY_STONE_BRICKS;
            } else if (selection < 0.55F) {
                this->next = StrongholdBlock::INFESTED_STONE_BRICKS;
            } else {
                this->next = StrongholdBlock::STONE_BRICKS;
            }
        } else {
            this->next = StrongholdBlock::CAVE_AIR;
        }
    }

    // public BlockState getNext() { return this.next; }
    StrongholdBlock getNext() const { return this->next; }
};

} // namespace mc::levelgen::structure

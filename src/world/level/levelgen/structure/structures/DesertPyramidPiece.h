// Port of net.minecraft.world.level.levelgen.structure.structures.DesertPyramidPiece
//
// Certified against the REAL DesertPyramidPiece via desert_pyramid_piece_parity
// (tools/DesertPyramidPieceParity.java drives the real class via a capturing
// WorldGenLevel proxy; the C++ side replays each case and compares every
// setBlock by position + (blockName, props)).
//
// SKIPPED for now (matching the SwampHutPiece precedent):
//   - createChest (4 chests at the TNT chamber) — needs loot tables
//   - placeSand / potentialSuspiciousSandWorldPositions — needs the suspicious
//     sand block-entity hookup ( ArchaeologyProvider )
//   - placeCollapsedRoof RNG — uses level.getRandom() (separate from the piece
//     random); will be ported when the engine has a real level.getRandom()
//   - addCellar (addCellarStairs + addCellarRoom) — also uses level.getRandom()
//     and placeCollapsedRoof
// These produce a small number of block writes concentrated in the cellar
// region; the visible pyramid (the part a player sees from outside) is fully
// 1:1. The skipped pieces will be added in a follow-up once the entity /
// archaeology / loot subsystems land.
#pragma once

#include "../StructurePieceBase.h"
#include "../ScatteredFeaturePieceBox.h"

namespace mc::levelgen::structure::piece {

class DesertPyramidPiece : public StructurePieceBase {
public:
    static constexpr int WIDTH = 21;
    static constexpr int DEPTH = 21;

    DesertPyramidPiece(mc::levelgen::RandomSource& random, int west, int north)
        : StructurePieceBase(makeDesertPyramidPiece(random, west, north), WIDTH, 15, DEPTH) {}

    void postProcess(StructureWorldAccess& world, mc::levelgen::RandomSource& random) {
        // Java: if (updateHeightPositionToLowestGroundHeight(level, -random.nextInt(3)))
        if (!updateHeightPositionToLowestGroundHeight(world, -random.nextInt(3))) return;

        const uint32_t sandstone = blockState("sandstone");
        const uint32_t air = blockState("air");
        const uint32_t cutSandstone = blockState("cut_sandstone");
        const uint32_t chiseledSandstone = blockState("chiseled_sandstone");
        const uint32_t orangeTerracotta = blockState("orange_terracotta");
        const uint32_t blueTerracotta = blockState("blue_terracotta");
        const uint32_t sandstoneSlab = blockState("sandstone_slab");
        const uint32_t tnt = blockState("tnt");
        const uint32_t stonePressurePlate = blockState("stone_pressure_plate");

        const uint32_t northStairs =
            mc::getBlockStateIdWith("sandstone_stairs", {{"facing", "north"}});
        const uint32_t southStairs =
            mc::getBlockStateIdWith("sandstone_stairs", {{"facing", "south"}});
        const uint32_t eastStairs =
            mc::getBlockStateIdWith("sandstone_stairs", {{"facing", "east"}});
        const uint32_t westStairs =
            mc::getBlockStateIdWith("sandstone_stairs", {{"facing", "west"}});

        const int W = WIDTH;
        const int D = DEPTH;

        // Foundation slab
        generateBox(world, 0, -4, 0, W - 1, 0, D - 1, sandstone, sandstone, false);

        // 9-layer stepped pyramid
        for (int pos = 1; pos <= 9; ++pos) {
            generateBox(world, pos, pos, pos, W - 1 - pos, pos, D - 1 - pos,
                        sandstone, sandstone, false);
            generateBox(world, pos + 1, pos, pos + 1, W - 2 - pos, pos, D - 2 - pos,
                        air, air, false);
        }

        // Foundation fillColumnDown
        for (int x = 0; x < W; ++x)
            for (int z = 0; z < D; ++z)
                fillColumnDown(world, sandstone, x, -5, z);

        // Towers (NW corner)
        generateBox(world, 0, 0, 0, 4, 9, 4, sandstone, air, false);
        generateBox(world, 1, 10, 1, 3, 10, 3, sandstone, sandstone, false);
        placeBlock(world, northStairs, 2, 10, 0);
        placeBlock(world, southStairs, 2, 10, 4);
        placeBlock(world, eastStairs,  0, 10, 2);
        placeBlock(world, westStairs,  4, 10, 2);

        // Towers (NE corner)
        generateBox(world, W - 5, 0, 0, W - 1, 9, 4, sandstone, air, false);
        generateBox(world, W - 4, 10, 1, W - 2, 10, 3, sandstone, sandstone, false);
        placeBlock(world, northStairs, W - 3, 10, 0);
        placeBlock(world, southStairs, W - 3, 10, 4);
        placeBlock(world, eastStairs,  W - 5, 10, 2);
        placeBlock(world, westStairs,  W - 1, 10, 2);

        // Entrance
        generateBox(world, 8, 0, 0, 12, 4, 4, sandstone, air, false);
        generateBox(world, 9, 1, 0, 11, 3, 4, air, air, false);
        placeBlock(world, cutSandstone, 9, 1, 1);
        placeBlock(world, cutSandstone, 9, 2, 1);
        placeBlock(world, cutSandstone, 9, 3, 1);
        placeBlock(world, cutSandstone, 10, 3, 1);
        placeBlock(world, cutSandstone, 11, 3, 1);
        placeBlock(world, cutSandstone, 11, 2, 1);
        placeBlock(world, cutSandstone, 11, 1, 1);
        generateBox(world, 4, 1, 1, 8, 3, 3, sandstone, air, false);
        generateBox(world, 4, 1, 2, 8, 2, 2, air, air, false);
        generateBox(world, 12, 1, 1, 16, 3, 3, sandstone, air, false);
        generateBox(world, 12, 1, 2, 16, 2, 2, air, air, false);

        // Top floor
        generateBox(world, 5, 4, 5, W - 6, 4, D - 6, sandstone, sandstone, false);
        generateBox(world, 9, 4, 9, 11, 4, 11, air, air, false);
        generateBox(world, 8, 1, 8, 8, 3, 8, cutSandstone, cutSandstone, false);
        generateBox(world, 12, 1, 8, 12, 3, 8, cutSandstone, cutSandstone, false);
        generateBox(world, 8, 1, 12, 8, 3, 12, cutSandstone, cutSandstone, false);
        generateBox(world, 12, 1, 12, 12, 3, 12, cutSandstone, cutSandstone, false);
        generateBox(world, 1, 1, 5, 4, 4, 11, sandstone, sandstone, false);
        generateBox(world, W - 5, 1, 5, W - 2, 4, 11, sandstone, sandstone, false);
        generateBox(world, 6, 7, 9, 6, 7, 11, sandstone, sandstone, false);
        generateBox(world, W - 7, 7, 9, W - 7, 7, 11, sandstone, sandstone, false);
        generateBox(world, 5, 5, 9, 5, 7, 11, cutSandstone, cutSandstone, false);
        generateBox(world, W - 6, 5, 9, W - 6, 7, 11, cutSandstone, cutSandstone, false);
        placeBlock(world, air, 5, 5, 10);
        placeBlock(world, air, 5, 6, 10);
        placeBlock(world, air, 6, 6, 10);
        placeBlock(world, air, W - 6, 5, 10);
        placeBlock(world, air, W - 6, 6, 10);
        placeBlock(world, air, W - 7, 6, 10);
        generateBox(world, 2, 4, 4, 2, 6, 4, air, air, false);
        generateBox(world, W - 3, 4, 4, W - 3, 6, 4, air, air, false);
        placeBlock(world, northStairs, 2, 4, 5);
        placeBlock(world, northStairs, 2, 3, 4);
        placeBlock(world, northStairs, W - 3, 4, 5);
        placeBlock(world, northStairs, W - 3, 3, 4);
        generateBox(world, 1, 1, 3, 2, 2, 3, sandstone, sandstone, false);
        generateBox(world, W - 3, 1, 3, W - 2, 2, 3, sandstone, sandstone, false);
        placeBlock(world, sandstone, 1, 1, 2);
        placeBlock(world, sandstone, W - 2, 1, 2);
        placeBlock(world, sandstoneSlab, 1, 2, 2);
        placeBlock(world, sandstoneSlab, W - 2, 2, 2);
        placeBlock(world, westStairs, 2, 1, 2);
        placeBlock(world, eastStairs, W - 3, 1, 2);

        // Pillars
        generateBox(world, 4, 3, 5, 4, 3, 17, sandstone, sandstone, false);
        generateBox(world, W - 5, 3, 5, W - 5, 3, 17, sandstone, sandstone, false);
        generateBox(world, 3, 1, 5, 4, 2, 16, air, air, false);
        generateBox(world, W - 6, 1, 5, W - 5, 2, 16, air, air, false);

        // Pillar decorations (cut + chiseled sandstone)
        for (int z = 5; z <= 17; z += 2) {
            placeBlock(world, cutSandstone, 4, 1, z);
            placeBlock(world, chiseledSandstone, 4, 2, z);
            placeBlock(world, cutSandstone, W - 5, 1, z);
            placeBlock(world, chiseledSandstone, W - 5, 2, z);
        }

        // Floor terracotta pattern
        placeBlock(world, orangeTerracotta, 10, 0, 7);
        placeBlock(world, orangeTerracotta, 10, 0, 8);
        placeBlock(world, orangeTerracotta, 9, 0, 9);
        placeBlock(world, orangeTerracotta, 11, 0, 9);
        placeBlock(world, orangeTerracotta, 8, 0, 10);
        placeBlock(world, orangeTerracotta, 12, 0, 10);
        placeBlock(world, orangeTerracotta, 7, 0, 10);
        placeBlock(world, orangeTerracotta, 13, 0, 10);
        placeBlock(world, orangeTerracotta, 9, 0, 11);
        placeBlock(world, orangeTerracotta, 11, 0, 11);
        placeBlock(world, orangeTerracotta, 10, 0, 12);
        placeBlock(world, orangeTerracotta, 10, 0, 13);
        placeBlock(world, blueTerracotta, 10, 0, 10);

        // Side decorations (x = 0 and x = width-1)
        for (int x = 0; x <= W - 1; x += W - 1) {
            placeBlock(world, cutSandstone, x, 2, 1);
            placeBlock(world, orangeTerracotta, x, 2, 2);
            placeBlock(world, cutSandstone, x, 2, 3);
            placeBlock(world, cutSandstone, x, 3, 1);
            placeBlock(world, orangeTerracotta, x, 3, 2);
            placeBlock(world, cutSandstone, x, 3, 3);
            placeBlock(world, orangeTerracotta, x, 4, 1);
            placeBlock(world, chiseledSandstone, x, 4, 2);
            placeBlock(world, orangeTerracotta, x, 4, 3);
            placeBlock(world, cutSandstone, x, 5, 1);
            placeBlock(world, orangeTerracotta, x, 5, 2);
            placeBlock(world, cutSandstone, x, 5, 3);
            placeBlock(world, orangeTerracotta, x, 6, 1);
            placeBlock(world, chiseledSandstone, x, 6, 2);
            placeBlock(world, orangeTerracotta, x, 6, 3);
            placeBlock(world, orangeTerracotta, x, 7, 1);
            placeBlock(world, orangeTerracotta, x, 7, 2);
            placeBlock(world, orangeTerracotta, x, 7, 3);
            placeBlock(world, cutSandstone, x, 8, 1);
            placeBlock(world, cutSandstone, x, 8, 2);
            placeBlock(world, cutSandstone, x, 8, 3);
        }

        // Front-side decorations (x = 2 and x = width-3)
        for (int x = 2; x <= W - 3; x += W - 3 - 2) {
            placeBlock(world, cutSandstone, x - 1, 2, 0);
            placeBlock(world, orangeTerracotta, x, 2, 0);
            placeBlock(world, cutSandstone, x + 1, 2, 0);
            placeBlock(world, cutSandstone, x - 1, 3, 0);
            placeBlock(world, orangeTerracotta, x, 3, 0);
            placeBlock(world, cutSandstone, x + 1, 3, 0);
            placeBlock(world, orangeTerracotta, x - 1, 4, 0);
            placeBlock(world, chiseledSandstone, x, 4, 0);
            placeBlock(world, orangeTerracotta, x + 1, 4, 0);
            placeBlock(world, cutSandstone, x - 1, 5, 0);
            placeBlock(world, orangeTerracotta, x, 5, 0);
            placeBlock(world, cutSandstone, x + 1, 5, 0);
            placeBlock(world, orangeTerracotta, x - 1, 6, 0);
            placeBlock(world, chiseledSandstone, x, 6, 0);
            placeBlock(world, orangeTerracotta, x + 1, 6, 0);
            placeBlock(world, orangeTerracotta, x - 1, 7, 0);
            placeBlock(world, orangeTerracotta, x, 7, 0);
            placeBlock(world, orangeTerracotta, x + 1, 7, 0);
            placeBlock(world, cutSandstone, x - 1, 8, 0);
            placeBlock(world, cutSandstone, x, 8, 0);
            placeBlock(world, cutSandstone, x + 1, 8, 0);
        }

        // Front-center decoration
        generateBox(world, 8, 4, 0, 12, 6, 0, cutSandstone, cutSandstone, false);
        placeBlock(world, air, 8, 6, 0);
        placeBlock(world, air, 12, 6, 0);
        placeBlock(world, orangeTerracotta, 9, 5, 0);
        placeBlock(world, chiseledSandstone, 10, 5, 0);
        placeBlock(world, orangeTerracotta, 11, 5, 0);

        // TNT chamber
        generateBox(world, 8, -14, 8, 12, -11, 12, cutSandstone, cutSandstone, false);
        generateBox(world, 8, -10, 8, 12, -10, 12, chiseledSandstone, chiseledSandstone, false);
        generateBox(world, 8, -9, 8, 12, -9, 12, cutSandstone, cutSandstone, false);
        generateBox(world, 8, -8, 8, 12, -1, 12, sandstone, sandstone, false);
        generateBox(world, 9, -11, 9, 11, -1, 11, air, air, false);
        placeBlock(world, stonePressurePlate, 10, -11, 10);
        generateBox(world, 9, -13, 9, 11, -13, 11, tnt, air, false);
        placeBlock(world, air, 8, -11, 10);
        placeBlock(world, air, 8, -10, 10);
        placeBlock(world, chiseledSandstone, 7, -10, 10);
        placeBlock(world, cutSandstone, 7, -11, 10);
        placeBlock(world, air, 12, -11, 10);
        placeBlock(world, air, 12, -10, 10);
        placeBlock(world, chiseledSandstone, 13, -10, 10);
        placeBlock(world, cutSandstone, 13, -11, 10);
        placeBlock(world, air, 10, -11, 8);
        placeBlock(world, air, 10, -10, 8);
        placeBlock(world, chiseledSandstone, 10, -10, 7);
        placeBlock(world, cutSandstone, 10, -11, 7);
        placeBlock(world, air, 10, -11, 12);
        placeBlock(world, air, 10, -10, 12);
        placeBlock(world, chiseledSandstone, 10, -10, 13);
        placeBlock(world, cutSandstone, 10, -11, 13);

        // 4 chests at the TNT chamber (SKIPPED — needs loot tables).
        // Java:
        //   for (Direction direction : Direction.Plane.HORIZONTAL) {
        //     if (!hasPlacedChest[direction.get2DDataValue()]) {
        //       int xo = direction.getStepX() * 2;
        //       int zo = direction.getStepZ() * 2;
        //       hasPlacedChest[direction.get2DDataValue()] =
        //         createChest(level, chunkBB, random, 10+xo, -11, 10+zo, BuiltInLootTables.DESERT_PYRAMID);
        //     }
        //   }

        // Cellar (SKIPPED — needs level.getRandom() + placeCollapsedRoof RNG
        // + placeSand/suspicious sand + the cellar room hieroglyph sandstone).
        // Java: addCellar(level, chunkBB) -> addCellarStairs + addCellarRoom.
    }
};

} // namespace mc::levelgen::structure::piece

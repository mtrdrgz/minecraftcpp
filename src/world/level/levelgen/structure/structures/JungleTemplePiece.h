// Port of net.minecraft.world.level.levelgen.structure.structures.JungleTemplePiece
//
// SKIPPED for now (matching the SwampHut/DesertPyramid precedent):
//   - createChest (main chest + hidden chest) — needs loot tables
//   - createDispenser (2 arrow dispensers) — needs loot tables
//   - Piston traps + redstone wiring — needs redstone block-state support
//   - Tripwire hooks + tripwire — needs tripwire block-state support
//   - Vine placement (decorative) — needs vine block-state with multi-face props
// The visible cobblestone/mossy-cobblestone structure (walls, floor, stairs,
// entrance, corridors) IS ported 1:1 via the certified MossStoneSelector.
#pragma once

#include "../StructurePieceBase.h"
#include "../ScatteredFeaturePieceBox.h"
#include "JungleTempleStoneSelector.h"

namespace mc::levelgen::structure::piece {

class JungleTemplePiece : public StructurePieceBase {
public:
    static constexpr int WIDTH = 12;
    static constexpr int DEPTH = 15;

    JungleTemplePiece(mc::levelgen::RandomSource& random, int west, int north)
        : StructurePieceBase(makeScatteredFeaturePiece(random, west, 64, north, WIDTH, 10, DEPTH), WIDTH, 10, DEPTH) {}

    void postProcess(StructureWorldAccess& world, mc::levelgen::RandomSource& random) {
        if (!updateAverageGroundHeight(world, 0)) return;

        structures::JungleTempleStoneSelectorEngine stoneSelector;

        // Foundation + walls (random cobblestone/mossy via selector)
        generateBox(world, 0, -4, 0, WIDTH - 1, 0, DEPTH - 1, false, random, stoneSelector);
        generateBox(world, 2, 1, 2, 9, 2, 2, false, random, stoneSelector);
        generateBox(world, 2, 1, 12, 9, 2, 12, false, random, stoneSelector);
        generateBox(world, 2, 1, 3, 2, 2, 11, false, random, stoneSelector);
        generateBox(world, 9, 1, 3, 9, 2, 11, false, random, stoneSelector);
        generateBox(world, 1, 3, 1, 10, 6, 1, false, random, stoneSelector);
        generateBox(world, 1, 3, 13, 10, 6, 13, false, random, stoneSelector);
        generateBox(world, 1, 3, 2, 1, 6, 12, false, random, stoneSelector);
        generateBox(world, 10, 3, 2, 10, 6, 12, false, random, stoneSelector);
        generateBox(world, 2, 3, 2, 9, 3, 12, false, random, stoneSelector);
        generateBox(world, 2, 6, 2, 9, 6, 12, false, random, stoneSelector);
        generateBox(world, 3, 7, 3, 8, 7, 11, false, random, stoneSelector);
        generateBox(world, 4, 8, 4, 7, 8, 10, false, random, stoneSelector);

        // Interior air spaces
        generateAirBox(world, 3, 1, 3, 8, 2, 11);
        generateAirBox(world, 4, 3, 6, 7, 3, 9);
        generateAirBox(world, 2, 4, 2, 9, 5, 12);
        generateAirBox(world, 4, 6, 5, 7, 6, 9);
        generateAirBox(world, 5, 7, 6, 6, 7, 8);
        generateAirBox(world, 5, 1, 2, 6, 2, 2);
        generateAirBox(world, 5, 2, 12, 6, 2, 12);
        generateAirBox(world, 5, 5, 1, 6, 5, 1);
        generateAirBox(world, 5, 5, 13, 6, 5, 13);
        const uint32_t air = mc::getDefaultBlockStateId("air", 0);
        placeBlock(world, air, 1, 5, 5);
        placeBlock(world, air, 10, 5, 5);
        placeBlock(world, air, 1, 5, 9);
        placeBlock(world, air, 10, 5, 9);

        // Pillars at z=0 and z=14
        for (int z = 0; z <= 14; z += 14) {
            generateBox(world, 2, 4, z, 2, 5, z, false, random, stoneSelector);
            generateBox(world, 4, 4, z, 4, 5, z, false, random, stoneSelector);
            generateBox(world, 7, 4, z, 7, 5, z, false, random, stoneSelector);
            generateBox(world, 9, 4, z, 9, 5, z, false, random, stoneSelector);
        }
        generateBox(world, 5, 6, 0, 6, 6, 0, false, random, stoneSelector);

        // Pillars at x=0 and x=11
        for (int x = 0; x <= 11; x += 11) {
            for (int z = 2; z <= 12; z += 2) {
                generateBox(world, x, 4, z, x, 5, z, false, random, stoneSelector);
            }
            generateBox(world, x, 6, 5, x, 6, 5, false, random, stoneSelector);
            generateBox(world, x, 6, 9, x, 6, 9, false, random, stoneSelector);
        }

        // Corner pillars
        generateBox(world, 2, 7, 2, 2, 9, 2, false, random, stoneSelector);
        generateBox(world, 9, 7, 2, 9, 9, 2, false, random, stoneSelector);
        generateBox(world, 2, 7, 12, 2, 9, 12, false, random, stoneSelector);
        generateBox(world, 9, 7, 12, 9, 9, 12, false, random, stoneSelector);
        generateBox(world, 4, 9, 4, 4, 9, 4, false, random, stoneSelector);
        generateBox(world, 7, 9, 4, 7, 9, 4, false, random, stoneSelector);
        generateBox(world, 4, 9, 10, 4, 9, 10, false, random, stoneSelector);
        generateBox(world, 7, 9, 10, 7, 9, 10, false, random, stoneSelector);
        generateBox(world, 5, 9, 7, 6, 9, 7, false, random, stoneSelector);

        // Stairs (cobblestone_stairs with 4 facings)
        const uint32_t northStairs = mc::getBlockStateIdWith("cobblestone_stairs", {{"facing", "north"}});
        const uint32_t southStairs = mc::getBlockStateIdWith("cobblestone_stairs", {{"facing", "south"}});
        const uint32_t eastStairs  = mc::getBlockStateIdWith("cobblestone_stairs", {{"facing", "east"}});
        const uint32_t westStairs  = mc::getBlockStateIdWith("cobblestone_stairs", {{"facing", "west"}});
        placeBlock(world, northStairs, 5, 9, 6);
        placeBlock(world, northStairs, 6, 9, 6);
        placeBlock(world, southStairs, 5, 9, 8);
        placeBlock(world, southStairs, 6, 9, 8);
        placeBlock(world, northStairs, 4, 0, 0);
        placeBlock(world, northStairs, 5, 0, 0);
        placeBlock(world, northStairs, 6, 0, 0);
        placeBlock(world, northStairs, 7, 0, 0);
        placeBlock(world, northStairs, 4, 1, 8);
        placeBlock(world, northStairs, 4, 2, 9);
        placeBlock(world, northStairs, 4, 3, 10);
        placeBlock(world, northStairs, 7, 1, 8);
        placeBlock(world, northStairs, 7, 2, 9);
        placeBlock(world, northStairs, 7, 3, 10);
        generateBox(world, 4, 1, 9, 4, 1, 9, false, random, stoneSelector);
        generateBox(world, 7, 1, 9, 7, 1, 9, false, random, stoneSelector);
        generateBox(world, 4, 1, 10, 7, 2, 10, false, random, stoneSelector);
        generateBox(world, 5, 4, 5, 6, 4, 5, false, random, stoneSelector);
        placeBlock(world, eastStairs, 4, 4, 5);
        placeBlock(world, westStairs, 7, 4, 5);

        // Stair descent (entrance to lower level)
        for (int i = 0; i < 4; ++i) {
            placeBlock(world, southStairs, 5, 0 - i, 6 + i);
            placeBlock(world, southStairs, 6, 0 - i, 6 + i);
            generateAirBox(world, 5, 0 - i, 7 + i, 6, 0 - i, 9 + i);
        }

        // Lower level air spaces
        generateAirBox(world, 1, -3, 12, 10, -1, 13);
        generateAirBox(world, 1, -3, 1, 3, -1, 13);
        generateAirBox(world, 1, -3, 1, 9, -1, 5);

        // Lower level floor
        for (int z = 1; z <= 13; z += 2) {
            generateBox(world, 1, -3, z, 1, -2, z, false, random, stoneSelector);
        }
        for (int z = 2; z <= 12; z += 2) {
            generateBox(world, 1, -1, z, 3, -1, z, false, random, stoneSelector);
        }
        generateBox(world, 2, -2, 1, 5, -2, 1, false, random, stoneSelector);
        generateBox(world, 7, -2, 1, 9, -2, 1, false, random, stoneSelector);
        generateBox(world, 6, -3, 1, 6, -3, 1, false, random, stoneSelector);
        generateBox(world, 6, -1, 1, 6, -1, 1, false, random, stoneSelector);

        // Tripwires, dispensers, redstone, pistons, chests, vines: SKIPPED.
        // The visible cobblestone temple structure is complete; the puzzle
        // mechanics (traps, hidden chest) need entity/loot/redstone subsystems.
    }
};

} // namespace mc::levelgen::structure::piece

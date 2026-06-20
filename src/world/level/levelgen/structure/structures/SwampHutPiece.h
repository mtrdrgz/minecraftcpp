// Port of net.minecraft.world.level.levelgen.structure.structures.SwampHutPiece
//
// Certified byte-exact against the REAL SwampHutPiece.postProcess by
// swamp_hut_piece_parity (tools/SwampHutPieceParity.java drives the real class
// via a capturing WorldGenLevel proxy and emits every setBlock to a TSV).
//
// Entity spawns (witch + cat) at the tail of Java's postProcess are NOT ported
// — they call EntityType.create(level.getLevel(), ...) which needs a real
// ServerLevel. The block-placement surface (floor, walls, roof, foundation)
// is fully 1:1.
#pragma once

#include "../StructurePieceBase.h"
#include "../ScatteredFeaturePieceBox.h"

namespace mc::levelgen::structure::piece {

class SwampHutPiece : public StructurePieceBase {
public:
    SwampHutPiece(mc::levelgen::RandomSource& random, int west, int north)
        : StructurePieceBase(makeSwampHutPiece(random, west, north), 7, 7, 9) {}

    void postProcess(StructureWorldAccess& world) {
        if (!updateAverageGroundHeight(world, 0)) return;

        const uint32_t sprucePlanks = blockState("spruce_planks");
        const uint32_t oakLog = blockState("oak_log");
        const uint32_t oakFence = blockState("oak_fence");
        const uint32_t air = blockState("air");
        const uint32_t pottedRedMushroom = blockState("potted_red_mushroom");
        const uint32_t craftingTable = blockState("crafting_table");
        const uint32_t cauldron = blockState("cauldron");

        // Roof — spruce stairs with the four cardinal facings (Java calls
        // Blocks.SPRUCE_STAIRS.defaultBlockState().setValue(StairBlock.FACING, dir)
        // for each of NORTH/EAST/WEST/SOUTH, plus four corner pieces with
        // OUTER_LEFT/OUTER_RIGHT shapes overriding the straight-shape corners).
        const uint32_t northStairs =
            mc::getBlockStateIdWith("spruce_stairs", {{"facing", "north"}});
        const uint32_t eastStairs =
            mc::getBlockStateIdWith("spruce_stairs", {{"facing", "east"}});
        const uint32_t westStairs =
            mc::getBlockStateIdWith("spruce_stairs", {{"facing", "west"}});
        const uint32_t southStairs =
            mc::getBlockStateIdWith("spruce_stairs", {{"facing", "south"}});
        const uint32_t northOuterRight =
            mc::getBlockStateIdWith("spruce_stairs", {{"facing", "north"}, {"shape", "outer_right"}});
        const uint32_t northOuterLeft =
            mc::getBlockStateIdWith("spruce_stairs", {{"facing", "north"}, {"shape", "outer_left"}});
        const uint32_t southOuterLeft =
            mc::getBlockStateIdWith("spruce_stairs", {{"facing", "south"}, {"shape", "outer_left"}});
        const uint32_t southOuterRight =
            mc::getBlockStateIdWith("spruce_stairs", {{"facing", "south"}, {"shape", "outer_right"}});

        generateBox(world, 1, 1, 1, 5, 1, 7, sprucePlanks, sprucePlanks, false);
        generateBox(world, 1, 4, 2, 5, 4, 7, sprucePlanks, sprucePlanks, false);
        generateBox(world, 2, 1, 0, 4, 1, 0, sprucePlanks, sprucePlanks, false);
        generateBox(world, 2, 2, 2, 3, 3, 2, sprucePlanks, sprucePlanks, false);
        generateBox(world, 1, 2, 3, 1, 3, 6, sprucePlanks, sprucePlanks, false);
        generateBox(world, 5, 2, 3, 5, 3, 6, sprucePlanks, sprucePlanks, false);
        generateBox(world, 2, 2, 7, 4, 3, 7, sprucePlanks, sprucePlanks, false);
        generateBox(world, 1, 0, 2, 1, 3, 2, oakLog, oakLog, false);
        generateBox(world, 5, 0, 2, 5, 3, 2, oakLog, oakLog, false);
        generateBox(world, 1, 0, 7, 1, 3, 7, oakLog, oakLog, false);
        generateBox(world, 5, 0, 7, 5, 3, 7, oakLog, oakLog, false);
        placeBlock(world, oakFence, 2, 3, 2);
        placeBlock(world, oakFence, 3, 3, 7);
        placeBlock(world, air, 1, 3, 4);
        placeBlock(world, air, 5, 3, 4);
        placeBlock(world, air, 5, 3, 5);
        placeBlock(world, pottedRedMushroom, 1, 3, 5);
        placeBlock(world, craftingTable, 3, 2, 6);
        placeBlock(world, cauldron, 4, 2, 6);
        placeBlock(world, oakFence, 1, 2, 1);
        placeBlock(world, oakFence, 5, 2, 1);
        // Roof edges — one facing per side
        generateBox(world, 0, 4, 1, 6, 4, 1, northStairs, northStairs, false);
        generateBox(world, 0, 4, 2, 0, 4, 7, eastStairs,  eastStairs,  false);
        generateBox(world, 6, 4, 2, 6, 4, 7, westStairs,  westStairs,  false);
        generateBox(world, 0, 4, 8, 6, 4, 8, southStairs, southStairs, false);
        // Roof corner pieces (override the straight-shape corner cells)
        placeBlock(world, northOuterRight, 0, 4, 1);
        placeBlock(world, northOuterLeft,  6, 4, 1);
        placeBlock(world, southOuterLeft,  0, 4, 8);
        placeBlock(world, southOuterRight, 6, 4, 8);
        // Foundation
        for (int z = 2; z <= 7; z += 5) {
            for (int x = 1; x <= 5; x += 4) {
                fillColumnDown(world, oakLog, x, -1, z);
            }
        }
        // Entity spawns (witch + cat) — NOT ported (entity system pending)
    }
};

} // namespace mc::levelgen::structure::piece

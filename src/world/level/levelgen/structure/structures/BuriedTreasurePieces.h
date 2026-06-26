// Port of net.minecraft.world.level.levelgen.structure.structures.BuriedTreasurePieces
// (BuriedTreasurePiece.postProcess).
//
// The chest LOOT (createChest -> chest block-entity + BuiltInLootTables.BURIED_TREASURE)
// is NOT ported — that needs the loot-table + block-entity system (same honest gap as
// the DesertPyramid / SwampHut chests). The chest BLOCK and the surrounding air/liquid
// fill ARE placed 1:1, so the structure is a real, visible buried-treasure mound.
#pragma once

#include "../StructurePieceBase.h"        // StructureWorldAccess, blockState()
#include "../../../block/BlockState.h"    // mc::getBlockState
#include "../../../block/Blocks.h"

#include <string>

namespace mc::levelgen::structure::piece {

class BuriedTreasurePiece {
public:
    // BuriedTreasureStructure.generatePieces:
    //   offset = new BlockPos(chunkPos.getBlockX(9), 90, chunkPos.getBlockZ(9))
    //   the piece's boundingBox is new BoundingBox(offset), so minX/minZ = offset.x/z.
    BuriedTreasurePiece(int offsetX, int offsetZ) : m_x(offsetX), m_z(offsetZ) {}

    void postProcess(StructureWorldAccess& world) {
        if (!world.getBlock || !world.setBlock) return;

        auto blockName = [](uint32_t state) -> std::string {
            const mc::BlockState* bs = mc::getBlockState(state);
            if (!bs || !bs->block) return {};
            std::string name = bs->block->name;
            if (name.rfind("minecraft:", 0) == 0) name.erase(0, 10);
            return name;
        };
        auto isAir = [](uint32_t state) {
            const mc::BlockState* bs = mc::getBlockState(state);
            return !bs || bs->isAir();
        };
        // BuriedTreasurePiece.isLiquid: blockState.is(WATER) || is(LAVA).
        auto isLiquid = [&](uint32_t state) {
            const std::string n = blockName(state);
            return n == "water" || n == "lava";
        };
        // belowState.is(SANDSTONE|STONE|ANDESITE|GRANITE|DIORITE). These are all
        // single-state blocks, so a block-name test is exact.
        auto isAnchor = [&](uint32_t state) {
            const std::string n = blockName(state);
            return n == "sandstone" || n == "stone" || n == "andesite" ||
                   n == "granite" || n == "diorite";
        };

        const uint32_t sand = blockState("sand");
        const uint32_t chest = blockState("chest");

        const int minX = m_x, minZ = m_z;
        // Java: y = getHeight(OCEAN_FLOOR_WG, minX, minZ). The engine's getHeight is a
        // safe (>=) upper bound for the downward scan, which stops at the first
        // anchor-supported column, so the burial spot is identical.
        int y = world.getHeight ? world.getHeight(minX, minZ) : 90;

        // Direction.values(): DOWN, UP, NORTH, SOUTH, WEST, EAST.
        static const int DX[6] = { 0, 0, 0, 0, -1, 1 };
        static const int DY[6] = { -1, 1, 0, 0, 0, 0 };
        static const int DZ[6] = { 0, 0, -1, 1, 0, 0 };
        constexpr int UP = 1;

        for (; y > world.minY; --y) {
            const uint32_t currentState = world.getBlock(minX, y, minZ);
            const uint32_t belowState = world.getBlock(minX, y - 1, minZ);
            if (!isAnchor(belowState)) continue;

            const uint32_t softState =
                (!isAir(currentState) && !isLiquid(currentState)) ? currentState : sand;

            for (int d = 0; d < 6; ++d) {
                const int rx = minX + DX[d], ry = y + DY[d], rz = minZ + DZ[d];
                const uint32_t relState = world.getBlock(rx, ry, rz);
                if (!(isAir(relState) || isLiquid(relState))) continue;
                const uint32_t belowRel = world.getBlock(rx, ry - 1, rz);
                if ((isAir(belowRel) || isLiquid(belowRel)) && d != UP) {
                    world.setBlock(rx, ry, rz, belowState);
                } else {
                    world.setBlock(rx, ry, rz, softState);
                }
            }

            // createChest(level, chunkBB, random, pos, BURIED_TREASURE) — loot/block-entity
            // not ported; place the chest block itself (default state).
            world.setBlock(minX, y, minZ, chest);
            return;
        }
    }

private:
    int m_x, m_z;
};

}  // namespace mc::levelgen::structure::piece

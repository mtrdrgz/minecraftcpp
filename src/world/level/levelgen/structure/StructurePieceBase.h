// Port of net.minecraft.world.level.levelgen.structure.StructurePiece base methods
#pragma once

#include "ScatteredFeaturePieceBox.h"
#include "../../block/Blocks.h"
#include "../../block/BlockState.h"
#include "../RandomSource.h"

// Types BoundingBox, Direction, Mirror, Rotation, BlockPos, Vec3i are defined
// in StructurePieceMath.h (namespace piece) AND StructureTransforms.h (namespace structure).
// We rely on whichever is included first by the caller.
// For StructureGen.cpp, StructureTransforms.h is included, so types are in
// namespace mc::levelgen::structure.

namespace mc::levelgen::structure {

// Forward-declare types that come from StructureTransforms.h (included by caller)
// If StructurePieceMath.h is included first, these are in piece:: and we need using-declarations.

struct StructureWorldAccess {
    std::function<uint32_t(int, int, int)> getBlock;
    std::function<void(int, int, int, uint32_t)> setBlock;
    std::function<int(int, int)> getHeight;
    std::function<bool(int, int, int)> isInsideBoundingBox;
    int minY = -64;
};

// Use the types from whatever namespace they're defined in.
// In StructureGen.cpp context, they come from StructureTransforms.h in namespace mc::levelgen::structure.
// We use a helper to access them.
class StructurePieceBase {
public:
    StructurePieceBase(const piece::ScatteredFeaturePieceCtor& ctor, int width, int height, int depth)
        : m_boundingBox(ctor.boundingBox)
        , m_orientation(ctor.orientation)
        , m_mirror(ctor.mirror)
        , m_rotation(ctor.rotation)
        , m_width(width), m_height(height), m_depth(depth)
        , m_heightPosition(-1) {}

    // Get world position from local coords
    struct LocalPos { int x, y, z; };
    LocalPos getWorldPos(int x, int y, int z) const {
        return { m_boundingBox.minX + x, m_boundingBox.minY + y, m_boundingBox.minZ + z };
    }

    void placeBlock(StructureWorldAccess& world, uint32_t state, int x, int y, int z) const {
        auto pos = getWorldPos(x, y, z);
        if (world.isInsideBoundingBox && !world.isInsideBoundingBox(pos.x, pos.y, pos.z)) return;
        if (world.setBlock) world.setBlock(pos.x, pos.y, pos.z, state);
    }

    void generateBox(StructureWorldAccess& world, int x0, int y0, int z0,
                     int x1, int y1, int z1,
                     uint32_t edgeBlock, uint32_t fillBlock, bool skipAir) const {
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                for (int z = z0; z <= z1; z++) {
                    if (skipAir) {
                        auto pos = getWorldPos(x, y, z);
                        if (world.getBlock) {
                            uint32_t existing = world.getBlock(pos.x, pos.y, pos.z);
                            const mc::BlockState* bs = mc::getBlockState(existing);
                            if (bs && bs->block && bs->block->isAir()) continue;
                        }
                    }
                    if (y != y0 && y != y1 && x != x0 && x != x1 && z != z0 && z != z1)
                        placeBlock(world, fillBlock, x, y, z);
                    else
                        placeBlock(world, edgeBlock, x, y, z);
                }
    }

    void fillColumnDown(StructureWorldAccess& world, uint32_t blockState, int x, int startY, int z) const {
        auto pos = getWorldPos(x, startY, z);
        if (world.isInsideBoundingBox && !world.isInsideBoundingBox(pos.x, pos.y, pos.z)) return;
        while (pos.y > world.minY + 1) {
            if (world.setBlock) world.setBlock(pos.x, pos.y, pos.z, blockState);
            pos.y--;
            if (world.getBlock) {
                uint32_t existing = world.getBlock(pos.x, pos.y, pos.z);
                const mc::BlockState* bs = mc::getBlockState(existing);
                if (bs && bs->block && !bs->block->isAir() && !bs->block->isFluid()) break;
            }
        }
    }

    bool updateAverageGroundHeight(StructureWorldAccess& world, int offset) {
        if (m_heightPosition >= 0) return true;
        int total = 0, count = 0;
        for (int z = m_boundingBox.minZ; z <= m_boundingBox.maxZ; z++)
            for (int x = m_boundingBox.minX; x <= m_boundingBox.maxX; x++) {
                if (world.isInsideBoundingBox && !world.isInsideBoundingBox(x, 64, z)) continue;
                if (world.getHeight) { total += world.getHeight(x, z); count++; }
            }
        if (count == 0) return false;
        m_heightPosition = total / count + offset;
        int dy = m_heightPosition - m_boundingBox.minY;
        m_boundingBox.minY += dy;
        m_boundingBox.maxY += dy;
        return true;
    }

protected:
    piece::BoundingBox m_boundingBox;
    piece::Direction m_orientation = piece::Direction::NORTH;
    piece::Mirror m_mirror = piece::Mirror::NONE;
    piece::Rotation m_rotation = piece::Rotation::NONE;
    int m_width = 0, m_height = 0, m_depth = 0;
    int m_heightPosition = -1;
};

inline uint32_t blockState(const char* name) {
    return mc::getDefaultBlockStateId(name, 0);
}

} // namespace mc::levelgen::structure

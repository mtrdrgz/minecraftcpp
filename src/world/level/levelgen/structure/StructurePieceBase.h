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
    // Optional state-transform hook: given a stateId + Mirror + Rotation, returns
    // the transformed stateId (Java's BlockState.mirror().rotate() applied).
    // If null, placeBlock uses the state as-is (correct only when the piece's
    // mirror/rotation are NONE, e.g. SwampHutPiece facing NORTH). Used by the
    // SwampHutPiece parity test to replicate Java's placeBlock transform.
    std::function<uint32_t(uint32_t stateId, piece::Mirror mir, piece::Rotation rot)> transformState;
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

    // Get world position from local coords. Java's StructurePiece.getWorldPos
    // is orientation-dependent:
    //   NORTH: worldX = minX + x, worldZ = maxZ - z
    //   SOUTH: worldX = minX + x, worldZ = minZ + z
    //   WEST:  worldX = maxX - z, worldZ = minZ + x
    //   EAST:  worldX = minX + z, worldZ = minZ + x
    //   worldY = minY + y (always, when orientation != null)
    // (StructurePiece.java:132-176.) The previous C++ port was NORTH-only,
    // which placed blocks at wrong world positions for any non-NORTH piece.
    struct LocalPos { int x, y, z; };
    LocalPos getWorldPos(int x, int y, int z) const {
        int wx, wz;
        switch (m_orientation) {
            case piece::Direction::NORTH: wx = m_boundingBox.minX + x; wz = m_boundingBox.maxZ - z; break;
            case piece::Direction::SOUTH: wx = m_boundingBox.minX + x; wz = m_boundingBox.minZ + z; break;
            case piece::Direction::WEST:  wx = m_boundingBox.maxX - z; wz = m_boundingBox.minZ + x; break;
            case piece::Direction::EAST:  wx = m_boundingBox.minX + z; wz = m_boundingBox.minZ + x; break;
            default:                      wx = m_boundingBox.minX + x; wz = m_boundingBox.minZ + z; break;
        }
        return { wx, m_boundingBox.minY + y, wz };
    }

    void placeBlock(StructureWorldAccess& world, uint32_t state, int x, int y, int z) const {
        // Java StructurePiece.placeBlock:
        //   BlockPos pos = getWorldPos(x, y, z);
        //   if (chunkBB.isInside(pos)) {
        //       if (canBeReplaced(level, x, y, z, chunkBB)) {  // default: true
        //           if (mirror != NONE)  blockState = blockState.mirror(mirror);
        //           if (rotation != NONE) blockState = blockState.rotate(rotation);
        //           level.setBlock(pos, blockState, 2);
        //           ...fluid / shape-check postprocessing...
        //       }
        //   }
        // We apply the mirror/rotate via world.transformState if the caller
        // supplied one (the engine wires it to BlockRotation.h's rotate/mirror;
        // standalone parity binaries wire it to a local lookup). If null, the
        // state is placed as-is — correct only when the piece's mirror/rotation
        // are NONE.
        auto pos = getWorldPos(x, y, z);
        if (world.isInsideBoundingBox && !world.isInsideBoundingBox(pos.x, pos.y, pos.z)) return;
        if (world.transformState) {
            state = world.transformState(state, m_mirror, m_rotation);
        }
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
        // Java StructurePiece.fillColumnDown:
        //   while (isReplaceableByStructures(level.getBlockState(pos)) && pos.getY() > level.getMinY() + 1) {
        //       level.setBlock(pos, blockState, 2);
        //       pos.move(Direction.DOWN);
        //   }
        // isReplaceableByStructures = isAir() || liquid() || is(GLOW_LICHEN) || is(SEAGRASS) || is(TALL_SEAGRASS).
        // NOTE: the check happens FIRST each iteration — if the starting block is
        // already solid (e.g. floor under the hut), NOTHING is placed. The previous
        // C++ port did setBlock-then-check, which over-wrote one solid block per
        // column before noticing. This fix matches Java exactly.
        auto pos = getWorldPos(x, startY, z);
        if (world.isInsideBoundingBox && !world.isInsideBoundingBox(pos.x, pos.y, pos.z)) return;
        while (pos.y > world.minY + 1 && isReplaceableByStructures(world, pos.x, pos.y, pos.z)) {
            if (world.setBlock) world.setBlock(pos.x, pos.y, pos.z, blockState);
            pos.y--;
        }
    }

    // Java StructurePiece.isReplaceableByStructures(state):
    //   state.isAir() || state.liquid() || state.is(Blocks.GLOW_LICHEN)
    //                  || state.is(Blocks.SEAGRASS) || state.is(Blocks.TALL_SEAGRASS)
    static bool isReplaceableByStructures(StructureWorldAccess& world, int x, int y, int z) {
        if (!world.getBlock) return true;  // unknown => treat as replaceable (matches air)
        uint32_t id = world.getBlock(x, y, z);
        const mc::BlockState* bs = mc::getBlockState(id);
        if (!bs || !bs->block) return true;  // unknown => replaceable
        if (bs->block->isAir()) return true;
        if (bs->block->isFluid()) return true;
        const std::string& name = bs->block->name;
        if (name == "glow_lichen" || name == "seagrass" || name == "tall_seagrass") return true;
        return false;
    }

    bool updateAverageGroundHeight(StructureWorldAccess& world, int offset) {
        // Java ScatteredFeaturePiece.updateAverageGroundHeight:
        //   if (heightPosition >= 0) return true;
        //   int total=0, count=0;
        //   for z in [minZ..maxZ] for x in [minX..maxX]:
        //     pos.set(x, 64, z);
        //     if (chunkBB.isInside(pos)) { total += getHeightmapPos(MOTION_BLOCKING_NO_LEAVES, pos).getY(); count++; }
        //   if (count == 0) return false;
        //   heightPosition = total / count;
        //   boundingBox.move(0, heightPosition - minY + offset, 0);
        // (ScatteredFeaturePiece.java:50-76.)
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

    bool updateHeightPositionToLowestGroundHeight(StructureWorldAccess& world, int offset) {
        // Java ScatteredFeaturePiece.updateHeightPositionToLowestGroundHeight:
        //   if (heightPosition >= 0) return true;
        //   int lowest = level.getMaxY() + 1;
        //   boolean found = false;
        //   for z in [minZ..maxZ] for x in [minX..maxX]:
        //     pos.set(x, 0, z);
        //     lowest = min(lowest, getHeightmapPos(MOTION_BLOCKING_NO_LEAVES, pos).getY());
        //     found = true;
        //   if (!found) return false;
        //   heightPosition = lowest;
        //   boundingBox.move(0, heightPosition - minY + offset, 0);
        // (ScatteredFeaturePiece.java:78-102.) Used by DesertPyramidPiece and
        // JungleTemplePiece. NOTE: no chunkBB.isInside check here (unlike
        // updateAverageGroundHeight) — Java iterates the whole bounding box.
        if (m_heightPosition >= 0) return true;
        int lowest = 319 + 1;  // level.getMaxY() + 1 for overworld
        bool found = false;
        for (int z = m_boundingBox.minZ; z <= m_boundingBox.maxZ; z++)
            for (int x = m_boundingBox.minX; x <= m_boundingBox.maxX; x++) {
                if (world.getHeight) {
                    int h = world.getHeight(x, z);
                    if (h < lowest) lowest = h;
                    found = true;
                }
            }
        if (!found) return false;
        m_heightPosition = lowest;
        int dy = m_heightPosition - m_boundingBox.minY + offset;
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

#pragma once

// 1:1 port of net.minecraft.client.renderer.block.BlockModelLighter.prepareQuadShape
// (BlockModelLighter.java:219-274) — the per-quad geometry classification that the AO + flat
// lighting paths run first: the quad's min/max extent, the faceShape[12] weights (used by the AO
// blend's barycentric vertex weights), and the facePartial / faceCubic flags. Level-free except
// faceCubic's state.isCollisionShapeFullBlock (fed as a bool). Certified by
// block_model_lighter_shape_parity.

#include "../model/Joml.h"
#include "BlockModelLighterTables.h"

#include <algorithm>

namespace mc::render::block {

namespace aolight {

using Vector3f = mc::render::model::joml::Vector3f;

struct QuadShape {
    float faceShape[SIZE_INFO_COUNT];  // valid only when ambientOcclusion
    bool facePartial;
    bool faceCubic;
};

// direction: Direction ordinal DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5.
inline QuadShape prepareQuadShape(const Vector3f positions[4], int direction,
                                  bool isCollisionShapeFullBlock, bool ambientOcclusion) {
    float minX = 32.0F, minY = 32.0F, minZ = 32.0F;
    float maxX = -32.0F, maxY = -32.0F, maxZ = -32.0F;
    for (int i = 0; i < 4; ++i) {
        float x = positions[i].x, y = positions[i].y, z = positions[i].z;
        minX = std::min(minX, x); minY = std::min(minY, y); minZ = std::min(minZ, z);
        maxX = std::max(maxX, x); maxY = std::max(maxY, y); maxZ = std::max(maxZ, z);
    }

    QuadShape r{};
    if (ambientOcclusion) {
        r.faceShape[S_WEST] = minX;
        r.faceShape[S_EAST] = maxX;
        r.faceShape[S_DOWN] = minY;
        r.faceShape[S_UP] = maxY;
        r.faceShape[S_NORTH] = minZ;
        r.faceShape[S_SOUTH] = maxZ;
        r.faceShape[S_FLIP_WEST] = 1.0F - minX;
        r.faceShape[S_FLIP_EAST] = 1.0F - maxX;
        r.faceShape[S_FLIP_DOWN] = 1.0F - minY;
        r.faceShape[S_FLIP_UP] = 1.0F - maxY;
        r.faceShape[S_FLIP_NORTH] = 1.0F - minZ;
        r.faceShape[S_FLIP_SOUTH] = 1.0F - maxZ;
    }

    const float lo = 1.0E-4F, hi = 0.9999F;
    switch (direction) {
        case 0: case 1:  // DOWN, UP
            r.facePartial = minX >= lo || minZ >= lo || maxX <= hi || maxZ <= hi; break;
        case 2: case 3:  // NORTH, SOUTH
            r.facePartial = minX >= lo || minY >= lo || maxX <= hi || maxY <= hi; break;
        default:         // WEST, EAST
            r.facePartial = minY >= lo || minZ >= lo || maxY <= hi || maxZ <= hi; break;
    }

    switch (direction) {
        case 0: r.faceCubic = (minY == maxY) && (minY < lo || isCollisionShapeFullBlock); break;  // DOWN
        case 1: r.faceCubic = (minY == maxY) && (maxY > hi || isCollisionShapeFullBlock); break;  // UP
        case 2: r.faceCubic = (minZ == maxZ) && (minZ < lo || isCollisionShapeFullBlock); break;  // NORTH
        case 3: r.faceCubic = (minZ == maxZ) && (maxZ > hi || isCollisionShapeFullBlock); break;  // SOUTH
        case 4: r.faceCubic = (minX == maxX) && (minX < lo || isCollisionShapeFullBlock); break;  // WEST
        default: r.faceCubic = (minX == maxX) && (maxX > hi || isCollisionShapeFullBlock); break; // EAST
    }
    return r;
}

}  // namespace aolight

}  // namespace mc::render::block

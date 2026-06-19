#pragma once

// 1:1 port of BlockModelLighter.prepareQuadFlat (BlockModelLighter.java:198-217) — the non-AO
// (flat) block-lighting path: a single lightCoords + a single directional-shade color applied to
// all 4 vertices. Composes ONLY certified pieces: aolight::prepareQuadShape (faceCubic) +
// levelrender::getLightCoords + mc::argb::gray + CardinalLighting.byFace/up. Certified by
// block_model_lighter_flat_parity.
//
//   if lightCoords == -1 (CHECK_LIGHT): prepareQuadShape -> lightPos = faceCubic ? pos+dir : pos;
//     lightCoords = getLightCoords(state, level, lightPos)   [emissive/emission from `state`,
//     packedBrightness from lightPos];  else use the passed lightCoords.
//   color = ARGB.gray(materialShade ? cardinal.byFace(dir) : cardinal.up()).

#include "../../util/ARGB.h"
#include "../../world/level/CardinalLighting.h"
#include "../LevelRenderLight.h"
#include "BlockModelLighterShape.h"

namespace mc::render::block {

namespace aolight {

namespace argb = mc::argb;
namespace levelrender = mc::render::levelrender;
namespace cardinal = mc::world::level::cardinal;

struct FlatResult {
    int color;        // applied to all 4 vertices
    int lightCoords;  // applied to all 4 vertices
};

inline FlatResult prepareQuadFlat(
    const Vector3f positions[4], int direction, bool materialShade,
    const cardinal::CardinalLighting& cardLight, int passedLightCoords,
    bool isCollisionShapeFullBlock,
    // getLightCoords(state, level, lightPos): emissive/emission from `state`, brightness from lightPos.
    bool emissiveRendering, int lightEmission,
    int packedBrightnessAtPos, int packedBrightnessAtPosPlusDir) {

    FlatResult out;
    if (passedLightCoords == -1) {  // CHECK_LIGHT
        QuadShape qs = prepareQuadShape(positions, direction, isCollisionShapeFullBlock, false);
        int packed = qs.faceCubic ? packedBrightnessAtPosPlusDir : packedBrightnessAtPos;
        out.lightCoords = levelrender::getLightCoords(emissiveRendering, packed, lightEmission);
    } else {
        out.lightCoords = passedLightCoords;
    }
    float brightness = materialShade ? cardLight.byFace(direction) : cardLight.up;
    out.color = argb::gray(brightness);
    return out;
}

}  // namespace aolight

}  // namespace mc::render::block

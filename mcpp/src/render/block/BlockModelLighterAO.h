#pragma once

// 1:1 port of the ambient-occlusion BLEND in BlockModelLighter.prepareQuadAmbientOcclusion
// (BlockModelLighter.java:126-196) — the smooth-lighting math that turns the per-corner shade/light
// values + the quad geometry into the 4 per-vertex colors + lightCoords. Composes ONLY certified
// pieces: aolight::prepareQuadShape + the certified AdjacencyInfo/AmbientVertexRemap tables +
// mc::argb::gray/scaleRGB + mc::util::lightcoords::smoothBlend/smoothWeightedBlend + clamp. The
// per-corner shade/light inputs (resolved by the level corner-walk) are fed externally. Certified by
// block_model_lighter_ao_parity.

#include "../../util/ARGB.h"
#include "../../util/Brightness.h"
#include "../../world/level/CardinalLighting.h"
#include "BlockModelLighterShape.h"

namespace mc::render::block {

namespace aolight {

namespace argb = mc::argb;
namespace lc = mc::util::lightcoords;
namespace cardinal = mc::world::level::cardinal;

struct AOResult {
    int color[4];        // indexed by output vertex slot (after AmbientVertexRemap)
    int lightCoords[4];
};

// java.lang.Math.clamp(float value, float min, float max) for finite values.
inline float clampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline AOResult prepareQuadAmbientOcclusionBlend(
    const Vector3f positions[4], int direction, bool materialShade,
    const cardinal::CardinalLighting& cardLight, bool isCollisionShapeFullBlock,
    float shade0, float shade1, float shade2, float shade3,
    float shadeCorner02, float shadeCorner03, float shadeCorner12, float shadeCorner13, float shadeCenter,
    int light0, int light1, int light2, int light3,
    int lightCorner02, int lightCorner03, int lightCorner12, int lightCorner13, int lightCenter) {

    QuadShape qs = prepareQuadShape(positions, direction, isCollisionShapeFullBlock, true);
    const AdjacencyInfo& info = ADJACENCY[direction];
    const int* remap = AMBIENT_VERTEX_REMAP[direction];

    int tmpColor[4], tmpLight[4];  // by output slot

    if (qs.facePartial && info.doNonCubicWeight) {
        float tempShade1 = (shade3 + shade0 + shadeCorner03 + shadeCenter) * 0.25F;
        float tempShade2 = (shade2 + shade0 + shadeCorner02 + shadeCenter) * 0.25F;
        float tempShade3 = (shade2 + shade1 + shadeCorner12 + shadeCenter) * 0.25F;
        float tempShade4 = (shade3 + shade1 + shadeCorner13 + shadeCenter) * 0.25F;

        int tc1 = lc::smoothBlend(light3, light0, lightCorner03, lightCenter);
        int tc2 = lc::smoothBlend(light2, light0, lightCorner02, lightCenter);
        int tc3 = lc::smoothBlend(light2, light1, lightCorner12, lightCenter);
        int tc4 = lc::smoothBlend(light3, light1, lightCorner13, lightCenter);

        for (int v = 0; v < 4; ++v) {
            const int* vw = info.vertWeights[v];
            float w01 = qs.faceShape[vw[0]] * qs.faceShape[vw[1]];
            float w23 = qs.faceShape[vw[2]] * qs.faceShape[vw[3]];
            float w45 = qs.faceShape[vw[4]] * qs.faceShape[vw[5]];
            float w67 = qs.faceShape[vw[6]] * qs.faceShape[vw[7]];
            float gray = clampF(tempShade1 * w01 + tempShade2 * w23 + tempShade3 * w45 + tempShade4 * w67, 0.0F, 1.0F);
            tmpColor[remap[v]] = argb::gray(gray);
            tmpLight[remap[v]] = lc::smoothWeightedBlend(tc1, tc2, tc3, tc4, w01, w23, w45, w67);
        }
    } else {
        float ll1 = (shade3 + shade0 + shadeCorner03 + shadeCenter) * 0.25F;
        float ll2 = (shade2 + shade0 + shadeCorner02 + shadeCenter) * 0.25F;
        float ll3 = (shade2 + shade1 + shadeCorner12 + shadeCenter) * 0.25F;
        float ll4 = (shade3 + shade1 + shadeCorner13 + shadeCenter) * 0.25F;
        tmpLight[remap[0]] = lc::smoothBlend(light3, light0, lightCorner03, lightCenter);
        tmpLight[remap[1]] = lc::smoothBlend(light2, light0, lightCorner02, lightCenter);
        tmpLight[remap[2]] = lc::smoothBlend(light2, light1, lightCorner12, lightCenter);
        tmpLight[remap[3]] = lc::smoothBlend(light3, light1, lightCorner13, lightCenter);
        tmpColor[remap[0]] = argb::gray(ll1);
        tmpColor[remap[1]] = argb::gray(ll2);
        tmpColor[remap[2]] = argb::gray(ll3);
        tmpColor[remap[3]] = argb::gray(ll4);
    }

    float scale = materialShade ? cardLight.byFace(direction) : cardLight.up;
    AOResult out;
    for (int i = 0; i < 4; ++i) {
        out.color[i] = argb::scaleRGB(tmpColor[i], scale);
        out.lightCoords[i] = tmpLight[i];
    }
    return out;
}

}  // namespace aolight

}  // namespace mc::render::block

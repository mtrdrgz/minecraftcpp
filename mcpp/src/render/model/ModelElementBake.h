#pragma once

// 1:1 port of net.minecraft.client.resources.model.cuboid.UnbakedCuboidGeometry.bake
// (UnbakedCuboidGeometry.java:22-74) — the element-level block-model bake: for each cuboid
// element, the degenerate-dimension face gating (from==to disables perpendicular faces), the
// per-face shouldDraw-by-axis test, the auto/explicit UV choice, FaceBakery.bakeQuad per face
// (the certified assembly), and the unculled/culled bucketing (culled bucket = the certified
// Direction.rotate(modelMatrix, cullForDirection)). Output mirrors QuadCollection's buckets.
//
// The material resolution (MaterialInfo.of -> Sheets/RenderType) is metadata only (it never
// affects positions/UVs/direction/bucketing); the gate feeds resolved sprite bounds per face,
// exactly as the bakeQuad gate does. Certified by model_element_bake_parity.

#include "DirectionRotate.h"
#include "FaceBakery.h"

#include <vector>

namespace mc::render::model {

namespace elembake {

using fb::BakedQuadGeom;
using fb::SpriteUV;
using fb::UVs;
using joml::Matrix4f;
using joml::Vector3f;

// Direction.getAxis() ordinal: DOWN/UP->Y(1), NORTH/SOUTH->Z(2), WEST/EAST->X(0).
inline constexpr int AXIS_OF_DIR[6] = {1, 1, 2, 2, 0, 0};

// One face of an element (the geometry-relevant subset of CuboidFace + its resolved sprite).
struct ElementFace {
    bool present = false;
    SpriteUV sprite{};       // resolved atlas bounds for face.texture()
    bool hasUv = false;      // face.uvs() != null
    UVs uv{};                // explicit uv (model space [0,16]) when hasUv
    int uvRotation = 0;      // Quadrant shift 0..3
    int tintIndex = -1;      // metadata (carried, not bucket-affecting)
    int cullForDirection = -1;  // -1 = unculled, else Direction ordinal
};

// One cuboid element: box + optional element rotation + faces indexed by Direction ordinal.
struct CuboidElement {
    Vector3f from{}, to{};
    bool hasElement = false;
    Vector3f elementOrigin{};
    Matrix4f elementTransform{};
    bool shade = true;       // metadata
    int lightEmission = 0;   // metadata
    ElementFace faces[6];    // by Direction ordinal (DOWN..EAST)
};

// QuadCollection-equivalent buckets: unculled + per-Direction culled, insertion order.
struct QuadBuckets {
    std::vector<BakedQuadGeom> unculled;
    std::vector<BakedQuadGeom> culled[6];  // by Direction ordinal
};

// UnbakedCuboidGeometry.bake. modelMatrix = modelState.transformation().getMatrix()
// (identity when the model state is the identity); hasModel/hasUvTransform/uvTransform are
// the per-bake flags (uvTransform = modelState.inverseFaceTransformation, the same for all
// faces in the identity / per-state case fed here).
inline QuadBuckets bakeCuboidGeometry(
    const std::vector<CuboidElement>& elements,
    bool hasModel, const Matrix4f& modelMatrix,
    bool hasUvTransform, const Matrix4f& uvTransform) {
    QuadBuckets out;
    for (const CuboidElement& el : elements) {
        bool drawXFaces = true, drawYFaces = true, drawZFaces = true;
        if (el.from.x == el.to.x) { drawYFaces = false; drawZFaces = false; }
        if (el.from.y == el.to.y) { drawXFaces = false; drawZFaces = false; }
        if (el.from.z == el.to.z) { drawXFaces = false; drawYFaces = false; }
        if (!(drawXFaces || drawYFaces || drawZFaces)) continue;

        for (int facing = 0; facing < 6; ++facing) {
            const ElementFace& face = el.faces[facing];
            if (!face.present) continue;
            int axis = AXIS_OF_DIR[facing];
            bool shouldDraw = (axis == 0) ? drawXFaces : (axis == 1) ? drawYFaces : drawZFaces;
            if (!shouldDraw) continue;

            UVs uvs = face.hasUv ? face.uv : fb::defaultFaceUV(el.from, el.to, facing);
            BakedQuadGeom quad = fb::bakeQuad(el.from, el.to, uvs, face.uvRotation, facing, face.sprite,
                                              hasModel, modelMatrix, hasUvTransform, uvTransform,
                                              el.hasElement, el.elementOrigin, el.elementTransform);
            if (face.cullForDirection < 0) {
                out.unculled.push_back(quad);
            } else {
                int bucket = dir::rotate(modelMatrix, face.cullForDirection);
                out.culled[bucket].push_back(quad);
            }
        }
    }
    return out;
}

}  // namespace elembake

}  // namespace mc::render::model

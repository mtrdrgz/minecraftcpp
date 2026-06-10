#pragma once

// 1:1 port of the net.minecraft.client.resources.model.ResolvedModel parent-chain resolution
// (ResolvedModel.java:21-86) — findTopTextureSlots / findTopAmbientOcclusion / findTopGuiLight /
// findTopGeometry. Each walks `this` -> parent() accumulating/short-circuiting on the node's
// wrapped() UnbakedModel property. bakeTopGeometry = getTopGeometry().bake(...) = the certified
// element bake (ModelElementBake.h). Pure data; reuses the certified TextureSlots.Resolver.

#include "TextureSlotsResolve.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc::render::model {

namespace resolvedmodel {

// UnbakedModel.GuiLight (UnbakedModel.java:36-38). DEFAULT_GUI_LIGHT = SIDE.
enum class GuiLight { FRONT = 0, SIDE = 1 };

// One node of the resolved parent chain = the node's wrapped() UnbakedModel properties.
//   ambientOcclusion()/guiLight()/geometry() are @Nullable (absent => inherit from parent).
//   geometryId: a stable identity for this node's geometry (>=0) when present (-1 = absent);
//   the EMPTY fallback is reported as -1.
struct ModelNode {
    std::optional<bool> ambientOcclusion;
    std::optional<GuiLight> guiLight;
    std::optional<int> geometryId;
    texslots::Data textureSlots;  // this node's own textureSlots() Data layer
};

// chain[0] = top (this), chain[i+1] = parent of chain[i], ..., last = root.

// ResolvedModel.findTopAmbientOcclusion (default true).
inline bool findTopAmbientOcclusion(const std::vector<ModelNode>& chain) {
    for (const ModelNode& n : chain)
        if (n.ambientOcclusion.has_value()) return *n.ambientOcclusion;
    return true;
}

// ResolvedModel.findTopGuiLight (default SIDE).
inline GuiLight findTopGuiLight(const std::vector<ModelNode>& chain) {
    for (const ModelNode& n : chain)
        if (n.guiLight.has_value()) return *n.guiLight;
    return GuiLight::SIDE;
}

// ResolvedModel.findTopGeometry: first node with geometry, else EMPTY (-1).
inline int findTopGeometry(const std::vector<ModelNode>& chain) {
    for (const ModelNode& n : chain)
        if (n.geometryId.has_value()) return *n.geometryId;
    return -1;
}

// ResolvedModel.findTopTextureSlots: addLast(node.textureSlots()) top..root, then resolve.
inline std::unordered_map<std::string, texslots::Material> findTopTextureSlots(const std::vector<ModelNode>& chain) {
    texslots::Resolver resolver;
    for (const ModelNode& n : chain) resolver.addLast(n.textureSlots);
    return resolver.resolve();
}

}  // namespace resolvedmodel

}  // namespace mc::render::model

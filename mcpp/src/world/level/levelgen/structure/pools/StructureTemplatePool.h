#pragma once

// 1:1 C++ port of the PURE, deterministic core of the 26.1.2 jigsaw template-pool
// type hierarchy + the worldgen/template_pool JSON loader:
//   net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool
//   net.minecraft.world.level.levelgen.structure.pools.StructurePoolElement (base)
//   net.minecraft.world.level.levelgen.structure.pools.SinglePoolElement
//   net.minecraft.world.level.levelgen.structure.pools.LegacySinglePoolElement
//   net.minecraft.world.level.levelgen.structure.pools.ListPoolElement
//   net.minecraft.world.level.levelgen.structure.pools.EmptyPoolElement
//   net.minecraft.world.level.levelgen.structure.pools.FeaturePoolElement
//
// This feeds the C++ jigsaw structure-assembly placer. NOTHING here touches a real
// world: it is the pool's `templates` build (weight-expansion), getRandomTemplate /
// getShuffledTemplates (Util.shuffle), getMaxSize (max element box Y-span), the
// per-element getBoundingBox/getProjection/getGroundLevelDelta/getSize, plus a
// nlohmann JSON loader for the worldgen/template_pool schema.
//
// Sources translated verbatim (line refs into 26.1.2/src):
//   StructureTemplatePool ctor `templates` build                    :48-61
//       for each rawTemplate (element,weight): add element `weight` times, in order.
//   StructureTemplatePool.getMaxSize(manager)                       :83-94
//       max over templates (!= EmptyPoolElement.INSTANCE) of
//       element.getBoundingBox(manager, BlockPos.ZERO, Rotation.NONE).getYSpan(),
//       orElse(0). Cached in maxSize (SIZE_UNSET = Integer.MIN_VALUE).
//   StructureTemplatePool.getRandomTemplate(random)                 :105-107
//       templates.isEmpty() ? EMPTY : templates.get(random.nextInt(size)).
//   StructureTemplatePool.getShuffledTemplates(random)              :109-111
//       Util.shuffledCopy(templates, random) = copy then Util.shuffle.
//   StructureTemplatePool.size()                                    :113-115
//   StructureTemplatePool.Projection                                :117-148
//       TERRAIN_MATCHING("terrain_matching") ordinal 0,
//       RIGID("rigid") ordinal 1.  (declaration order!)
//   StructurePoolElement.getProjection/setProjection/getGroundLevelDelta :78-94
//       getGroundLevelDelta() == 1 (base; no subclass overrides it).
//   SinglePoolElement.getSize(mgr, rot) -> template.getSize(rot)    :82-86
//   SinglePoolElement.getBoundingBox(mgr, pos, rot)                 :129-133
//       template.getBoundingBox(new StructurePlaceSettings().setRotation(rot), pos)
//       = StructureTemplate.getBoundingBox(pos, rot, pivot=ZERO, mirror=NONE, size)
//       (StructurePlaceSettings defaults: rotationPivot=BlockPos.ZERO, mirror=NONE).
//   LegacySinglePoolElement                                          (whole file)
//       identical box/size/projection semantics to SinglePoolElement; only place()
//       processors differ (not ported here — placement lives elsewhere).
//   ListPoolElement.getSize(mgr, rot)                               :37-51
//       per-axis max over sub-element sizes.
//   ListPoolElement.getBoundingBox(mgr, pos, rot)                   :60-68
//       encapsulate of sub-element boxes (filter out EmptyPoolElement.INSTANCE).
//   ListPoolElement ctor: sub-elements inherit the list's projection :27-35,112-114
//   EmptyPoolElement.getSize -> Vec3i.ZERO                          :26-29
//   EmptyPoolElement.getBoundingBox -> throws (filter me!)         :38-41
//       (its projection is fixed TERRAIN_MATCHING in the private ctor :22-24)
//   FeaturePoolElement.getSize -> Vec3i.ZERO                        :52-55
//   FeaturePoolElement.getBoundingBox(mgr, pos, rot)               :72-78
//       new BoundingBox(pos.x, pos.y, pos.z, pos.x+0, pos.y+0, pos.z+0) (size ZERO).
//   net.minecraft.util.Util.shuffle / shuffledCopy                 Util.java:1049-1068
//       Fisher-Yates: for (i=size; i>1; i--) { swapTo=nextInt(i); swap(i-1,swapTo); }
//   StructureTemplate.getBoundingBox(pos, rot, pivot, mirror, size) StructureTemplate.java:623-629
//       -> reused certified structureGetBoundingBox (StructureTransforms.h).
//   BoundingBox.encapsulatingBoxes(Iterable)                       BoundingBox.java:138-148
//
// DELIBERATELY UNPORTED (world-touching / not needed for assembly geometry; listed,
// NOT stubbed-true): place(), getShuffledJigsawBlocks (lives in StructureTemplateLoader.h
// for SinglePoolElement; ListPoolElement delegates to elements[0]), handleDataMarker,
// FeaturePoolElement's PlacedFeature/defaultJigsawNBT, processors lists, the Projection
// GravityProcessor instances. The fallback is carried as a resource-id string only
// (Holder<StructureTemplatePool> getFallback identity is not needed for the geometry gate).
//
// Certified byte-exact by structure_template_pool_parity
// (tools/StructureTemplatePoolParity.java drives the REAL pools + REAL elements).

#include "../templatesystem/StructureTransforms.h"   // structureGetBoundingBox, Rotation, BoundingBox, Vec3i (certified)
#include "../../RandomSource.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mc::levelgen::structure::pools {

using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::Mirror;
using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Vec3i;
using mc::levelgen::structure::BlockPos;
using mc::levelgen::structure::kBlockPosZero;
using mc::levelgen::structure::structureGetBoundingBox;

// ── StructureTemplatePool.Projection (StructureTemplatePool.java:117-148) ─────
// Enum DECLARATION order is TERRAIN_MATCHING, RIGID — so ordinals are
// TERRAIN_MATCHING=0, RIGID=1. The serialized names are "terrain_matching"/"rigid".
enum class Projection : int { TERRAIN_MATCHING = 0, RIGID = 1 };

inline const char* projectionName(Projection p) noexcept {
    return p == Projection::RIGID ? "rigid" : "terrain_matching";
}

// Projection.byName(name) — StringRepresentable.fromEnum(values).byName(name).
inline std::optional<Projection> projectionByName(const std::string& name) {
    if (name == "rigid") return Projection::RIGID;
    if (name == "terrain_matching") return Projection::TERRAIN_MATCHING;
    return std::nullopt;
}

// A size resolver: maps an element template location (Identifier string, e.g.
// "minecraft:pillager_outpost/base_plate") to the StructureTemplate's UNROTATED
// Vec3i size. The parity test feeds it from the base64 .nbt the Java tool ships;
// in the real engine it is the StructureTemplateManager.getOrCreate(...).getSize().
using SizeResolver = std::function<Vec3i(const std::string& location)>;

// ── StructurePoolElement hierarchy ───────────────────────────────────────────
// We model the concrete types with an enum tag + the per-type data; getBoundingBox/
// getSize/getProjection/getGroundLevelDelta dispatch on the tag. This keeps the
// header allocation-free and copyable while reproducing the exact Java semantics.
enum class ElementType : int { SINGLE, LEGACY, LIST, EMPTY, FEATURE };

struct StructurePoolElement {
    ElementType type = ElementType::EMPTY;
    Projection projection = Projection::RIGID;

    // SINGLE / LEGACY: the template location (Identifier string).
    std::string location;

    // LIST: the encapsulated sub-elements (each carries the list's projection).
    std::vector<StructurePoolElement> elements;

    // ── getProjection() — StructurePoolElement.java:83-90 ──
    Projection getProjection() const noexcept { return projection; }

    // ── getGroundLevelDelta() — StructurePoolElement.java:92-94: returns 1 (no
    // concrete subclass overrides it). ──
    int getGroundLevelDelta() const noexcept { return 1; }

    bool isEmpty() const noexcept { return type == ElementType::EMPTY; }

    // ListPoolElement.setProjection :100-105 + setProjectionOnEachElement :112-114 —
    // set this element's projection and, if a list, recursively each sub-element's.
    void setProjectionRecursive(Projection p) noexcept {
        projection = p;
        if (type == ElementType::LIST)
            for (auto& c : elements) c.setProjectionRecursive(p);
    }

    // ── getSize(manager, rotation) ──
    // SinglePoolElement/Legacy -> template.getSize(rotation); template.getSize(rot)
    // swaps X/Z when rotation is CW90/CCW90 (StructureTemplate.getSize). The
    // certified StructureTransforms doesn't expose getSize(rotation) directly, but
    // getSize(rotation) = (rot==CW90||CCW90) ? (z,y,x) : (x,y,z) of the raw size.
    Vec3i getSize(const SizeResolver& sizeOf, Rotation rotation) const {
        switch (type) {
            case ElementType::SINGLE:
            case ElementType::LEGACY: {
                Vec3i s = sizeOf(location);
                if (rotation == Rotation::CLOCKWISE_90 || rotation == Rotation::COUNTERCLOCKWISE_90)
                    return Vec3i{s.z, s.y, s.x};
                return s;
            }
            case ElementType::LIST: {
                // ListPoolElement.getSize :37-51 — per-axis max over sub-elements.
                int sx = 0, sy = 0, sz = 0;
                for (const auto& e : elements) {
                    Vec3i s = e.getSize(sizeOf, rotation);
                    sx = std::max(sx, s.x);
                    sy = std::max(sy, s.y);
                    sz = std::max(sz, s.z);
                }
                return Vec3i{sx, sy, sz};
            }
            case ElementType::EMPTY:
            case ElementType::FEATURE:
            default:
                return Vec3i{0, 0, 0}; // Vec3i.ZERO
        }
    }

    // ── getBoundingBox(manager, position, rotation) ──
    BoundingBox getBoundingBox(const SizeResolver& sizeOf, const BlockPos& position, Rotation rotation) const {
        switch (type) {
            case ElementType::SINGLE:
            case ElementType::LEGACY: {
                // SinglePoolElement.getBoundingBox :129-133 ->
                // StructureTemplate.getBoundingBox(pos, rot, pivot=ZERO, mirror=NONE, size).
                Vec3i size = sizeOf(location);
                return structureGetBoundingBox(position, rotation, kBlockPosZero, Mirror::NONE, size);
            }
            case ElementType::FEATURE: {
                // FeaturePoolElement.getBoundingBox :72-78 — size is Vec3i.ZERO, so
                // box = (pos.x, pos.y, pos.z, pos.x, pos.y, pos.z).
                return BoundingBox(position.x, position.y, position.z,
                                   position.x, position.y, position.z);
            }
            case ElementType::LIST: {
                // ListPoolElement.getBoundingBox :60-68 — encapsulate of sub-element
                // boxes, filtering out EmptyPoolElement.INSTANCE; orElseThrow if empty.
                bool started = false;
                BoundingBox result{0, 0, 0, 0, 0, 0};
                for (const auto& e : elements) {
                    if (e.type == ElementType::EMPTY) continue; // filter EmptyPoolElement.INSTANCE
                    BoundingBox b = e.getBoundingBox(sizeOf, position, rotation);
                    if (!started) {
                        // encapsulatingBoxes seeds result with the FIRST box verbatim
                        // (BoundingBox.java:144-145 — no inversion normalization needed
                        // since b is already normalized).
                        result = b;
                        started = true;
                    } else {
                        result.encapsulate(b);
                    }
                }
                if (!started)
                    throw std::runtime_error("Unable to calculate boundingbox for ListPoolElement");
                return result;
            }
            case ElementType::EMPTY:
            default:
                // EmptyPoolElement.getBoundingBox :38-41 — throws "filter me!".
                throw std::runtime_error("Invalid call to EmptyPoolElement.getBoundingBox, filter me!");
        }
    }

    // Resolved location(s) for the TSV element-order check. SINGLE/LEGACY -> the
    // template id; LIST -> "list[a,b,...]" (mirrors JigsawPlacementParity.elementLocation);
    // EMPTY -> "Empty"; FEATURE -> "Feature".
    std::string locationString() const {
        switch (type) {
            case ElementType::SINGLE:
            case ElementType::LEGACY:
                return location;
            case ElementType::LIST: {
                std::string s = "list[";
                for (std::size_t i = 0; i < elements.size(); ++i) {
                    if (i) s += ',';
                    s += elements[i].locationString();
                }
                s += ']';
                return s;
            }
            case ElementType::FEATURE: return "feature[" + location + "]";
            case ElementType::EMPTY:
            default:                   return "Empty";
        }
    }
};

// ── StructureTemplatePool (StructureTemplatePool.java) ───────────────────────
class StructureTemplatePool {
public:
    // rawTemplates: the (element, weight) pairs in JSON order; templates: the
    // weight-expanded flat list (ctor :48-61). fallback: the fallback pool id string.
    StructureTemplatePool() = default;

    static constexpr int SIZE_UNSET = INT32_MIN; // StructureTemplatePool.java:27.

    std::string fallback;
    std::vector<std::pair<StructurePoolElement, int>> rawTemplates;
    std::vector<StructurePoolElement> templates; // weight-expanded.

    // getFallback() — :101-103 (we carry the id string, not the Holder identity).
    const std::string& getFallbackName() const noexcept { return fallback; }

    // size() — :113-115.
    int size() const noexcept { return static_cast<int>(templates.size()); }

    // getRandomTemplate(random) — :105-107. Returns the index into templates (or -1
    // when empty, i.e. EmptyPoolElement.INSTANCE in Java). The placer reads the
    // element; the index is the deterministic, comparable observable.
    int getRandomTemplateIndex(mc::levelgen::RandomSource& random) const {
        if (templates.empty()) return -1; // EmptyPoolElement.INSTANCE
        return random.nextInt(static_cast<int>(templates.size()));
    }

    // getShuffledTemplates(random) — :109-111 = Util.shuffledCopy(templates, random):
    // copy the templates list, then Util.shuffle (Fisher-Yates over nextInt). We
    // return the post-shuffle indices into the ORIGINAL templates list (the
    // deterministic observable the parity gate compares).
    std::vector<int> getShuffledTemplateIndices(mc::levelgen::RandomSource& random) const {
        std::vector<int> idx(templates.size());
        for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<int>(i);
        // Util.shuffle(copy, random) — Util.java:1061-1068.
        int n = static_cast<int>(idx.size());
        for (int i = n; i > 1; --i) {
            int swapTo = random.nextInt(i);
            std::swap(idx[static_cast<std::size_t>(i - 1)], idx[static_cast<std::size_t>(swapTo)]);
        }
        return idx;
    }

    // getMaxSize(manager) — :83-94: max over templates (!= EmptyPoolElement.INSTANCE)
    // of element.getBoundingBox(manager, BlockPos.ZERO, Rotation.NONE).getYSpan(),
    // orElse(0). Cached (SIZE_UNSET sentinel).
    int getMaxSize(const SizeResolver& sizeOf) const {
        if (maxSize_ == SIZE_UNSET) {
            int best = 0;
            bool any = false;
            for (const auto& t : templates) {
                if (t.type == ElementType::EMPTY) continue; // filter EmptyPoolElement.INSTANCE
                int span = t.getBoundingBox(sizeOf, kBlockPosZero, Rotation::NONE).getYSpan();
                best = any ? std::max(best, span) : span;
                any = true;
            }
            maxSize_ = any ? best : 0; // .max().orElse(0)
        }
        return maxSize_;
    }

private:
    mutable int maxSize_ = SIZE_UNSET;
};

// ── JSON loader (worldgen/template_pool schema) ──────────────────────────────
// Mirrors how StructurePlacement.cpp consumes nlohmann::json.
//
// Schema (StructureTemplatePool.DIRECT_CODEC + StructurePoolElement.CODEC dispatch):
//   { "fallback": "<pool id>",
//     "elements": [ { "element": {<element>}, "weight": <int 1..150> }, ... ] }
//   <element> = { "element_type": "minecraft:single_pool_element |
//                                  legacy_single_pool_element | list_pool_element |
//                                  empty_pool_element | feature_pool_element",
//                 "location": "<id>" (single/legacy),
//                 "projection": "rigid|terrain_matching" (single/legacy/list),
//                 "elements": [<element>...] (list) }
namespace detail {

inline std::string stripNamespace(const std::string& s) {
    auto pos = s.find(':');
    return pos == std::string::npos ? s : s.substr(pos + 1);
}

// Parse one <element> object (StructurePoolElement.CODEC dispatch on element_type).
// `inheritedProjection` is the parent list's projection for list children (the list
// ctor :27-35 + setProjectionOnEachElement :112-114 overwrite each child's projection
// with the list's; for non-list elements it is read from the element's own field).
inline StructurePoolElement parseElement(const nlohmann::json& j) {
    StructurePoolElement e;
    std::string et = stripNamespace(j.at("element_type").get<std::string>());

    if (et == "empty_pool_element") {
        e.type = ElementType::EMPTY;
        // EmptyPoolElement's ctor fixes projection = TERRAIN_MATCHING (:22-24).
        e.projection = Projection::TERRAIN_MATCHING;
        return e;
    }

    if (et == "feature_pool_element") {
        e.type = ElementType::FEATURE;
        // projectionCodec(): "projection" field.
        e.projection = projectionByName(j.at("projection").get<std::string>()).value_or(Projection::RIGID);
        // "feature": PlacedFeature reference id — carried (normalized) so the element
        // gets a stable identity in locationString() (mirrors the GT's elementLocation).
        std::string fid = j.at("feature").get<std::string>();
        if (fid.find(':') == std::string::npos) fid = "minecraft:" + fid;
        e.location = fid;
        return e;
    }

    if (et == "list_pool_element") {
        e.type = ElementType::LIST;
        e.projection = projectionByName(j.at("projection").get<std::string>()).value_or(Projection::RIGID);
        for (const auto& sub : j.at("elements")) {
            StructurePoolElement child = parseElement(sub);
            // ListPoolElement ctor :27-35 -> setProjectionOnEachElement(projection)
            // overwrites EACH child's projection with the list's projection.
            child.setProjectionRecursive(e.projection);
            e.elements.push_back(std::move(child));
        }
        return e;
    }

    // single / legacy_single.
    e.type = (et == "legacy_single_pool_element") ? ElementType::LEGACY : ElementType::SINGLE;
    e.projection = projectionByName(j.at("projection").get<std::string>()).value_or(Projection::RIGID);
    // templateCodec(): "location" Identifier (carried verbatim, normalized to ns:path).
    std::string loc = j.at("location").get<std::string>();
    if (loc.find(':') == std::string::npos) loc = "minecraft:" + loc;
    e.location = loc;
    return e;
}

} // namespace detail

// loadPool(json) — StructureTemplatePool.DIRECT_CODEC.decode. Builds rawTemplates in
// JSON order and the weight-expanded `templates` list (ctor :48-61).
inline StructureTemplatePool loadPool(const nlohmann::json& j) {
    StructureTemplatePool pool;
    pool.fallback = j.at("fallback").get<std::string>();
    if (pool.fallback.find(':') == std::string::npos) pool.fallback = "minecraft:" + pool.fallback;

    for (const auto& entry : j.at("elements")) {
        StructurePoolElement element = detail::parseElement(entry.at("element"));
        int weight = entry.at("weight").get<int>(); // Codec.intRange(1,150).
        pool.rawTemplates.emplace_back(element, weight);
        // ctor :52-58 — add `element` to templates `weight` times, in JSON order.
        for (int i = 0; i < weight; ++i) pool.templates.push_back(element);
    }
    return pool;
}

} // namespace mc::levelgen::structure::pools

#pragma once

// 1:1 port of net.minecraft.client.resources.model.sprite.TextureSlots.Resolver.resolve +
// TextureSlots.getMaterial (TextureSlots.java:33-39, 102-162) — the model texture-slot
// resolution: a stack of Data layers (parent..child) collapsed into a slot->Material map,
// with '#ref' indirection resolved to a fixpoint. This is the texture half of the model bake
// input (ResolvedModel.findTopTextureSlots). Pure data; no GL, no JSON.
//
// The resolved map is ORDER-INVARIANT: resolution is monotonic (a slot, once resolved, never
// leaves; each Reference has a single fixed target), so map iteration order only changes the
// number of fixpoint passes, never the final values. So std::unordered_map is faithful here.

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc::render::model {

namespace texslots {

// net.minecraft.client.resources.model.sprite.Material — (Identifier sprite, boolean forceTranslucent).
struct Material {
    std::string sprite;        // Identifier "namespace:path"
    bool forceTranslucent = false;
    bool operator==(const Material& o) const { return sprite == o.sprite && forceTranslucent == o.forceTranslucent; }
};

// TextureSlots.SlotContents: either a Value(Material) or a Reference(target slot name).
struct SlotContents {
    bool isReference = false;
    std::string refTarget;     // when isReference
    Material value;            // when !isReference
};

// TextureSlots.Data — one layer's slot map. (Insertion order is irrelevant to the result.)
struct Data {
    std::vector<std::pair<std::string, SlotContents>> values;
    void addReference(const std::string& slot, const std::string& target) {
        values.push_back({slot, SlotContents{true, target, {}}});
    }
    void addTexture(const std::string& slot, const Material& m) {
        values.push_back({slot, SlotContents{false, {}, m}});
    }
};

// TextureSlots.Resolver — addFirst/addLast push layers; resolve() collapses them.
class Resolver {
public:
    void addLast(const Data& d) { entries.push_back(d); }
    void addFirst(const Data& d) { entries.insert(entries.begin(), d); }

    // resolve(): returns the resolved slot -> Material map (TextureSlots.resolvedValues).
    std::unordered_map<std::string, Material> resolve() const {
        std::unordered_map<std::string, Material> resolved;
        if (entries.empty()) return resolved;

        std::unordered_map<std::string, std::string> unresolved;  // slot -> reference target

        // Lists.reverse(entries): parent layers first, child layers override.
        for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
            for (const auto& [slot, contents] : it->values) {
                if (!contents.isReference) {
                    unresolved.erase(slot);
                    resolved[slot] = contents.value;
                } else {
                    resolved.erase(slot);
                    unresolved[slot] = contents.refTarget;
                }
            }
        }

        if (unresolved.empty()) return resolved;

        bool hasChanges = true;
        while (hasChanges) {
            hasChanges = false;
            for (auto it = unresolved.begin(); it != unresolved.end();) {
                auto found = resolved.find(it->second);
                if (found != resolved.end()) {
                    resolved[it->first] = found->second;
                    it = unresolved.erase(it);
                    hasChanges = true;
                } else {
                    ++it;
                }
            }
        }
        // any still-unresolved references are dropped (vanilla logs a warning) — never faked.
        return resolved;
    }

    // TextureSlots.getMaterial(reference): strip a leading '#', then look up.
    static std::optional<Material> getMaterial(const std::unordered_map<std::string, Material>& resolved,
                                               std::string reference) {
        if (!reference.empty() && reference[0] == '#') reference = reference.substr(1);
        auto it = resolved.find(reference);
        if (it == resolved.end()) return std::nullopt;
        return it->second;
    }

private:
    std::vector<Data> entries;
};

}  // namespace texslots

}  // namespace mc::render::model

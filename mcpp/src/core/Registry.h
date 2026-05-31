#pragma once
#include "ResourceLocation.h"
#include <vector>
#include <unordered_map>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace mc {

// Direct port of net.minecraft.core.Registry<T>
// Bidirectional mapping: ResourceLocation <-> numeric ID <-> T*
template<class T>
class Registry {
public:
    uint32_t register_(ResourceLocation loc, T* entry) {
        uint32_t id = static_cast<uint32_t>(m_entries.size());
        m_entries.push_back(entry);
        m_byId[id] = entry;
        m_byName[loc] = id;
        m_nameById[id] = loc;
        entry->registryId = id;
        return id;
    }

    T* getById(uint32_t id) const {
        if (id < m_entries.size()) return m_entries[id];
        return nullptr;
    }

    T* getByName(const ResourceLocation& loc) const {
        auto it = m_byName.find(loc);
        if (it == m_byName.end()) return nullptr;
        return m_entries[it->second];
    }

    std::optional<uint32_t> getId(const ResourceLocation& loc) const {
        auto it = m_byName.find(loc);
        if (it == m_byName.end()) return std::nullopt;
        return it->second;
    }

    const ResourceLocation* getName(uint32_t id) const {
        auto it = m_nameById.find(id);
        if (it == m_nameById.end()) return nullptr;
        return &it->second;
    }

    size_t size() const { return m_entries.size(); }

    auto begin() const { return m_entries.begin(); }
    auto end()   const { return m_entries.end(); }

private:
    std::vector<T*>                              m_entries;
    std::unordered_map<ResourceLocation, uint32_t> m_byName;
    std::unordered_map<uint32_t, ResourceLocation> m_nameById;
    std::unordered_map<uint32_t, T*>               m_byId;
};

} // namespace mc

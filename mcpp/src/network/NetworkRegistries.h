// NetworkRegistries — the engine's source of network VarInt ids for registry-held types.
//
// On the play-protocol wire, a registry value (SoundEvent, MobEffect, MenuType,
// EntityType, ParticleType, DataComponentType, Attribute, ...) is encoded as a single
// VarInt of its BuiltInRegistries.<R>.getId(value) — the 0-based insertion (registration)
// order index (ByteBufCodecs.registry/holderRegistry/holder; MappedRegistry.getId). For a
// 1:1 port the C++ id MUST equal the vanilla id, which is only true if our table is built
// in the EXACT registration order the jar uses.
//
// We therefore do NOT guess or alphabetize: the order is captured verbatim from the real
// jar by tools/RegistryNetworkIdDump.java into assets/network_registries.tsv (rows
// "E <registry> <id> <ns:path>", ids 0..count-1), and the C++ side just loads that table.
// The network_registries_parity gate regenerates the dump fresh from the jar and asserts
// our loaded table is byte-identical, so the snapshot can never silently drift from vanilla.
//
// This is a name<->id table only (no per-type object data) — exactly what the wire needs.
#pragma once

#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc::net {

class NetworkRegistries {
public:
    // Load the "E <registry> <id> <ns:path>" rows from a network_registries.tsv asset.
    // Returns false if the file can't be opened. Ids must be dense 0..count-1 in file
    // order (the order IS the wire order); we assert that as we load.
    bool loadFromFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        m_byName.clear();
        m_byId.clear();
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("E\t", 0) != 0) continue;  // only entry rows
            // E \t <registry> \t <id> \t <name>
            std::stringstream ss(line);
            std::string tag, reg, idStr, name;
            std::getline(ss, tag, '\t');
            std::getline(ss, reg, '\t');
            std::getline(ss, idStr, '\t');
            std::getline(ss, name, '\t');
            if (reg.empty() || name.empty()) continue;
            int32_t id = (int32_t)std::stol(idStr);
            auto& vec = m_byId[reg];
            // dense, in-order: the row's id must equal the next slot.
            if ((int32_t)vec.size() != id) { m_loadOrderOk = false; }
            vec.push_back(name);
            m_byName[reg][name] = id;
        }
        m_loaded = true;
        return true;
    }

    // The wire VarInt id for a registry entry (ns:path), or nullopt if absent. This is
    // exactly BuiltInRegistries.<registry>.getId(value) for the ported registries.
    std::optional<int32_t> id(const std::string& registry, const std::string& entry) const {
        auto r = m_byName.find(registry);
        if (r == m_byName.end()) return std::nullopt;
        auto e = r->second.find(entry);
        if (e == r->second.end()) return std::nullopt;
        return e->second;
    }

    // The registry entry name (ns:path) for a wire id, or nullopt if out of range.
    std::optional<std::string> name(const std::string& registry, int32_t id) const {
        auto r = m_byId.find(registry);
        if (r == m_byId.end()) return std::nullopt;
        if (id < 0 || (size_t)id >= r->second.size()) return std::nullopt;
        return r->second[(size_t)id];
    }

    size_t size(const std::string& registry) const {
        auto r = m_byId.find(registry);
        return r == m_byId.end() ? 0 : r->second.size();
    }

    bool loaded() const { return m_loaded; }
    bool loadOrderOk() const { return m_loadOrderOk; }

private:
    std::unordered_map<std::string, std::unordered_map<std::string, int32_t>> m_byName;
    std::unordered_map<std::string, std::vector<std::string>> m_byId;  // index == wire id
    bool m_loaded = false;
    bool m_loadOrderOk = true;
};

}  // namespace mc::net

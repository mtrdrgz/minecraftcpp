#pragma once

// Tiny helpers for the canonical block-state id strings used across worldgen
// (e.g. "minecraft:tall_grass[half=lower]"). Properties are serialised sorted by
// name, matching Java's BlockStateParser.serialize.

#include <map>
#include <string>

namespace mc::block {

// Block id without properties: "minecraft:x[half=lower]" -> "minecraft:x".
inline std::string blockName(const std::string& state) {
    const auto b = state.find('[');
    return b == std::string::npos ? state : state.substr(0, b);
}

inline std::map<std::string, std::string> properties(const std::string& state) {
    std::map<std::string, std::string> props;
    const auto b = state.find('[');
    if (b == std::string::npos) {
        return props;
    }
    const auto e = state.find(']', b);
    std::string body = state.substr(b + 1, e - b - 1);
    std::size_t i = 0;
    while (i < body.size()) {
        const auto comma = body.find(',', i);
        const std::string kv = body.substr(i, comma == std::string::npos ? std::string::npos : comma - i);
        const auto eq = kv.find('=');
        if (eq != std::string::npos) {
            props[kv.substr(0, eq)] = kv.substr(eq + 1);
        }
        if (comma == std::string::npos) break;
        i = comma + 1;
    }
    return props;
}

inline std::string serializeState(const std::string& name, const std::map<std::string, std::string>& props) {
    if (props.empty()) {
        return name;
    }
    std::string out = name + "[";
    bool first = true;
    for (const auto& [k, v] : props) { // std::map => sorted by key, like Java
        if (!first) out += ',';
        out += k + "=" + v;
        first = false;
    }
    out += "]";
    return out;
}

// state.setValue(key, val), re-serialised canonically.
inline std::string setProperty(const std::string& state, const std::string& key, const std::string& val) {
    auto props = properties(state);
    props[key] = val;
    return serializeState(blockName(state), props);
}

} // namespace mc::block

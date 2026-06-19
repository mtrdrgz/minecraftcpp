#pragma once
#include <string>
#include <string_view>
#include <functional>

namespace mc {

// Direct port of net.minecraft.resources.ResourceLocation
// Format: "namespace:path"  (default namespace = "minecraft")
struct ResourceLocation {
    std::string ns;
    std::string path;

    ResourceLocation() = default;
    ResourceLocation(std::string_view ns, std::string_view path) : ns(ns), path(path) {}

    // Parse "namespace:path" or just "path" (uses "minecraft" ns)
    static ResourceLocation parse(std::string_view str) {
        auto colon = str.find(':');
        if (colon == std::string_view::npos)
            return {"minecraft", std::string(str)};
        return {std::string(str.substr(0, colon)), std::string(str.substr(colon + 1))};
    }

    std::string toString() const { return ns + ":" + path; }
    bool operator==(const ResourceLocation&) const = default;
    bool operator<(const ResourceLocation& o) const {
        if (ns != o.ns) return ns < o.ns;
        return path < o.path;
    }
};

} // namespace mc

namespace std {
template<> struct hash<mc::ResourceLocation> {
    size_t operator()(const mc::ResourceLocation& r) const noexcept {
        size_t h = std::hash<std::string>{}(r.ns);
        h ^= std::hash<std::string>{}(r.path) + 0x9e3779b9 + (h<<6) + (h>>2);
        return h;
    }
};
} // namespace std

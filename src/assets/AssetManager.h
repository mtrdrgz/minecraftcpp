#pragma once
#include <string_view>
#include <string>
#include <vector>
#include <cstdint>

namespace mc {

// Singleton asset reader — resolves asset paths to raw bytes.
// Reads from the embedded assets.bin resource (a zip archive).
class AssetManager {
public:
    static AssetManager& instance();

    // Returns raw bytes for the given asset path, empty if not found.
    std::vector<uint8_t> readRaw(std::string_view path);

    // Returns all packed paths that start with `prefix`, sorted by pack order.
    std::vector<std::string> list(std::string_view prefix);
};

} // namespace mc

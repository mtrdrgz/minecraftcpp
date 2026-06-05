#pragma once
#include <cstdint>
#include <span>

namespace mc {

// Loads the assets.bin embedded Windows resource and exposes raw span access.
class AssetPack {
public:
    static bool init();
    static void shutdown();

    static std::span<const uint8_t> data();
};

} // namespace mc

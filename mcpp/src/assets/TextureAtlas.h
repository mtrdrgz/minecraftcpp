#pragma once
#include "../render/IRenderDevice.h"
#include <string>
#include <unordered_map>
#include <cstdint>
#include <span>

namespace mc {

struct AtlasUV {
    float u0 = 0.f, v0 = 0.f, u1 = 0.0625f, v1 = 0.0625f;
};

// Loads block_atlas.png + block_atlas.json and provides UV lookups by texture name.
class TextureAtlas {
public:
    TextureAtlas() = default;
    ~TextureAtlas() = default;

    void load(render::IRenderDevice* dev, render::ICommandList* cmd,
              std::span<const uint8_t> pngData,
              std::span<const uint8_t> jsonData);

    bool              isLoaded() const { return m_loaded; }
    render::ITexture* texture()  const { return m_texture; }

    const AtlasUV* uv(const std::string& name) const;
    const AtlasUV& missingUV() const { return m_missing; }

private:
    render::ITexture* m_texture = nullptr;
    bool              m_loaded  = false;
    std::unordered_map<std::string, AtlasUV> m_uvMap;
    AtlasUV m_missing{};
};

} // namespace mc

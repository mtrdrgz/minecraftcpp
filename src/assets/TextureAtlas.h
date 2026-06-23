#pragma once
#include "../render/IRenderDevice.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
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

    // Advance animated sprites (water/lava/fire/...) to the frame for `timeSeconds`
    // and re-upload the atlas if any frame changed. Cheap no-op when nothing animates.
    void tickAnimations(render::ICommandList* cmd, double timeSeconds);

private:
    bool loadFromAssetPack(render::IRenderDevice* dev, render::ICommandList* cmd);

    // One animated sprite: its 16x16 tile origin in the atlas, the decoded frames, and
    // the .mcmeta sequence (frameIndex, durationTicks). Port of the vanilla texture
    // animation (TextureAtlasSprite.AnimatedTexture): discrete frames, no interpolation.
    struct AnimatedSprite {
        int x0 = 0, y0 = 0;
        std::vector<std::vector<uint8_t>> frames;   // each TILE*TILE*4 RGBA
        std::vector<std::pair<int, int>> seq;       // (frameIndex, durationTicks)
        int totalTicks = 0;
        int lastFrame = -1;
    };

    render::ITexture* m_texture = nullptr;
    bool              m_loaded  = false;
    std::unordered_map<std::string, AtlasUV> m_uvMap;
    AtlasUV m_missing{};

    // CPU-side atlas kept for in-place animation updates (loadFromAssetPack path).
    std::vector<uint8_t>         m_atlasPixels;
    int                          m_atlasW = 0, m_atlasH = 0;
    std::vector<AnimatedSprite>  m_animated;
};

} // namespace mc

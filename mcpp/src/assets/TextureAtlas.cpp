#include "TextureAtlas.h"
#include "../core/Log.h"
#include <nlohmann/json.hpp>
#include <stb_image.h>

namespace mc {

void TextureAtlas::load(render::IRenderDevice* dev, render::ICommandList* cmd,
                        std::span<const uint8_t> pngData,
                        std::span<const uint8_t> jsonData) {
    if (pngData.empty() || jsonData.empty()) return;

    int w = 0, h = 0, ch = 0;
    uint8_t* pixels = stbi_load_from_memory(pngData.data(), (int)pngData.size(),
                                             &w, &h, &ch, 4);
    if (!pixels) {
        MC_LOG_WARN("TextureAtlas: PNG decode failed");
        return;
    }

    render::TextureDesc desc;
    desc.width     = (uint32_t)w;
    desc.height    = (uint32_t)h;
    desc.format    = render::TextureFormat::RGBA8;
    desc.filter    = render::FilterMode::Nearest;
    desc.genMipmaps = false;
    m_texture = dev->createTexture(desc);
    if (m_texture) cmd->uploadTexture(m_texture, pixels);
    stbi_image_free(pixels);

    if (!m_texture) {
        MC_LOG_WARN("TextureAtlas: GPU texture creation failed");
        return;
    }

    try {
        auto j = nlohmann::json::parse(jsonData.begin(), jsonData.end());
        for (auto& [name, arr] : j.value("textures", nlohmann::json::object()).items()) {
            if (arr.is_array() && arr.size() == 4) {
                AtlasUV auv;
                auv.u0 = arr[0].get<float>();
                auv.v0 = arr[1].get<float>();
                auv.u1 = arr[2].get<float>();
                auv.v1 = arr[3].get<float>();
                m_uvMap[name] = auv;
            }
        }
    } catch (const std::exception& e) {
        MC_LOG_WARN("TextureAtlas: JSON parse failed: {}", e.what());
    }

    m_loaded = true;
    MC_LOG_INFO("TextureAtlas: loaded {}x{} atlas, {} entries", w, h, m_uvMap.size());
}

const AtlasUV* TextureAtlas::uv(const std::string& name) const {
    auto it = m_uvMap.find(name);
    return it != m_uvMap.end() ? &it->second : nullptr;
}

} // namespace mc

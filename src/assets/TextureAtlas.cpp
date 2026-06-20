#include "TextureAtlas.h"
#include "AssetManager.h"
#include "../core/Log.h"
#include "../world/level/block/Blocks.h"
#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace mc {

namespace {
constexpr int TILE_SIZE = 16;
constexpr int ATLAS_COLS = 32;

void addTextureName(std::set<std::string>& names, const std::string& name) {
    if (!name.empty()) names.insert(name);
}

std::vector<std::string> collectBlockTextureNames() {
    std::set<std::string> names;
    for (const auto& blockPtr : g_blockStorage) {
        if (!blockPtr) continue;
        addTextureName(names, blockPtr->textures.all);
        addTextureName(names, blockPtr->textures.top);
        addTextureName(names, blockPtr->textures.bot);
        addTextureName(names, blockPtr->textures.side);
    }
    names.insert("missingno");
    names.insert("water_still");
    names.insert("water_flow");
    names.insert("lava_still");
    names.insert("lava_flow");
    return std::vector<std::string>(names.begin(), names.end());
}

bool decodePng(const std::vector<uint8_t>& bytes, int& w, int& h, std::vector<uint8_t>& rgba) {
    if (bytes.empty()) return false;
    int ch = 0;
    uint8_t* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &ch, 4);
    if (!pixels || w <= 0 || h <= 0) {
        if (pixels) stbi_image_free(pixels);
        return false;
    }
    rgba.assign(pixels, pixels + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
    stbi_image_free(pixels);
    return true;
}

void writeMissingTile(std::vector<uint8_t>& atlas, int atlasW, int x0, int y0) {
    for (int y = 0; y < TILE_SIZE; ++y) {
        for (int x = 0; x < TILE_SIZE; ++x) {
            const bool alt = ((x / 4) + (y / 4)) & 1;
            uint8_t* dst = atlas.data() + ((y0 + y) * atlasW + (x0 + x)) * 4;
            dst[0] = alt ? 0 : 255;
            dst[1] = 0;
            dst[2] = alt ? 0 : 255;
            dst[3] = 255;
        }
    }
}

void copyTopLeftTile(std::vector<uint8_t>& atlas, int atlasW, int x0, int y0,
                     const std::vector<uint8_t>& src, int srcW, int srcH) {
    for (int y = 0; y < TILE_SIZE; ++y) {
        const int sy = std::min(y, srcH - 1);
        for (int x = 0; x < TILE_SIZE; ++x) {
            const int sx = std::min(x, srcW - 1);
            const uint8_t* sp = src.data() + (sy * srcW + sx) * 4;
            uint8_t* dp = atlas.data() + ((y0 + y) * atlasW + (x0 + x)) * 4;
            dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = sp[3];
        }
    }
}
} // namespace

void TextureAtlas::load(render::IRenderDevice* dev, render::ICommandList* cmd,
                        std::span<const uint8_t> pngData,
                        std::span<const uint8_t> jsonData) {
    if (pngData.empty() || jsonData.empty()) {
        loadFromAssetPack(dev, cmd);
        return;
    }

    int w = 0, h = 0, ch = 0;
    uint8_t* pixels = stbi_load_from_memory(pngData.data(), (int)pngData.size(),
                                             &w, &h, &ch, 4);
    if (!pixels) {
        MC_LOG_WARN("TextureAtlas: embedded PNG decode failed, trying assets.bin fallback");
        loadFromAssetPack(dev, cmd);
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
                if (name == "__missing__" || name == "missing_texture" || name == "missingno") {
                    m_missing = auv;
                }
            }
        }
    } catch (const std::exception& e) {
        MC_LOG_WARN("TextureAtlas: JSON parse failed: {}", e.what());
    }

    if (m_uvMap.empty()) {
        MC_LOG_WARN("TextureAtlas: embedded atlas has no UVs, trying assets.bin fallback");
        if (m_texture) dev->destroyTexture(m_texture);
        m_texture = nullptr;
        loadFromAssetPack(dev, cmd);
        return;
    }

    m_loaded = true;
    MC_LOG_INFO("TextureAtlas: loaded {}x{} atlas, {} entries", w, h, m_uvMap.size());
}

bool TextureAtlas::loadFromAssetPack(render::IRenderDevice* dev, render::ICommandList* cmd) {
    std::vector<std::string> names = collectBlockTextureNames();
    if (names.empty()) return false;

    const int rows = std::max(1, (static_cast<int>(names.size()) + ATLAS_COLS - 1) / ATLAS_COLS);
    const int atlasW = ATLAS_COLS * TILE_SIZE;
    const int atlasH = rows * TILE_SIZE;
    std::vector<uint8_t> atlas(static_cast<std::size_t>(atlasW) * static_cast<std::size_t>(atlasH) * 4u, 0);

    m_uvMap.clear();
    int loaded = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        const std::string& name = names[i];
        const int col = static_cast<int>(i % ATLAS_COLS);
        const int row = static_cast<int>(i / ATLAS_COLS);
        const int x0 = col * TILE_SIZE;
        const int y0 = row * TILE_SIZE;

        int tw = 0, th = 0;
        std::vector<uint8_t> rgba;
        const std::vector<uint8_t> bytes = AssetManager::instance().readRaw("minecraft/textures/block/" + name + ".png");
        if (decodePng(bytes, tw, th, rgba)) {
            copyTopLeftTile(atlas, atlasW, x0, y0, rgba, tw, th);
            ++loaded;
        } else {
            writeMissingTile(atlas, atlasW, x0, y0);
        }

        AtlasUV uv;
        uv.u0 = static_cast<float>(x0) / static_cast<float>(atlasW);
        uv.v0 = static_cast<float>(y0) / static_cast<float>(atlasH);
        uv.u1 = static_cast<float>(x0 + TILE_SIZE) / static_cast<float>(atlasW);
        uv.v1 = static_cast<float>(y0 + TILE_SIZE) / static_cast<float>(atlasH);
        m_uvMap[name] = uv;
        if (name == "missingno") m_missing = uv;
    }

    if (loaded == 0) {
        MC_LOG_WARN("TextureAtlas: assets.bin fallback found no block textures");
        return false;
    }

    render::TextureDesc desc;
    desc.width = static_cast<uint32_t>(atlasW);
    desc.height = static_cast<uint32_t>(atlasH);
    desc.format = render::TextureFormat::RGBA8;
    desc.filter = render::FilterMode::Nearest;
    desc.genMipmaps = false;
    m_texture = dev->createTexture(desc);
    if (!m_texture) return false;
    cmd->uploadTexture(m_texture, atlas.data());
    m_loaded = true;
    MC_LOG_INFO("TextureAtlas: built {}x{} atlas from assets.bin, {} of {} textures loaded",
                atlasW, atlasH, loaded, names.size());
    return true;
}

const AtlasUV* TextureAtlas::uv(const std::string& name) const {
    auto it = m_uvMap.find(name);
    return it != m_uvMap.end() ? &it->second : nullptr;
}

} // namespace mc

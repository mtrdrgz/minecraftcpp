#include "SoundManager.h"
#include "../assets/AssetManager.h"
#include "../core/Log.h"

namespace mc::audio {

SoundManager::SoundManager() {
    for (int i = 0; i < (int)SoundSource::COUNT; ++i) {
        m_volumes[(SoundSource)i] = 1.0f;
    }
}

void SoundManager::init() {
    SoundEngine::instance().init();
    
    // Register some default sounds for testing/skeleton
    // In a real port, these would be loaded from a JSON registry like blocks.
    m_registry.register_(ResourceLocation::parse("entity.player.hurt"), new SoundEvent{ResourceLocation::parse("minecraft:sounds/mob/player/hurt1.ogg")});
    m_registry.register_(ResourceLocation::parse("block.grass.step"), new SoundEvent{ResourceLocation::parse("minecraft:sounds/step/grass1.ogg")});
    m_registry.register_(ResourceLocation::parse("ui.button.click"), new SoundEvent{ResourceLocation::parse("minecraft:sounds/random/click.ogg")});
}

void SoundManager::play(uint32_t soundEventId, SoundSource source, double x, double y, double z, float volume, float pitch) {
    SoundEvent* evt = m_registry.getById(soundEventId);
    if (evt) {
        play(evt->location, source, x, y, z, volume, pitch);
    }
}

void SoundManager::play(const ResourceLocation& name, SoundSource source, double x, double y, double z, float volume, float pitch) {
    const OggData& data = getOrLoadSound(name);
    if (data.empty()) return;

    float categoryVol = m_volumes[source] * m_volumes[SoundSource::MASTER];
    if (categoryVol <= 0.001f) return;

    SoundEngine::instance().playAt(data, x, y, z, volume * categoryVol, pitch);
}

void SoundManager::stop(const ResourceLocation* name, SoundSource* source) {
    // Basic implementation: stop everything if both null, or just stopAll for now
    // A proper implementation would track handles per name/source.
    if (!name && !source) {
        SoundEngine::instance().stopAll();
    }
}

void SoundManager::setVolume(SoundSource source, float volume) {
    m_volumes[source] = volume;
}

float SoundManager::getVolume(SoundSource source) const {
    auto it = m_volumes.find(source);
    return it != m_volumes.end() ? it->second : 1.0f;
}

const OggData& SoundManager::getOrLoadSound(const ResourceLocation& location) {
    auto it = m_cache.find(location);
    if (it != m_cache.end()) return it->second;

    // Map ResourceLocation to asset path
    // In MC assets, sound "minecraft:entity.player.hurt" might be at "minecraft/sounds/mob/hurt1.ogg"
    // The SoundEvent structure above already has the .ogg path in its 'location' for this skeleton.
    std::string path = location.path;
    if (path.find(".ogg") == std::string::npos) {
        path = "minecraft/sounds/" + path + ".ogg";
    } else if (path.find("minecraft/") == 0) {
        // already has namespace
    } else {
        path = location.ns + "/" + path;
    }

    auto bytes = AssetManager::instance().readRaw(path);
    if (bytes.empty()) {
        MC_LOG_WARN("Sound asset not found: {}", path);
        return m_emptySound;
    }

    OggData data = decodeOgg(bytes);
    if (data.empty()) {
        return m_emptySound;
    }

    return m_cache[location] = std::move(data);
}

} // namespace mc::audio

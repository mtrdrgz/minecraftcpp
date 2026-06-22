#pragma once
#include "SoundSource.h"
#include "SoundEvent.h"
#include "OggDecoder.h"
#include "core/Registry.h"
#include <unordered_map>
#include <map>

#ifdef _WIN32
#include "SoundEngine.h"
#endif

namespace mc::audio {

// Port of net.minecraft.client.sounds.SoundManager
class SoundManager {
public:
    SoundManager();
    ~SoundManager() = default;

    void init();

    // Play sound from registry by ID (network ID)
    void play(uint32_t soundEventId, SoundSource source, double x, double y, double z, float volume, float pitch);

    // Play sound from registry by name
    void play(const ResourceLocation& name, SoundSource source, double x, double y, double z, float volume, float pitch);

    // Stop sounds
    void stop(const ResourceLocation* name, SoundSource* source);

    // Volume management
    void setVolume(SoundSource source, float volume);
    float getVolume(SoundSource source) const;

    // Listener position (for 3D audio). On Linux this is currently a no-op
    // because the audio backend (XAudio2) is Windows-only.
    void setListenerPosition(double, double, double, float, float) {}

    Registry<SoundEvent>& registry() { return m_registry; }

private:
    const OggData& getOrLoadSound(const ResourceLocation& location);

    Registry<SoundEvent> m_registry;
    std::unordered_map<ResourceLocation, OggData> m_cache;
    std::map<SoundSource, float> m_volumes;

    OggData m_emptySound;
};

} // namespace mc::audio

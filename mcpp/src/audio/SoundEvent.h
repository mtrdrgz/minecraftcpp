#pragma once
#include "core/ResourceLocation.h"
#include <optional>

namespace mc::audio {

// Port of net.minecraft.sounds.SoundEvent
struct SoundEvent {
    ResourceLocation location;
    std::optional<float> fixedRange;
    uint32_t registryId = 0; // Filled by registry

    float getRange(float volume) const {
        return fixedRange.value_or(volume > 1.0f ? 16.0f * volume : 16.0f);
    }
};

} // namespace mc::audio

#pragma once
#include <cstdint>

namespace mc::audio {

// Port of net.minecraft.sounds.SoundSource
enum class SoundSource : int32_t {
    MASTER = 0,
    MUSIC = 1,
    RECORDS = 2,
    WEATHER = 3,
    BLOCKS = 4,
    HOSTILE = 5,
    NEUTRAL = 6,
    PLAYERS = 7,
    AMBIENT = 8,
    VOICE = 9,
    COUNT
};

} // namespace mc::audio

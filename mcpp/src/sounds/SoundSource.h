#pragma once
// Port of net.minecraft.sounds.SoundSource (MC 26.1.2).
//
// Java source (26.1.2/src/net/minecraft/sounds/SoundSource.java):
//   public enum SoundSource {
//      MASTER("master"), MUSIC("music"), RECORDS("record"), WEATHER("weather"),
//      BLOCKS("block"), HOSTILE("hostile"), NEUTRAL("neutral"), PLAYERS("player"),
//      AMBIENT("ambient"), VOICE("voice"), UI("ui");
//      private final String name;
//      SoundSource(final String name) { this.name = name; }
//      public String getName() { return this.name; }
//   }
//
// NOTE: a partial enum exists at mcpp/src/audio/SoundSource.h but it is missing the
// UI constant and the getName() name strings; this header is the faithful 1:1 port.
// SoundSource does NOT implement StringRepresentable, so there is no getSerializedName().

#include <cstdint>
#include <string_view>

namespace mc::sounds {

// Ordinals follow declaration order, exactly as Java enum ordinal().
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
    UI = 10,
};

// Number of enum constants (SoundSource.values().length).
inline constexpr int32_t SOUND_SOURCE_COUNT = 11;

// Java enum constant identifier, i.e. name() — the UPPERCASE declared name.
inline constexpr std::string_view soundSourceName(SoundSource v) {
    switch (v) {
        case SoundSource::MASTER:  return "MASTER";
        case SoundSource::MUSIC:   return "MUSIC";
        case SoundSource::RECORDS: return "RECORDS";
        case SoundSource::WEATHER: return "WEATHER";
        case SoundSource::BLOCKS:  return "BLOCKS";
        case SoundSource::HOSTILE: return "HOSTILE";
        case SoundSource::NEUTRAL: return "NEUTRAL";
        case SoundSource::PLAYERS: return "PLAYERS";
        case SoundSource::AMBIENT: return "AMBIENT";
        case SoundSource::VOICE:   return "VOICE";
        case SoundSource::UI:      return "UI";
    }
    return {};
}

// Java SoundSource.getName() — the lowercase serialized identifier passed to the ctor.
inline constexpr std::string_view soundSourceGetName(SoundSource v) {
    switch (v) {
        case SoundSource::MASTER:  return "master";
        case SoundSource::MUSIC:   return "music";
        case SoundSource::RECORDS: return "record";
        case SoundSource::WEATHER: return "weather";
        case SoundSource::BLOCKS:  return "block";
        case SoundSource::HOSTILE: return "hostile";
        case SoundSource::NEUTRAL: return "neutral";
        case SoundSource::PLAYERS: return "player";
        case SoundSource::AMBIENT: return "ambient";
        case SoundSource::VOICE:   return "voice";
        case SoundSource::UI:      return "ui";
    }
    return {};
}

} // namespace mc::sounds

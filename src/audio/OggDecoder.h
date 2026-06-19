#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace mc::audio {

// Decoded PCM payload returned by decodeOgg().
// `samples` is interleaved 16-bit signed PCM (LRLRLR... for stereo).
// On failure, all fields are zero / empty.
struct OggData {
    std::vector<int16_t> samples;
    int                  sampleRate = 0;
    int                  channels   = 0;

    bool empty() const noexcept { return samples.empty(); }
};

// One-shot decode of an entire .ogg file held in memory.
// Implemented with stb_vorbis_decode_memory() (vendored as vendor/stb/stb_vorbis.c).
// Streaming / push decoders are out of scope for the Phase 12 skeleton.
//
// Returns an empty OggData on failure (with a warning logged via MC_LOG_WARN).
//
// TODO: if vendor/stb/stb_vorbis.c is missing from the build, this function
//       falls back to a stub that always returns empty OggData. The missing
//       dependency is "stb_vorbis.c" — vendor it under vendor/stb/.
OggData decodeOgg(std::span<const uint8_t> data);

} // namespace mc::audio

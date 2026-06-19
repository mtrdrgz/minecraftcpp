#include "OggDecoder.h"
#include "../core/Log.h"

#include <cstdlib>

// ── stb_vorbis dependency check ─────────────────────────────────────────────
// The decoder is gated on __has_include so the file still compiles cleanly if
// stb_vorbis.c is ever removed from vendor/stb/. In that case decodeOgg()
// becomes a stub that logs a warning and returns empty OggData.
#if __has_include(<stb_vorbis.c>) || __has_include("stb_vorbis.c") || __has_include("../../vendor/stb/stb_vorbis.c")
    #define MC_HAVE_STB_VORBIS 1
#else
    #define MC_HAVE_STB_VORBIS 0
#endif

#if MC_HAVE_STB_VORBIS
    // stb_vorbis is a single-translation-unit library. We only declare the
    // function we need here; the .c file itself is compiled exactly once in
    // CMakeLists.txt (as a C source file) so we don't pull it into this TU.
    extern "C" int stb_vorbis_decode_memory(const unsigned char* mem,
                                            int                  len,
                                            int*                 channels,
                                            int*                 sample_rate,
                                            short**              output);
#endif

namespace mc::audio {

OggData decodeOgg(std::span<const uint8_t> data) {
#if !MC_HAVE_STB_VORBIS
    MC_LOG_WARN("decodeOgg: stb_vorbis not vendored — returning empty OggData (data.size={} bytes)",
                data.size());
    return {};
#else
    if (data.empty()) {
        MC_LOG_WARN("decodeOgg: empty input buffer");
        return {};
    }
    if (data.size() > static_cast<size_t>(INT32_MAX)) {
        MC_LOG_WARN("decodeOgg: input too large ({} bytes > INT_MAX)", data.size());
        return {};
    }

    int    channels    = 0;
    int    sampleRate  = 0;
    short* pcm         = nullptr;

    // Returns the number of samples PER CHANNEL on success, or a negative
    // value on failure.
    int sampleCount = stb_vorbis_decode_memory(data.data(),
                                               static_cast<int>(data.size()),
                                               &channels,
                                               &sampleRate,
                                               &pcm);
    if (sampleCount <= 0 || pcm == nullptr) {
        MC_LOG_WARN("decodeOgg: stb_vorbis_decode_memory failed (returned {})", sampleCount);
        if (pcm) std::free(pcm);
        return {};
    }
    if (channels < 1 || channels > 2) {
        MC_LOG_WARN("decodeOgg: unsupported channel count {} (only mono/stereo supported)", channels);
        std::free(pcm);
        return {};
    }

    OggData out;
    out.sampleRate = sampleRate;
    out.channels   = channels;
    const size_t totalSamples = static_cast<size_t>(sampleCount) * static_cast<size_t>(channels);
    out.samples.assign(pcm, pcm + totalSamples);

    std::free(pcm);  // stb_vorbis allocated with malloc()

    MC_LOG_DEBUG("decodeOgg: {} samples/ch, {} Hz, {} ch ({} bytes -> {} samples total)",
                 sampleCount, sampleRate, channels, data.size(), totalSamples);
    return out;
#endif
}

} // namespace mc::audio

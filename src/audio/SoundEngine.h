#pragma once
#include "OggDecoder.h"

#include <cstdint>
#include <memory>

namespace mc::audio {

// Opaque handle returned by playAt() / playGlobal(). Use it to stop a sound
// before it finishes playing. Handles are monotonic — once invalidated, they
// stay invalid; the engine never re-uses an ID.
struct SoundHandle {
    uint64_t id = 0;
    bool valid() const noexcept { return id != 0; }
};

// Singleton XAudio2-backed sound engine.
//
// Only the bare minimum needed for the Phase 12 skeleton: create a mastering
// voice, decode an OggData into a source voice + buffer, play it, optionally
// attenuate by 3D distance. Per-SoundEvent dispatch (port of SoundEngine.java)
// is later work and lives in audio/SoundManager.h/.cpp.
//
// TODO: if <xaudio2.h> is not available (missing Windows SDK component) the
//       implementation degrades to a no-op stub — init() still returns true,
//       playAt()/playGlobal() return an invalid SoundHandle. See SoundEngine.cpp.
class SoundEngine {
public:
    static SoundEngine& instance();

    // No copy / no move — singleton.
    SoundEngine(const SoundEngine&)            = delete;
    SoundEngine& operator=(const SoundEngine&) = delete;
    SoundEngine(SoundEngine&&)                 = delete;
    SoundEngine& operator=(SoundEngine&&)      = delete;

    // Initialises COM (multi-threaded), XAudio2 engine, and the mastering
    // voice. Safe to call multiple times — second call is a no-op and returns
    // the previous result.
    bool init();

    // Stops everything, releases the mastering voice and XAudio2 instance.
    // Safe to call without a prior init().
    void shutdown();

    // Play a sound positioned in 3D world space. Volume is attenuated linearly
    // by the listener distance (0..16 blocks). `pitch` is in semitones —
    // the resulting frequency ratio is 2^(pitch/12).
    SoundHandle playAt(const OggData& sound,
                       double x, double y, double z,
                       float volume = 1.0f,
                       float pitch  = 1.0f);

    // Play a sound globally (UI clicks, music). No spatialisation.
    SoundHandle playGlobal(const OggData& sound,
                           float volume = 1.0f,
                           float pitch  = 1.0f);

    // Update the listener pose used by playAt() for distance attenuation.
    // yaw/pitch are in degrees and currently unused (no HRTF yet) but kept
    // in the signature so callers don't churn when we add X3DAudio later.
    void setListenerPosition(double x, double y, double z,
                             float yawDeg, float pitchDeg);

    // Stop a previously-issued sound. No-op for invalid / already-stopped
    // handles.
    void stop(SoundHandle h);

    // Stop every currently-playing sound.
    void stopAll();

private:
    SoundEngine() = default;
    ~SoundEngine();

    // PIMPL keeps <xaudio2.h>, <windows.h>, <wrl/client.h> out of the public
    // header — only one .cpp pulls them in.
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace mc::audio

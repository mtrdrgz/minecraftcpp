#include "SoundEngine.h"
#include "../core/Log.h"

// ── xaudio2 availability check ──────────────────────────────────────────────
// xaudio2_9.dll ships with every supported version of Windows 10/11 and is
// part of the Windows 10 SDK. If the header isn't reachable the implementation
// falls back to a stub.
#if __has_include(<xaudio2.h>)
    #define MC_HAVE_XAUDIO2 1
#else
    #define MC_HAVE_XAUDIO2 0
#endif

#if MC_HAVE_XAUDIO2
    #include <windows.h>
    #include <wrl/client.h>
    #include <xaudio2.h>

    #include <atomic>
    #include <cmath>
    #include <mutex>
    #include <unordered_map>
    #include <vector>

    #pragma comment(lib, "ole32.lib")  // CoInitializeEx
#endif

namespace mc::audio {

// ── Impl ────────────────────────────────────────────────────────────────────
#if MC_HAVE_XAUDIO2

using Microsoft::WRL::ComPtr;

namespace {
    // Max distance in blocks for linear distance attenuation. Vanilla MC uses
    // a more complex model in net/minecraft/client/sounds/AttenuationType.java,
    // but for the Phase 12 skeleton we follow the brief: linear 0..16.
    constexpr float kMaxAttenuationDistance = 16.0f;

    // Convert semitone offset to XAudio2 frequency ratio.
    // The public API says "pitch" but semantically callers pass either a
    // multiplier (1.0 == unchanged) or a semitone delta. We treat 0.5..2.0
    // as a direct multiplier (matching SoundEngine.java's PITCH_MIN/PITCH_MAX)
    // since that's what the Java engine does — see SoundEngine.java line 47-48.
    inline float clampPitch(float p) {
        if (p < 0.5f) return 0.5f;
        if (p > 2.0f) return 2.0f;
        return p;
    }

    inline float clampVolume(float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }
}

struct SoundEngine::Impl {
    // One playing instance. We have to keep the PCM bytes alive for as long
    // as XAudio2 is reading from them, so the buffer is owned here.
    struct Voice {
        IXAudio2SourceVoice* voice = nullptr;
        std::vector<int16_t> pcm;          // copy of OggData::samples
        uint64_t             id   = 0;
    };

    ComPtr<IXAudio2>          xaudio2;
    IXAudio2MasteringVoice*   masterVoice = nullptr;  // released by xaudio2

    bool                      comInitialised = false;
    bool                      initialised    = false;

    std::mutex                         voicesMutex;
    std::unordered_map<uint64_t, Voice> voices;       // keyed by handle id
    std::atomic<uint64_t>              nextId{1};

    // Listener pose (world space, blocks)
    double  lx = 0.0, ly = 0.0, lz = 0.0;
    float   lyaw = 0.0f, lpitch = 0.0f;

    bool initEngine();
    void shutdownEngine();

    SoundHandle play(const OggData& sound, float volume, float pitch, float attenuation);
    void        stopVoice(uint64_t id);
    void        stopAllVoices();

    // Cleanup helper — call without holding voicesMutex.
    void destroyVoice(Voice& v);
};

bool SoundEngine::Impl::initEngine() {
    if (initialised) return true;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // RPC_E_CHANGED_MODE means COM was already initialised by someone else
    // with a different threading model — that's fine for our purposes,
    // we just don't own the uninit.
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        MC_LOG_ERROR("SoundEngine: CoInitializeEx failed (hr=0x{:08X})", (uint32_t)hr);
        return false;
    }
    comInitialised = SUCCEEDED(hr);  // we only Uninit if we successfully Init'd

    hr = XAudio2Create(xaudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        MC_LOG_ERROR("SoundEngine: XAudio2Create failed (hr=0x{:08X})", (uint32_t)hr);
        if (comInitialised) { CoUninitialize(); comInitialised = false; }
        return false;
    }

    hr = xaudio2->CreateMasteringVoice(&masterVoice);
    if (FAILED(hr)) {
        MC_LOG_ERROR("SoundEngine: CreateMasteringVoice failed (hr=0x{:08X})", (uint32_t)hr);
        xaudio2.Reset();
        if (comInitialised) { CoUninitialize(); comInitialised = false; }
        return false;
    }

    initialised = true;
    MC_LOG_INFO("SoundEngine: XAudio2 initialised");
    return true;
}

void SoundEngine::Impl::shutdownEngine() {
    if (!initialised) return;

    stopAllVoices();

    if (masterVoice) {
        masterVoice->DestroyVoice();
        masterVoice = nullptr;
    }
    xaudio2.Reset();

    if (comInitialised) {
        CoUninitialize();
        comInitialised = false;
    }
    initialised = false;
    MC_LOG_INFO("SoundEngine: shutdown complete");
}

void SoundEngine::Impl::destroyVoice(Voice& v) {
    if (v.voice) {
        v.voice->Stop(0);
        v.voice->FlushSourceBuffers();
        v.voice->DestroyVoice();
        v.voice = nullptr;
    }
}

SoundHandle SoundEngine::Impl::play(const OggData& sound,
                                    float volume,
                                    float pitch,
                                    float attenuation) {
    if (!initialised) {
        MC_LOG_WARN("SoundEngine::play: engine not initialised");
        return {};
    }
    if (sound.empty() || sound.channels < 1 || sound.channels > 2 || sound.sampleRate <= 0) {
        MC_LOG_WARN("SoundEngine::play: invalid OggData (channels={}, sr={}, samples={})",
                    sound.channels, sound.sampleRate, sound.samples.size());
        return {};
    }

    // Build wave format for 16-bit PCM.
    WAVEFORMATEX wf{};
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = static_cast<WORD>(sound.channels);
    wf.nSamplesPerSec  = static_cast<DWORD>(sound.sampleRate);
    wf.wBitsPerSample  = 16;
    wf.nBlockAlign     = wf.nChannels * (wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    wf.cbSize          = 0;

    IXAudio2SourceVoice* src = nullptr;
    HRESULT hr = xaudio2->CreateSourceVoice(&src, &wf);
    if (FAILED(hr) || !src) {
        MC_LOG_WARN("SoundEngine::play: CreateSourceVoice failed (hr=0x{:08X})", (uint32_t)hr);
        return {};
    }

    Voice voice;
    voice.id  = nextId.fetch_add(1, std::memory_order_relaxed);
    voice.pcm = sound.samples;  // copy — XAudio2 reads from this address asynchronously
    voice.voice = src;

    XAUDIO2_BUFFER buf{};
    buf.Flags      = XAUDIO2_END_OF_STREAM;
    buf.AudioBytes = static_cast<UINT32>(voice.pcm.size() * sizeof(int16_t));
    buf.pAudioData = reinterpret_cast<const BYTE*>(voice.pcm.data());
    buf.PlayBegin  = 0;
    buf.PlayLength = 0;  // play whole buffer
    buf.LoopCount  = 0;  // one-shot

    hr = src->SubmitSourceBuffer(&buf);
    if (FAILED(hr)) {
        MC_LOG_WARN("SoundEngine::play: SubmitSourceBuffer failed (hr=0x{:08X})", (uint32_t)hr);
        src->DestroyVoice();
        return {};
    }

    src->SetVolume(clampVolume(volume) * attenuation);
    src->SetFrequencyRatio(clampPitch(pitch));

    hr = src->Start(0);
    if (FAILED(hr)) {
        MC_LOG_WARN("SoundEngine::play: Start failed (hr=0x{:08X})", (uint32_t)hr);
        src->DestroyVoice();
        return {};
    }

    SoundHandle handle{ voice.id };
    {
        std::lock_guard<std::mutex> lock(voicesMutex);
        voices.emplace(voice.id, std::move(voice));
    }
    return handle;
}

void SoundEngine::Impl::stopVoice(uint64_t id) {
    Voice v;
    {
        std::lock_guard<std::mutex> lock(voicesMutex);
        auto it = voices.find(id);
        if (it == voices.end()) return;
        v = std::move(it->second);
        voices.erase(it);
    }
    destroyVoice(v);
}

void SoundEngine::Impl::stopAllVoices() {
    std::unordered_map<uint64_t, Voice> drained;
    {
        std::lock_guard<std::mutex> lock(voicesMutex);
        drained.swap(voices);
    }
    for (auto& [id, v] : drained) destroyVoice(v);
}

#else  // !MC_HAVE_XAUDIO2 — stub Impl

struct SoundEngine::Impl {
    // No-op stub. Kept as a struct so the PIMPL unique_ptr still works.
};

#endif  // MC_HAVE_XAUDIO2

// ── Public façade ───────────────────────────────────────────────────────────

SoundEngine& SoundEngine::instance() {
    static SoundEngine s;
    return s;
}

SoundEngine::~SoundEngine() {
    shutdown();
}

bool SoundEngine::init() {
#if MC_HAVE_XAUDIO2
    if (!m_impl) m_impl = std::make_unique<Impl>();
    if (m_impl->initialised) return true;
    return m_impl->initEngine();
#else
    // TODO: <xaudio2.h> not available — sound engine is a no-op stub.
    //       Install the Windows 10/11 SDK or vendor the legacy XAudio2 redist.
    MC_LOG_WARN("SoundEngine::init: <xaudio2.h> not available — running as stub");
    if (!m_impl) m_impl = std::make_unique<Impl>();
    return true;
#endif
}

void SoundEngine::shutdown() {
#if MC_HAVE_XAUDIO2
    if (m_impl) {
        m_impl->shutdownEngine();
        m_impl.reset();
    }
#else
    m_impl.reset();
#endif
}

SoundHandle SoundEngine::playAt(const OggData& sound,
                                double x, double y, double z,
                                float volume, float pitch) {
#if MC_HAVE_XAUDIO2
    if (!m_impl || !m_impl->initialised) return {};

    // Linear distance attenuation, 0..16 blocks.
    const double dx = x - m_impl->lx;
    const double dy = y - m_impl->ly;
    const double dz = z - m_impl->lz;
    const float  dist = static_cast<float>(std::sqrt(dx*dx + dy*dy + dz*dz));

    float atten = 1.0f - (dist / kMaxAttenuationDistance);
    if (atten <= 0.0f) {
        // Out of range — skip allocation entirely.
        return {};
    }
    if (atten > 1.0f) atten = 1.0f;

    return m_impl->play(sound, volume, pitch, atten);
#else
    (void)sound; (void)x; (void)y; (void)z; (void)volume; (void)pitch;
    return {};
#endif
}

SoundHandle SoundEngine::playGlobal(const OggData& sound,
                                    float volume, float pitch) {
#if MC_HAVE_XAUDIO2
    if (!m_impl || !m_impl->initialised) return {};
    return m_impl->play(sound, volume, pitch, /*attenuation=*/1.0f);
#else
    (void)sound; (void)volume; (void)pitch;
    return {};
#endif
}

void SoundEngine::setListenerPosition(double x, double y, double z,
                                      float yawDeg, float pitchDeg) {
#if MC_HAVE_XAUDIO2
    if (!m_impl) return;
    m_impl->lx     = x;
    m_impl->ly     = y;
    m_impl->lz     = z;
    m_impl->lyaw   = yawDeg;
    m_impl->lpitch = pitchDeg;
#else
    (void)x; (void)y; (void)z; (void)yawDeg; (void)pitchDeg;
#endif
}

void SoundEngine::stop(SoundHandle h) {
#if MC_HAVE_XAUDIO2
    if (!m_impl || !h.valid()) return;
    m_impl->stopVoice(h.id);
#else
    (void)h;
#endif
}

void SoundEngine::stopAll() {
#if MC_HAVE_XAUDIO2
    if (!m_impl) return;
    m_impl->stopAllVoices();
#else
    // stub
#endif
}

} // namespace mc::audio

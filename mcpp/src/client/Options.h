#pragma once
#include "../audio/SoundSource.h"
#include <array>

namespace mc {

// Minimal port of net.minecraft.client.Options — the values backing the option
// screens. Ranges/defaults are taken from the Java OptionInstance definitions
// (volumes 0..1, fov 30..110, sensitivity 0..2, render distance 2..32, gamma 0..1).
// Not yet persisted to disk; this is the in-memory store the widgets read/write.
struct GameOptions {
    std::array<float, (int)audio::SoundSource::COUNT> volume{}; // [0,1] per source; index by SoundSource

    double fov = 70.0;            // 30..110
    double sensitivity = 0.5;     // 0..1 -> shown as 0..200% (MC stores 0..1, *2 for display? no: 0..1, display 2x)
    int    renderDistance = 12;   // 2..32 chunks
    double gamma = 0.5;           // 0..1 (brightness)

    int  guiScale = 0;            // cycle index: 0=Auto,1,2,3,4
    int  graphics = 1;            // cycle: 0=Fast,1=Fancy,2=Fabulous!
    bool vsync = true;
    bool fullscreen = false;
    bool viewBobbing = true;

    bool invertYMouse = false;
    bool autoJump = false;
    bool discreteMouseScroll = false;
    bool touchscreen = false;

    bool showSubtitles = false;
    bool directionalAudio = false;

    GameOptions() { volume.fill(1.0f); }
};

} // namespace mc

// main.cpp — cross-platform entry point for mcpp.
// Windows: Win32 window + WGL/Vulkan/DX12
// Linux:   GLFW window + OpenGL

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include "platform/Platform.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#include "core/Log.h"
#include "platform/Window.h"
#include "assets/AssetPack.h"
#include "render/RenderBackend.h"
#include "render/level/LevelRenderer.h"
#include "client/Minecraft.h"
#include "world/level/block/Blocks.h"
#include "world/item/Items.h"
#include "debug/DebugOverlay.h"
#include <string>
#include <chrono>
#include <vector>
#include <optional>

static void parseCommandLine(int argc, char** argv,
                              std::string& host, uint16_t& port, std::string& user, std::string& backend,
                              bool& quickPlaySingleplayer, uint64_t& singleplayerSeed,
                              int& spawnX, int& spawnZ, std::optional<int>& spawnY) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--backend" && i + 1 < argc) {
            backend = argv[++i];
        } else if (arg == "--quickPlaySingleplayer" || arg == "--singleplayer") {
            quickPlaySingleplayer = true;
        } else if (arg == "--seed" && i + 1 < argc) {
            try { singleplayerSeed = (uint64_t)std::stoll(argv[++i]); } catch(...) {}
        } else if (arg == "--spawn" && i + 2 < argc) {
            try {
                spawnX = std::stoi(argv[++i]);
                spawnZ = std::stoi(argv[++i]);
            } catch(...) {}
        } else if (arg == "--spawnY" && i + 1 < argc) {
            try { spawnY = std::stoi(argv[++i]); } catch(...) {}
        } else if (arg.find("--") != 0) {
            if (host.empty()) host = arg;
            else if (port == 25565) {
                try { port = (uint16_t)std::stoi(arg); } catch(...) {}
            }
            else if (user == "mcpp_player") user = arg;
        }
    }
}

int main(int argc, char** argv) {

    MC_LOG_INFO("mcpp starting — Minecraft 26.1.2 C++ port");

    if (!mc::AssetPack::init()) {
        MC_LOG_ERROR("Critical Error: assets.bin not found or invalid");
#ifdef _WIN32
        MessageBoxA(nullptr, "Critical Error: assets.bin not found or invalid", "mcpp — Fatal Error", MB_ICONERROR);
#endif
        return 1;
    }

    try {
        mc::initBlocks();
        mc::initItems();
    } catch (const std::exception& e) {
        std::string msg = "Critical Error during block/item initialization: ";
        msg += e.what();
        MC_LOG_ERROR("{}", msg);
#ifdef _WIN32
        MessageBoxA(nullptr, msg.c_str(), "mcpp — Fatal Error", MB_ICONERROR);
#endif
        return 1;
    }

    mc::WindowDesc wdesc;
    wdesc.title  = "Minecraft 26.1.2 [mcpp] - C++ Port";
    wdesc.width  = 1280;
    wdesc.height = 720;
    mc::Window window(wdesc);

    std::string host = "";
    uint16_t    port = 25565;
    std::string user = "mcpp_player";
    std::string backendStr = "opengl";
    bool quickPlaySingleplayer = false;
    uint64_t singleplayerSeed = 0;
    int spawnX = 0;
    int spawnZ = 0;
    std::optional<int> spawnY;

    parseCommandLine(argc, argv, host, port, user, backendStr, quickPlaySingleplayer,
                     singleplayerSeed, spawnX, spawnZ, spawnY);

    mc::render::BackendType backendType = mc::render::RenderBackend::typeFromString(backendStr);
    auto device = mc::render::RenderBackend::createDevice(backendType, window.nativeHandle());

    if (!device) {
        std::string msg = "Failed to create " + backendStr + " context.";
        MC_LOG_ERROR("{}", msg);
        return 1;
    }

    MC_LOG_INFO("Using backend: {}", device->backendName());

    mc::Minecraft mc(&window, device.get());
    mc::render::LevelRenderer levelRenderer(device.get(), &mc, &window);

    if (quickPlaySingleplayer) {
        mc.startLocalGameFast(singleplayerSeed, spawnX, spawnZ, spawnY);
    } else if (!host.empty()) {
        mc.connectToServer(host, port, user);
    }

    MC_LOG_INFO("Entering main loop");

    using Clock = std::chrono::steady_clock;
    auto lastTick = Clock::now();
    constexpr double TICK_MS = 1000.0 / 20.0;
    double tickAccum = 0.0;

    // Debug overlay (F1 to toggle — development tool, not part of 1:1 port)
    mc::DebugOverlay debugOverlay;
    bool f1WasDown = false;
    bool f2WasDown = false;

    constexpr float SKY_R = 0.529f, SKY_G = 0.808f, SKY_B = 0.980f;
    bool escWasDown = false;

    while (window.pollEvents()) {
        const bool escDown = window.isKeyDown(VK_ESCAPE);
        if (escDown && !escWasDown) {
            if (mc.screen()) {
                mc.screen()->keyPressed(VK_ESCAPE, 0, 0);
            } else if (mc.isInGame()) {
                mc.openPauseScreen();
            }
        }
        escWasDown = escDown;

        // Debug overlay: F1 toggle, number keys 1/2/3 for tabs
        const bool f1Down = window.isKeyDown(VK_F1);
        if (f1Down && !f1WasDown) {
            debugOverlay.visible = !debugOverlay.visible;
            if (debugOverlay.visible) {
                window.captureMouse(false);
            } else if (mc.isInGame()) {
                window.captureMouse(true);
            }
        }
        f1WasDown = f1Down;

        if (debugOverlay.visible) {
            if (window.isKeyDown('1')) debugOverlay.currentTab = 0;
            else if (window.isKeyDown('2')) debugOverlay.currentTab = 1;
            else if (window.isKeyDown('3')) debugOverlay.currentTab = 2;
            const bool f2Down = window.isKeyDown(VK_F2);
            if (f2Down && !f2WasDown) {
                debugOverlay.currentTab = (debugOverlay.currentTab + 1) % 3;
            }
            f2WasDown = f2Down;
        }

        // Click handling
        const bool lButtonClick = window.consumeLButtonClicked();
        if (lButtonClick) {
            if (debugOverlay.visible) {
                debugOverlay.pendingClick = true;
            } else if (mc.screen()) {
                mc.screen()->mouseClicked(mc.guiMouseX(), mc.guiMouseY(), 0);
            } else if (mc.isInGame() && !window.isMouseCaptured()) {
                window.captureMouse(true);
            }
        }
        int dragDx = 0, dragDy = 0;
        if (mc.screen() && window.consumeMouseDrag(dragDx, dragDy)) {
            const double scale = (double)mc.guiScale();
            mc.screen()->mouseDragged(mc.guiMouseX(), mc.guiMouseY(), 0,
                                      (double)dragDx / scale, (double)dragDy / scale);
        }
        if (window.consumeLButtonReleased()) {
            if (mc.screen()) {
                mc.screen()->mouseReleased(mc.guiMouseX(), mc.guiMouseY(), 0);
            }
        }
        // Mouse wheel scroll — dispatch to the active screen (for scrolling lists).
        {
            double sx, sy;
            if (mc.screen() && window.consumeScroll(sx, sy)) {
                const double scale = (double)mc.guiScale();
                mc.screen()->mouseScrolled(mc.guiMouseX(), mc.guiMouseY(), sx / scale, sy / scale);
            }
        }

        auto now = Clock::now();
        double dtMs = std::chrono::duration<double, std::milli>(now - lastTick).count();
        lastTick = now;
        tickAccum += dtMs;

        constexpr int MAX_TICKS_PER_FRAME = 2;
        constexpr double MAX_TICK_ACCUM_MS = TICK_MS * 5.0;
        if (tickAccum > MAX_TICK_ACCUM_MS) tickAccum = MAX_TICK_ACCUM_MS;
        int ticksThisFrame = 0;
        while (tickAccum >= TICK_MS && ticksThisFrame < MAX_TICKS_PER_FRAME) {
            mc.tick();
            tickAccum -= TICK_MS;
            ++ticksThisFrame;
        }

        float partialTick = (float)(tickAccum / TICK_MS);
        mc.resizeGui();
        mc.render(partialTick);

        static int frames = 0;
        if (++frames % 100 == 0) MC_LOG_INFO("Main loop ticking, frames={}", frames);

        auto* cmd = device->beginFrame(window.width(), window.height());
        if (cmd) {
            cmd->clear(SKY_R, SKY_G, SKY_B, 1.0f, true);

            if (mc.isInGame()) {
                levelRenderer.renderLevel(cmd, partialTick);

                if (mc.gui() && mc.guiGraphics()) {
                    if (mc.screen()) {
                        mc.screen()->render(*mc.guiGraphics(), (int)mc.guiMouseX(), (int)mc.guiMouseY(), partialTick);
                    } else {
                        mc.gui()->render(*mc.guiGraphics(), partialTick);
                    }
                    // Debug overlay
                    if (debugOverlay.visible && mc.font()) {
                        debugOverlay.render(*mc.guiGraphics(), *mc.font(), mc, window,
                                             (float)(dtMs / 1000.0));
                    }
                    mc.guiGraphics()->render(cmd, (float)mc.guiScaledWidth(), (float)mc.guiScaledHeight());
                }
            } else if (mc.screen()) {
                mc.renderPanorama(cmd, window.width(), window.height(), (float)(dtMs / 1000.0));
                mc.screen()->render(*mc.guiGraphics(), (int)mc.guiMouseX(), (int)mc.guiMouseY(), partialTick);
                mc.guiGraphics()->render(cmd, (float)mc.guiScaledWidth(), (float)mc.guiScaledHeight());
            }

            device->endFrame();
        }
    }

    device->waitIdle();
    mc.disconnect();
    mc::AssetPack::shutdown();
    return 0;
}

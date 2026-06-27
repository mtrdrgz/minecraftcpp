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
#include "core/CrashHandler.h"
#include "core/FrameProfiler.h"
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
    // ── Open the log file FIRST, before anything else ──
    // The log file is mcpp.log, placed next to the executable (or in cwd).
    // Every MC_LOG_* call writes to both stdout and this file, with auto-flush
    // so a crash never loses the last few lines.
    {
        std::string logPath = "mcpp.log";
#ifdef _WIN32
        // On Windows, put the log next to the .exe (not in cwd, which might be
        // a system dir if launched from a shortcut).
        char exePath[MAX_PATH];
        DWORD len = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        if (len > 0) {
            std::string p(exePath, len);
            size_t slash = p.find_last_of("\\/");
            if (slash != std::string::npos) {
                logPath = p.substr(0, slash + 1) + "mcpp.log";
            }
        }
#endif
        mc::log::FileLogger::instance().open(logPath);
        MC_LOG_INFO("mcpp starting — Minecraft 26.1.2 C++ port");
        MC_LOG_INFO("log file: {}", mc::log::FileLogger::instance().path());
    }

    // ── Install crash handlers + hang watchdog ──
    // These write crash dumps (signal code + stack trace + phase) to mcpp.log
    // before the process dies, so we can debug crashes/hangs from the log.
    mc::debug::initCrashHandlers();
    mc::debug::startWatchdog();
    MC_LOG_INFO("crash handlers + hang watchdog installed");

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
        mc::debug::FrameProfiler::instance().begin("frame.total");
        mc::debug::frameHeartbeat();  // tell the watchdog we're alive
        mc::debug::setPhase("input");

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
        mc::debug::setPhase("tick");
        while (tickAccum >= TICK_MS && ticksThisFrame < MAX_TICKS_PER_FRAME) {
            mc::debug::FrameProfiler::begin("tick");
            mc.tick();
            mc::debug::FrameProfiler::end("tick");
            tickAccum -= TICK_MS;
            ++ticksThisFrame;
        }

        float partialTick = (float)(tickAccum / TICK_MS);
        mc::debug::setPhase("render");
        mc.resizeGui();
        mc::debug::FrameProfiler::begin("mc.render");
        mc.render(partialTick);
        mc::debug::FrameProfiler::end("mc.render");

        static int frames = 0;
        if (++frames % 100 == 0) MC_LOG_INFO("Main loop ticking, frames={}", frames);

        // Log slow frames (>100ms) with the phase so we can see what's slow.
        if (dtMs > 100.0) {
            MC_LOG_WARN("Slow frame: {:.1f}ms (phase=render, frames={})", dtMs, frames);
            // Dump the profiler immediately on a slow frame so we see what's slow.
            mc::debug::FrameProfiler::instance().dumpNow();
        }

        mc::debug::setPhase("gpu");
        auto* cmd = device->beginFrame(window.width(), window.height());
        if (cmd) {
            cmd->clear(SKY_R, SKY_G, SKY_B, 1.0f, true);

            if (mc.isInGame()) {
                mc::debug::setPhase("renderLevel");
                mc::debug::FrameProfiler::begin("renderLevel");
                levelRenderer.renderLevel(cmd, partialTick);
                mc::debug::FrameProfiler::end("renderLevel");

                if (mc.gui() && mc.guiGraphics()) {
                    mc::debug::setPhase("gui");
                    mc::debug::FrameProfiler::begin("gui");
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
                    mc::debug::FrameProfiler::end("gui");
                }
            } else if (mc.screen()) {
                mc::debug::setPhase("panorama");
                mc::debug::FrameProfiler::begin("panorama");
                mc.renderPanorama(cmd, window.width(), window.height(), (float)(dtMs / 1000.0));
                mc.screen()->render(*mc.guiGraphics(), (int)mc.guiMouseX(), (int)mc.guiMouseY(), partialTick);
                mc.guiGraphics()->render(cmd, (float)mc.guiScaledWidth(), (float)mc.guiScaledHeight());
                mc::debug::FrameProfiler::end("panorama");
            }

            mc::debug::setPhase("endFrame");
            mc::debug::FrameProfiler::begin("gpu.endFrame");
            device->endFrame();
            mc::debug::FrameProfiler::end("gpu.endFrame");
        }

        mc::debug::FrameProfiler::end("frame.total");
        mc::debug::FrameProfiler::instance().endFrame();
    }

    mc::debug::setPhase("shutdown");
    mc::debug::stopWatchdog();
    device->waitIdle();
    mc.disconnect();
    mc::AssetPack::shutdown();
    MC_LOG_INFO("mcpp shutdown complete");
    return 0;
}

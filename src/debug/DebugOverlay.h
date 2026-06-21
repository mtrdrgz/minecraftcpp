#pragma once
// DebugOverlay — in-game debug overlay (F1 to toggle).
// DEVELOPMENT TOOL, NOT part of the 1:1 port.
//
// Tabs (switch with number keys 1/2/3 or F2):
//   1. Info    — FPS, position, chunk count, seed
//   2. Structures — find + teleport to structure spawn chunks (2M block radius)
//   3. Biomes  — list all 65 overworld biomes with teleport (2M block radius)
//
// Searches run on a background thread with live progress feedback.

#include "../core/Math.h"
#include "../client/Minecraft.h"
#include "../client/player/LocalPlayer.h"
#include "../world/level/levelgen/NoiseBasedChunkGenerator.h"
#include "../world/level/levelgen/BiomeManager.h"
#include "../world/level/levelgen/structure/placement/StructurePlacement.h"
#include "../render/gui/GuiGraphics.h"
#include "../render/gui/Font.h"
#include "../render/IRenderDevice.h"
#include "../platform/Window.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace mc {

struct DebugOverlay {
    bool visible = false;
    int currentTab = 0;  // 0=Info, 1=Structures, 2=Biomes
    bool pendingClick = false;

    // FPS tracking
    int frameCount = 0;
    double fpsTimer = 0.0;
    int currentFps = 0;
    std::chrono::steady_clock::time_point lastFrameTime = std::chrono::steady_clock::now();

    // ---- Structure search (async) ----
    struct StructureLoc {
        std::string name;
        int chunkX = 0, chunkZ = 0;
        int blockX = 0, blockZ = 0;
        bool visited = false;
    };
    std::vector<StructureLoc> foundStructures;
    std::string searchStatus;
    std::atomic<bool> searchRunning{false};
    std::atomic<int> searchProgress{0};   // 0..100
    std::atomic<int> searchFound{0};
    std::string searchCurrentSet;         // which set is being scanned
    std::thread searchThread;

    // ---- Biome search (async) ----
    std::string biomeSearchStatus;
    std::atomic<bool> biomeSearchRunning{false};
    std::atomic<int> biomeSearchProgress{0};  // 0..100
    std::atomic<int> biomeSearchRadius{0};    // current search radius in blocks
    std::thread biomeThread;
    std::atomic<bool> biomeFound{false};
    int biomeFoundX = 0, biomeFoundZ = 0;

    // ---- Methods ----
    bool button(render::GuiGraphics& g, render::Font& font,
                int x, int y, int w, int h,
                const std::string& text, int mouseX, int mouseY);

    void render(render::GuiGraphics& g, render::Font& font, Minecraft& mc,
                Window& window, float dtSec);

    void renderInfoTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY);
    void renderStructuresTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY);
    void renderBiomesTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY);

    void startStructureSearch(Minecraft& mc);
    void startBiomeSearch(Minecraft& mc, const std::string& targetBiome);

    void teleport(Minecraft& mc, double x, double y, double z);

    int drawText(render::GuiGraphics& g, render::Font& font, const std::string& text,
                 int x, int y, const glm::vec4& color);

    ~DebugOverlay();
};

} // namespace mc

#pragma once
// DebugOverlay — in-game debug overlay (F1 to toggle).
// This is a DEVELOPMENT TOOL, NOT part of the 1:1 port. It should be stripped
// for release builds.
//
// Tabs (switch with number keys 1/2/3 or F2):
//   1. Info    — FPS, position, chunk count, seed, loaded chunk stats
//   2. Structures — find + teleport to nearby structure spawn chunks
//   3. Biomes  — list all 65 overworld biomes with teleport buttons

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

#include <chrono>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace mc {

struct DebugOverlay {
    bool visible = false;
    int currentTab = 0;  // 0=Info, 1=Structures, 2=Biomes
    bool pendingClick = false;  // set by main loop when overlay should consume a click

    // FPS tracking
    int frameCount = 0;
    double fpsTimer = 0.0;
    int currentFps = 0;
    std::chrono::steady_clock::time_point lastFrameTime = std::chrono::steady_clock::now();

    // Structure search
    struct StructureLoc {
        std::string name;
        int chunkX = 0;
        int chunkZ = 0;
        int blockX = 0;
        int blockZ = 0;
        bool visited = false;
    };
    std::vector<StructureLoc> foundStructures;
    bool structuresSearched = false;
    std::string searchStatus;

    // Biome teleport
    std::string biomeSearchStatus;

    // Simple button helper: returns true if clicked
    bool button(render::GuiGraphics& g, render::Font& font,
                int x, int y, int w, int h,
                const std::string& text, int mouseX, int mouseY);

    // Main render
    void render(render::GuiGraphics& g, render::Font& font, Minecraft& mc,
                Window& window, float dtSec);

    // Tab content
    void renderInfoTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY);
    void renderStructuresTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY);
    void renderBiomesTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY);

    // Search for structure chunks within a radius of the player
    void searchStructures(Minecraft& mc);

    // Teleport player to a position
    void teleport(Minecraft& mc, double x, double y, double z);

    // Scan for a specific biome within a radius
    bool findBiome(Minecraft& mc, const std::string& targetBiome, int& outX, int& outZ);

    // Draw a text line, returns the next Y
    int drawText(render::GuiGraphics& g, render::Font& font, const std::string& text,
                 int x, int y, const glm::vec4& color);
};

} // namespace mc

#pragma once
// DebugOverlay — in-game debug overlay (F1 to toggle).
// This is a DEVELOPMENT TOOL, NOT part of the 1:1 port. It is guarded by
// MCPP_DEBUG_OVERLAY and should be stripped for release builds.
//
// Tabs:
//   1. Info    — FPS, position, chunk count, seed, loaded chunk stats
//   2. Structures — find + teleport to nearby structure spawn chunks
//                   (villages, pillager_outposts, swamp_huts, etc.)
//                   Tracks visited locations so re-clicking goes to a new one.
//   3. Biomes  — list all 65 overworld biomes with teleport buttons
//                (scans outward from current position for the target biome)

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

    // FPS tracking
    int frameCount = 0;
    double fpsTimer = 0.0;
    int currentFps = 0;

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
    struct ButtonResult {
        bool clicked = false;
        bool hovered = false;
    };

    ButtonResult button(render::GuiGraphics& g, render::Font& font,
                        int x, int y, int w, int h,
                        const std::string& text, int mouseX, int mouseY,
                        bool clicked);

    // Tab bar
    void drawTabBar(render::GuiGraphics& g, render::Font& font,
                    int mouseX, int mouseY, bool clicked, int screenW);

    // Main render
    void render(render::GuiGraphics& g, render::Font& font, Minecraft& mc,
                Window& window, float dtSec);

    // Tab content
    void renderInfoTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY, bool clicked);
    void renderStructuresTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY, bool clicked);
    void renderBiomesTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY, bool clicked);

    // Search for structure chunks within a radius of the player
    void searchStructures(Minecraft& mc);

    // Teleport player to a position
    void teleport(Minecraft& mc, double x, double y, double z);

    // Scan for a specific biome within a radius
    bool findBiome(Minecraft& mc, const std::string& targetBiome, int& outX, int& outZ);
};

} // namespace mc

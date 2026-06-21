// DebugOverlay — in-game debug overlay implementation.
// DEVELOPMENT TOOL, NOT part of the 1:1 port.

#include "DebugOverlay.h"
#include "../core/Log.h"

namespace mc {

bool DebugOverlay::button(render::GuiGraphics& g, render::Font& font,
                           int x, int y, int w, int h,
                           const std::string& text, int mouseX, int mouseY) {
    bool hovered = (mouseX >= x && mouseX < x + w && mouseY >= y && mouseY < y + h);
    bool clicked = hovered && pendingClick;

    glm::vec4 bg = hovered ? glm::vec4(0.3f, 0.3f, 0.5f, 0.95f)
                            : glm::vec4(0.15f, 0.15f, 0.2f, 0.95f);
    g.fill(x, y, x + w, y + h, bg);
    // Border
    g.fill(x, y, x + w, y + 1, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
    g.fill(x, y + h - 1, x + w, y + h, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
    g.fill(x, y, x + 1, y + h, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
    g.fill(x + w - 1, y, x + w, y + h, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));

    int textW = font.width(text);
    font.drawString(g, text, (float)(x + w / 2 - textW / 2), (float)(y + (h - 8) / 2),
                    {1, 1, 1, 1}, false);
    return clicked;
}

int DebugOverlay::drawText(render::GuiGraphics& g, render::Font& font,
                            const std::string& text, int x, int y,
                            const glm::vec4& color) {
    font.drawString(g, text, (float)x, (float)y, color, false);
    return y + 12;
}

void DebugOverlay::teleport(Minecraft& mc, double x, double y, double z) {
    PlayerState& state = mc.player();
    state.x = x;
    state.z = z;
    if (mc.m_localGenerator) {
        int h = mc.m_localGenerator->getBaseHeight((int)x, (int)z);
        state.y = (double)h + 1.0;
    } else {
        state.y = 100.0;
    }
    MC_LOG_INFO("[DEBUG] Teleported to ({:.1f}, {:.1f}, {:.1f})", x, state.y, z);
}

void DebugOverlay::searchStructures(Minecraft& mc) {
    foundStructures.clear();
    structuresSearched = true;
    searchStatus = "Searching...";

    if (!mc.m_localGenerator) {
        searchStatus = "No generator";
        return;
    }

    int px = (int)std::floor(mc.player().x / 16.0);
    int pz = (int)std::floor(mc.player().z / 16.0);
    const int searchRadius = 64;

    static const char* structureSetIds[] = {
        "minecraft:villages", "minecraft:pillager_outposts",
        "minecraft:swamp_huts", "minecraft:desert_pyramids",
        "minecraft:jungle_temples", "minecraft:igloos",
        "minecraft:shipwrecks", "minecraft:shipwreck_beached",
        "minecraft:ocean_ruins_cold", "minecraft:ocean_ruins_warm",
        "minecraft:ruined_portals", "minecraft:ruined_portals_desert",
        "minecraft:ruined_portals_jungle", "minecraft:ruined_portals_mountain",
        "minecraft:ruined_portals_nether", "minecraft:ruined_portals_ocean",
        "minecraft:ruined_portals_swamp", "minecraft:buried_treasures",
        "minecraft:nether_fossils", "minecraft:woodland_mansions",
        "minecraft:ocean_monuments", "minecraft:strongholds",
        "minecraft:ancient_cities", "minecraft:trial_chambers",
        "minecraft:bastion_remnants", "minecraft:end_cities",
        "minecraft:monuments",
    };

    auto& dataDir = mc.m_dataMinecraftDir;
    if (dataDir.empty()) {
        searchStatus = "No worldgen data dir";
        return;
    }

    levelgen::structure::StructureState state =
        levelgen::structure::StructureState::loadFromDirectory(
            dataDir + "/worldgen/structure_set", (int64_t)mc.m_worldSeed);

    int found = 0;
    for (const char* setId : structureSetIds) {
        auto it = state.sets.find(setId);
        if (it == state.sets.end()) continue;
        const auto& placement = it->second;
        if (!levelgen::structure::StructureState::isPlacementSupported(placement)) continue;

        for (int r = 0; r <= searchRadius && found < 50; ++r) {
            for (int dx = -r; dx <= r && found < 50; ++dx) {
                for (int dz = -r; dz <= r && found < 50; ++dz) {
                    if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                    int cx = px + dx;
                    int cz = pz + dz;
                    if (state.isStructureChunk(placement, cx, cz)) {
                        StructureLoc loc;
                        std::string name = setId;
                        if (name.starts_with("minecraft:")) name = name.substr(10);
                        loc.name = name;
                        loc.chunkX = cx;
                        loc.chunkZ = cz;
                        loc.blockX = cx * 16 + 8;
                        loc.blockZ = cz * 16 + 8;
                        foundStructures.push_back(loc);
                        ++found;
                    }
                }
            }
        }
    }

    searchStatus = "Found " + std::to_string(found) + " structures within " +
                   std::to_string(searchRadius * 16) + " blocks";
}

bool DebugOverlay::findBiome(Minecraft& mc, const std::string& targetBiome, int& outX, int& outZ) {
    if (!mc.m_localGenerator) return false;
    int px = (int)mc.player().x;
    int pz = (int)mc.player().z;
    const int maxRadius = 512;
    const int step = 16;

    for (int r = 0; r <= maxRadius; r += step) {
        for (int dx = -r; dx <= r; dx += step) {
            for (int dz = -r; dz <= r; dz += step) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int x = px + dx;
                int z = pz + dz;
                std::string biome = mc.m_localGenerator->getBiome(x, 64, z);
                if (biome == targetBiome) {
                    outX = x;
                    outZ = z;
                    return true;
                }
            }
        }
    }
    return false;
}

void DebugOverlay::renderInfoTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY) {
    int y = 44;
    const glm::vec4 white(1, 1, 1, 1);
    const glm::vec4 yellow(1, 1, 0, 1);
    const glm::vec4 gray(0.6f, 0.6f, 0.6f, 1);

    auto& state = mc.player();
    int chunkX = (int)std::floor(state.x / 16.0);
    int chunkZ = (int)std::floor(state.z / 16.0);

    y = drawText(g, font, "FPS: " + std::to_string(currentFps), 15, y, yellow);
    y = drawText(g, font, "XYZ: " + std::to_string((int)state.x) + " / " +
                std::to_string((int)state.y) + " / " + std::to_string((int)state.z), 15, y, white);
    y = drawText(g, font, "Chunk: " + std::to_string(chunkX) + ", " + std::to_string(chunkZ), 15, y, white);
    y = drawText(g, font, "Seed: " + std::to_string(mc.m_worldSeed), 15, y, white);
    y = drawText(g, font, "Chunks: " + std::to_string(mc.m_chunks.size()), 15, y, white);
    y = drawText(g, font, "Gen tasks: " + std::to_string(mc.m_generationTasks.size()), 15, y, white);

    if (mc.m_localGenerator) {
        std::string biome = mc.m_localGenerator->getBiome((int)state.x, (int)state.y, (int)state.z);
        y = drawText(g, font, "Biome: " + biome, 15, y, yellow);
    }

    y += 8;
    y = drawText(g, font, "F1=Close  1/2/3=Tabs  F2=Cycle", 15, y, gray);
}

void DebugOverlay::renderStructuresTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY) {
    int y = 44;

    if (button(g, font, 15, y, 180, 18, "Search Structures", mouseX, mouseY)) {
        searchStructures(mc);
    }
    y += 24;

    if (!searchStatus.empty()) {
        y = drawText(g, font, searchStatus, 15, y, glm::vec4(1, 1, 0, 1));
        y += 4;
    }

    if (foundStructures.empty() && structuresSearched) {
        drawText(g, font, "No structures found. Try searching again.", 15, y, glm::vec4(0.8f, 0.6f, 0.6f, 1));
        return;
    }

    // List with scroll-like layout
    int btnW = 300;
    int btnH = 16;
    for (auto& loc : foundStructures) {
        std::string label = loc.name + " @ " + std::to_string(loc.blockX) + ", " + std::to_string(loc.blockZ);
        if (loc.visited) label = "[V] " + label;
        if (button(g, font, 15, y, btnW, btnH, label, mouseX, mouseY)) {
            teleport(mc, (double)loc.blockX, 0, (double)loc.blockZ);
            loc.visited = true;
        }
        y += btnH + 2;
    }
}

void DebugOverlay::renderBiomesTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY) {
    int y = 44;

    static const char* biomes[] = {
        "plains", "sunflower_plains", "snowy_plains", "ice_spikes",
        "desert", "swamp", "mangrove_swamp", "forest",
        "flower_forest", "birch_forest", "dark_forest", "old_growth_birch_forest",
        "old_growth_pine_taiga", "old_growth_spruce_taiga", "taiga", "snowy_taiga",
        "savanna", "savanna_plateau", "windswept_savanna", "windswept_hills",
        "windswept_gravelly_hills", "windswept_forest", "jungle", "sparse_jungle",
        "bamboo_jungle", "badlands", "eroded_badlands", "wooded_badlands",
        "meadow", "cherry_grove", "grove", "snowy_slopes",
        "jagged_peaks", "frozen_peaks", "stony_peaks", "mushroom_fields",
        "beach", "snowy_beach", "stony_shore", "ocean",
        "warm_ocean", "lukewarm_ocean", "cold_ocean", "frozen_ocean",
        "deep_ocean", "deep_warm_ocean", "deep_lukewarm_ocean", "deep_cold_ocean",
        "deep_frozen_ocean", "river", "frozen_river", "lush_caves",
        "dripstone_caves", "deep_dark", "pale_garden", "nether_wastes",
        "crimson_forest", "warped_forest", "soul_sand_valley", "basalt_deltas",
        "the_end", "end_highlands", "end_midlands", "end_barrens",
        "small_end_islands", "the_void"
    };
    constexpr int biomeCount = sizeof(biomes) / sizeof(biomes[0]);

    if (!biomeSearchStatus.empty()) {
        y = drawText(g, font, biomeSearchStatus, 15, y, glm::vec4(1, 1, 0, 1));
        y += 4;
    }

    // 3-column layout
    int colW = 160;
    int btnH = 15;
    int gap = 2;
    int startY = y;
    // Calculate how many fit per column based on available height
    int availH = 680 - startY;
    int perCol = availH / (btnH + gap);
    if (perCol < 1) perCol = 1;

    for (int i = 0; i < biomeCount; ++i) {
        int col = i / perCol;
        int row = i % perCol;
        int bx = 15 + col * (colW + 6);
        int by = startY + row * (btnH + gap);

        std::string name = biomes[i];
        if (button(g, font, bx, by, colW, btnH, name, mouseX, mouseY)) {
            std::string fullBiome = "minecraft:" + name;
            biomeSearchStatus = "Searching for " + name + "...";
            int foundX, foundZ;
            if (findBiome(mc, fullBiome, foundX, foundZ)) {
                teleport(mc, (double)foundX, 0, (double)foundZ);
                biomeSearchStatus = "Found " + name + " at " + std::to_string(foundX) + ", " + std::to_string(foundZ);
            } else {
                biomeSearchStatus = "Could not find " + name + " within 512 blocks";
            }
        }
    }
}

void DebugOverlay::render(render::GuiGraphics& g, render::Font& font, Minecraft& mc,
                           Window& window, float dtSec) {
    if (!visible) return;

    // FPS tracking using real frame time
    auto now = std::chrono::steady_clock::now();
    float realDt = std::chrono::duration<float>(now - lastFrameTime).count();
    lastFrameTime = now;

    fpsTimer += realDt;
    ++frameCount;
    if (fpsTimer >= 0.5) {
        currentFps = (int)(frameCount / fpsTimer);
        frameCount = 0;
        fpsTimer = 0.0;
    }

    // Use GUI-scaled coordinates (the GuiGraphics render uses guiScaledWidth/Height)
    int screenW = mc.guiScaledWidth();
    int screenH = mc.guiScaledHeight();
    int mouseX = (int)mc.guiMouseX();
    int mouseY = (int)mc.guiMouseY();

    // Panel: fit within the screen with margins
    int panelW = std::min(screenW - 10, 640);
    int panelH = std::min(screenH - 10, screenH - 10);
    int panelX = 5;
    int panelY = 5;

    // Background
    g.fill(panelX, panelY, panelX + panelW, panelY + panelH, glm::vec4(0.05f, 0.05f, 0.08f, 0.88f));

    // Title bar
    g.fill(panelX, panelY, panelX + panelW, panelY + 20, glm::vec4(0.1f, 0.1f, 0.15f, 0.95f));
    font.drawString(g, "[DEBUG] F1=Close | 1/2/3=Tabs", (float)(panelX + 8), (float)(panelY + 6),
                    glm::vec4(1, 0.8f, 0, 1), false);

    // Tab bar
    const char* tabNames[] = {"1:Info", "2:Structures", "3:Biomes"};
    int tabW = 80;
    int tabH = 18;
    for (int i = 0; i < 3; ++i) {
        int tx = panelX + 8 + i * (tabW + 4);
        int ty = panelY + 22;
        bool active = (currentTab == i);
        bool hover = (mouseX >= tx && mouseX < tx + tabW && mouseY >= ty && mouseY < ty + tabH);

        glm::vec4 bg = active ? glm::vec4(0.2f, 0.4f, 0.8f, 0.95f)
                              : (hover ? glm::vec4(0.25f, 0.25f, 0.35f, 0.95f)
                                        : glm::vec4(0.1f, 0.1f, 0.15f, 0.95f));
        g.fill(tx, ty, tx + tabW, ty + tabH, bg);
        g.fill(tx, ty, tx + tabW, ty + 1, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
        g.fill(tx, ty + tabH - 1, tx + tabW, ty + tabH, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
        g.fill(tx, ty, tx + 1, ty + tabH, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
        g.fill(tx + tabW - 1, ty, tx + tabW, ty + tabH, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
        int tw = font.width(tabNames[i]);
        font.drawString(g, tabNames[i], (float)(tx + tabW / 2 - tw / 2), (float)(ty + 5),
                        {1, 1, 1, 1}, false);

        if (hover && pendingClick) {
            currentTab = i;
        }
    }

    // Tab content — translate coordinates to panel-relative
    // The tab content functions use absolute coords starting at y=44 (22 for
    // tab bar + 22 for title). We offset by panelY.
    g.push();
    g.translate((float)panelX, (float)panelY, 0.0f);

    switch (currentTab) {
        case 0: renderInfoTab(g, font, mc, mouseX - panelX, mouseY - panelY); break;
        case 1: renderStructuresTab(g, font, mc, mouseX - panelX, mouseY - panelY); break;
        case 2: renderBiomesTab(g, font, mc, mouseX - panelX, mouseY - panelY); break;
    }

    g.pop();

    // Clear the pending click AFTER all tabs have had a chance to consume it
    pendingClick = false;
}

} // namespace mc

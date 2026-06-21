// DebugOverlay — in-game debug overlay implementation.
// DEVELOPMENT TOOL, NOT part of the 1:1 port. Guard with MCPP_DEBUG_OVERLAY.

#include "DebugOverlay.h"
#include "../core/Log.h"

namespace mc {

DebugOverlay::ButtonResult DebugOverlay::button(
        render::GuiGraphics& g, render::Font& font,
        int x, int y, int w, int h,
        const std::string& text, int mouseX, int mouseY, bool clicked) {
    ButtonResult r;
    r.hovered = (mouseX >= x && mouseX < x + w && mouseY >= y && mouseY < y + h);
    r.clicked = r.hovered && clicked;

    // Background
    glm::vec4 bg = r.hovered ? glm::vec4(0.3f, 0.3f, 0.5f, 0.9f)
                              : glm::vec4(0.15f, 0.15f, 0.2f, 0.9f);
    g.fill(x, y, x + w, y + h, bg);
    // Border
    g.fill(x, y, x + w, y + 1, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
    g.fill(x, y + h - 1, x + w, y + h, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
    g.fill(x, y, x + 1, y + h, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
    g.fill(x + w - 1, y, x + w, y + h, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));

    // Text
    int textW = font.width(text);
    font.drawString(g, text, (float)(x + w / 2 - textW / 2), (float)(y + (h - 8) / 2),
                    {1, 1, 1, 1}, false);
    return r;
}

void DebugOverlay::drawTabBar(render::GuiGraphics& g, render::Font& font,
                               int mouseX, int mouseY, bool clicked, int screenW) {
    const char* tabNames[] = {"Info (1)", "Structures (2)", "Biomes (3)"};
    int tabW = 120;
    int tabH = 20;
    for (int i = 0; i < 3; ++i) {
        int tx = 10 + i * (tabW + 4);
        bool active = (currentTab == i);
        glm::vec4 bg = active ? glm::vec4(0.2f, 0.4f, 0.8f, 0.95f)
                              : (mouseX >= tx && mouseX < tx + tabW && mouseY >= 10 && mouseY < 10 + tabH
                                 ? glm::vec4(0.25f, 0.25f, 0.35f, 0.95f)
                                 : glm::vec4(0.1f, 0.1f, 0.15f, 0.95f));
        g.fill(tx, 10, tx + tabW, 10 + tabH, bg);
        g.fill(tx, 10, tx + tabW, 11, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
        g.fill(tx, 10 + tabH - 1, tx + tabW, 10 + tabH, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
        g.fill(tx, 10, tx + 1, 10 + tabH, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
        g.fill(tx + tabW - 1, 10, tx + tabW, 10 + tabH, glm::vec4(0.5f, 0.5f, 0.6f, 1.0f));
        int tw = font.width(tabNames[i]);
        font.drawString(g, tabNames[i], (float)(tx + tabW / 2 - tw / 2), 12, {1, 1, 1, 1}, false);

        if (clicked && mouseX >= tx && mouseX < tx + tabW && mouseY >= 10 && mouseY < 10 + tabH) {
            currentTab = i;
        }
    }
}

void DebugOverlay::teleport(Minecraft& mc, double x, double y, double z) {
    PlayerState& state = mc.player();
    state.x = x;
    state.z = z;
    // Find ground height at target
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

    // Access the structure placement state from StructureGen
    // We need to scan chunks around the player for structure spawn points
    int px = (int)std::floor(mc.player().x / 16.0);
    int pz = (int)std::floor(mc.player().z / 16.0);

    // List of structure sets to search for (from structure_set JSONs)
    // We'll scan a 64-chunk radius (1024 blocks) around the player
    const int searchRadius = 64;

    // Structure set IDs we care about
    static const char* structureSetIds[] = {
        "minecraft:villages",
        "minecraft:pillager_outposts",
        "minecraft:swamp_huts",
        "minecraft:desert_pyramids",
        "minecraft:jungle_temples",
        "minecraft:igloos",
        "minecraft:shipwrecks",
        "minecraft:shipwreck_beached",
        "minecraft:ocean_ruins_cold",
        "minecraft:ocean_ruins_warm",
        "minecraft:ruined_portals",
        "minecraft:ruined_portals_desert",
        "minecraft:ruined_portals_jungle",
        "minecraft:ruined_portals_mountain",
        "minecraft:ruined_portals_nether",
        "minecraft:ruined_portals_ocean",
        "minecraft:ruined_portals_swamp",
        "minecraft:buried_treasures",
        "minecraft:nether_fossils",
        "minecraft:woodland_mansions",
        "minecraft:ocean_monuments",
        "minecraft:strongholds",
        "minecraft:ancient_cities",
        "minecraft:trial_chambers",
        "minecraft:bastion_remnants",
        "minecraft:end_cities",
        "minecraft:monuments",
    };

    // Load structure placement state
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

        // Scan chunks in a spiral from the player outward
        for (int r = 0; r <= searchRadius && found < 50; ++r) {
            for (int dx = -r; dx <= r && found < 50; ++dx) {
                for (int dz = -r; dz <= r && found < 50; ++dz) {
                    if (std::max(std::abs(dx), std::abs(dz)) != r) continue;  // ring only
                    int cx = px + dx;
                    int cz = pz + dz;
                    if (state.isStructureChunk(placement, cx, cz)) {
                        StructureLoc loc;
                        // Strip "minecraft:" prefix for display
                        std::string name = setId;
                        if (name.starts_with("minecraft:")) name = name.substr(10);
                        loc.name = name;
                        loc.chunkX = cx;
                        loc.chunkZ = cz;
                        loc.blockX = cx * 16 + 8;
                        loc.blockZ = cz * 16 + 8;
                        // Check if visited
                        std::string key = name + "@" + std::to_string(cx) + "," + std::to_string(cz);
                        // Check against visited set
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

    // Spiral search outward up to 512 blocks
    const int maxRadius = 512;
    const int step = 16;  // check every 16 blocks

    for (int r = 0; r <= maxRadius; r += step) {
        for (int dx = -r; dx <= r; dx += step) {
            for (int dz = -r; dz <= r; dz += step) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;  // ring only
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

void DebugOverlay::renderInfoTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY, bool clicked) {
    int y = 40;
    const glm::vec4 white(1, 1, 1, 1);
    const glm::vec4 yellow(1, 1, 0, 1);

    auto& state = mc.player();
    int chunkX = (int)std::floor(state.x / 16.0);
    int chunkZ = (int)std::floor(state.z / 16.0);

    font.drawString(g, "FPS: " + std::to_string(currentFps), 15, y, yellow, false); y += 12;
    font.drawString(g, "Position: " + std::to_string((int)state.x) + ", " +
                    std::to_string((int)state.y) + ", " + std::to_string((int)state.z), 15, y, white, false); y += 12;
    font.drawString(g, "Chunk: " + std::to_string(chunkX) + ", " + std::to_string(chunkZ), 15, y, white, false); y += 12;
    font.drawString(g, "Seed: " + std::to_string(mc.m_worldSeed), 15, y, white, false); y += 12;
    font.drawString(g, "Loaded chunks: " + std::to_string(mc.m_chunks.size()), 15, y, white, false); y += 12;
    font.drawString(g, "Generation tasks: " + std::to_string(mc.m_generationTasks.size()), 15, y, white, false); y += 12;

    // Current biome
    if (mc.m_localGenerator) {
        std::string biome = mc.m_localGenerator->getBiome((int)state.x, (int)state.y, (int)state.z);
        font.drawString(g, "Biome: " + biome, 15, y, yellow, false); y += 12;
    }

    y += 8;
    font.drawString(g, "F1: Toggle debug | ESC: Pause", 15, y, glm::vec4(0.6f, 0.6f, 0.6f, 1), false);
}

void DebugOverlay::renderStructuresTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY, bool clicked) {
    int y = 40;

    // Search button
    auto br = button(g, font, 15, y, 200, 18, "Search Structures", mouseX, mouseY, clicked);
    y += 24;

    if (br.clicked) {
        searchStructures(mc);
    }

    if (!searchStatus.empty()) {
        font.drawString(g, searchStatus, 15, y, glm::vec4(1, 1, 0, 1), false);
        y += 16;
    }

    if (foundStructures.empty() && structuresSearched) {
        font.drawString(g, "No structures found. Try searching again.", 15, y, glm::vec4(0.8f, 0.6f, 0.6f, 1), false);
        return;
    }

    // List found structures with TP buttons
    int scrollY = y;
    int listEnd = 700;  // bottom of screen limit
    int btnW = 280;
    int btnH = 18;

    for (auto& loc : foundStructures) {
        if (scrollY >= listEnd) break;

        std::string label = loc.name + " at " + std::to_string(loc.blockX) + ", " + std::to_string(loc.blockZ);
        if (loc.visited) label = "[VISITED] " + label;

        auto bres = button(g, font, 15, scrollY, btnW, btnH, label, mouseX, mouseY, clicked);
        if (bres.clicked) {
            teleport(mc, (double)loc.blockX, 0, (double)loc.blockZ);
            loc.visited = true;
        }
        scrollY += btnH + 2;
    }
}

void DebugOverlay::renderBiomesTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY, bool clicked) {
    int y = 40;

    // All 65 overworld biomes
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
        font.drawString(g, biomeSearchStatus, 15, y, glm::vec4(1, 1, 0, 1), false);
        y += 16;
    }

    // Two-column layout
    int colW = 170;
    int btnH = 16;
    int startY = y;
    int listEnd = 700;
    int perCol = (listEnd - startY) / (btnH + 2);

    for (int i = 0; i < biomeCount; ++i) {
        int col = i / perCol;
        int row = i % perCol;
        int bx = 15 + col * (colW + 8);
        int by = startY + row * (btnH + 2);
        if (by >= listEnd) break;

        std::string name = biomes[i];
        std::string displayName = name;
        // Shorten for display
        if (displayName.length() > 22) displayName = displayName.substr(0, 20) + "..";

        auto bres = button(g, font, bx, by, colW, btnH, displayName, mouseX, mouseY, clicked);
        if (bres.clicked) {
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

    // FPS counter
    fpsTimer += dtSec;
    ++frameCount;
    if (fpsTimer >= 1.0) {
        currentFps = frameCount;
        frameCount = 0;
        fpsTimer = 0.0;
    }

    int mouseX = (int)mc.guiMouseX();
    int mouseY = (int)mc.guiMouseY();
    bool clicked = window.consumeLButtonClicked();

    // Semi-transparent background panel
    int panelW = std::min(window.width(), 640);
    int panelH = std::min(window.height(), 720);
    g.fill(5, 5, panelW, panelH, glm::vec4(0.05f, 0.05f, 0.08f, 0.85f));

    // Title
    font.drawString(g, "[DEBUG OVERLAY] F1 to close", 15, 22, glm::vec4(1, 0.8f, 0, 1), false);

    // Tab bar
    drawTabBar(g, font, mouseX, mouseY, clicked, panelW);

    // Tab content
    switch (currentTab) {
        case 0: renderInfoTab(g, font, mc, mouseX, mouseY, clicked); break;
        case 1: renderStructuresTab(g, font, mc, mouseX, mouseY, clicked); break;
        case 2: renderBiomesTab(g, font, mc, mouseX, mouseY, clicked); break;
    }
}

} // namespace mc

// DebugOverlay — in-game debug overlay implementation.
// DEVELOPMENT TOOL, NOT part of the 1:1 port.

#include "DebugOverlay.h"
#include "../core/Log.h"

namespace mc {

DebugOverlay::~DebugOverlay() {
    searchRunning = false;
    biomeSearchRunning = false;
    if (searchThread.joinable()) searchThread.join();
    if (biomeThread.joinable()) biomeThread.join();
}

bool DebugOverlay::button(render::GuiGraphics& g, render::Font& font,
                           int x, int y, int w, int h,
                           const std::string& text, int mouseX, int mouseY) {
    bool hovered = (mouseX >= x && mouseX < x + w && mouseY >= y && mouseY < y + h);
    bool clicked = hovered && pendingClick;

    glm::vec4 bg = hovered ? glm::vec4(0.3f, 0.3f, 0.5f, 0.95f)
                            : glm::vec4(0.15f, 0.15f, 0.2f, 0.95f);
    g.fill(x, y, x + w, y + h, bg);
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
    // Clear loaded chunks to force reload at new position
    mc.m_chunks.clear();
    mc.m_generationTasks.clear();
    mc.m_queuedChunks.clear();
    MC_LOG_INFO("[DEBUG] Teleported to ({:.1f}, {:.1f}, {:.1f})", x, state.y, z);
}

void DebugOverlay::startStructureSearch(Minecraft& mc) {
    if (searchRunning) return;
    if (searchThread.joinable()) searchThread.join();

    foundStructures.clear();
    searchRunning = true;
    searchProgress = 0;
    searchFound = 0;
    searchStatus = "Starting search...";

    // Capture all needed data by value (the thread can't access mc directly)
    uint64_t seed = mc.m_worldSeed;
    std::string dataDir = mc.m_dataMinecraftDir;
    int px = (int)std::floor(mc.player().x / 16.0);
    int pz = (int)std::floor(mc.player().z / 16.0);

    // 2M blocks = 125000 chunks radius
    const int searchRadius = 125000;

    static const char* structureSetIds[] = {
        "minecraft:villages", "minecraft:pillager_outposts",
        "minecraft:swamp_huts", "minecraft:desert_pyramids",
        "minecraft:jungle_temples", "minecraft:igloos",
        "minecraft:shipwrecks", "minecraft:shipwreck_beached",
        "minecraft:ocean_ruins_cold", "minecraft:ocean_ruins_warm",
        "minecraft:ruined_portals", "minecraft:buried_treasures",
        "minecraft:nether_fossils", "minecraft:woodland_mansions",
        "minecraft:ocean_monuments", "minecraft:strongholds",
        "minecraft:ancient_cities", "minecraft:trial_chambers",
        "minecraft:bastion_remnants", "minecraft:end_cities",
    };
    constexpr int numSets = sizeof(structureSetIds) / sizeof(structureSetIds[0]);

    searchThread = std::thread([this, seed, dataDir, px, pz, searchRadius]() {
        levelgen::structure::StructureState state =
            levelgen::structure::StructureState::loadFromDirectory(
                dataDir + "/worldgen/structure_set", (int64_t)seed);

        std::vector<StructureLoc> localFound;
        int setIdx = 0;

        for (const char* setId : structureSetIds) {
            if (!searchRunning) break;
            auto it = state.sets.find(setId);
            if (it == state.sets.end()) { ++setIdx; continue; }
            const auto& placement = it->second;
            if (!levelgen::structure::StructureState::isPlacementSupported(placement)) {
                ++setIdx; continue;
            }

            std::string name = setId;
            if (name.starts_with("minecraft:")) name = name.substr(10);
            searchCurrentSet = name;

            // For each ring, scan the perimeter
            for (int r = 0; r <= searchRadius && searchRunning; ++r) {
                // Update progress: each set gets a slice of the progress bar
                int setProgress = (int)((float)r / searchRadius * (100.0f / numSets));
                searchProgress = setIdx * (100 / numSets) + setProgress;

                // Only scan the ring (not the full square) for efficiency
                // For large radii, step by 1 (structure placement is per-chunk)
                for (int dx = -r; dx <= r && searchRunning; ++dx) {
                    // Top and bottom edges of the ring
                    for (int dz : { -r, r }) {
                        if (r == 0 && dz != 0) continue;
                        int cx = px + dx;
                        int cz = pz + dz;
                        if (state.isStructureChunk(placement, cx, cz)) {
                            StructureLoc loc;
                            loc.name = name;
                            loc.chunkX = cx;
                            loc.chunkZ = cz;
                            loc.blockX = cx * 16 + 8;
                            loc.blockZ = cz * 16 + 8;
                            localFound.push_back(loc);
                            searchFound = (int)localFound.size();
                        }
                    }
                    // Left and right edges (only for non-corner cells to avoid double-checking corners)
                    if (r > 0) {
                        for (int dz = -r + 1; dz <= r - 1 && searchRunning; ++dz) {
                            for (int dx2 : { -r, r }) {
                                int cx = px + dx2;
                                int cz = pz + dz;
                                if (state.isStructureChunk(placement, cx, cz)) {
                                    StructureLoc loc;
                                    loc.name = name;
                                    loc.chunkX = cx;
                                    loc.chunkZ = cz;
                                    loc.blockX = cx * 16 + 8;
                                    loc.blockZ = cz * 16 + 8;
                                    localFound.push_back(loc);
                                    searchFound = (int)localFound.size();
                                }
                            }
                        }
                    }
                }
            }
            ++setIdx;
        }

        // Sort by distance from player
        std::sort(localFound.begin(), localFound.end(), [px, pz](const StructureLoc& a, const StructureLoc& b) {
            int da = (a.chunkX - px) * (a.chunkX - px) + (a.chunkZ - pz) * (a.chunkZ - pz);
            int db = (b.chunkX - px) * (b.chunkX - px) + (b.chunkZ - pz) * (b.chunkZ - pz);
            return da < db;
        });

        // Copy results to main thread visible vector
        foundStructures = std::move(localFound);
        searchProgress = 100;
        searchRunning = false;
        searchStatus = "Found " + std::to_string(foundStructures.size()) +
                       " structures within 2M blocks";
        searchCurrentSet.clear();
    });
}

void DebugOverlay::startBiomeSearch(Minecraft& mc, const std::string& targetBiome) {
    if (biomeSearchRunning) return;
    if (biomeThread.joinable()) biomeThread.join();

    biomeSearchRunning = true;
    biomeFound = false;
    biomeSearchProgress = 0;
    biomeSearchRadius = 0;
    biomeSearchStatus = "Searching for " + targetBiome + "...";

    // We can't call getBiome from a thread (it accesses the generator which
    // may not be thread-safe). Instead, create a standalone generator in the
    // thread.
    uint64_t seed = mc.m_worldSeed;
    int px = (int)mc.player().x;
    int pz = (int)mc.player().z;
    const int maxRadius = 2000000;  // 2M blocks
    const int step = 64;            // check every 64 blocks for speed

    biomeThread = std::thread([this, seed, px, pz, targetBiome, maxRadius, step]() {
        // Create a thread-local generator for biome lookups
        levelgen::NoiseBasedChunkGenerator gen(seed);

        for (int r = 0; r <= maxRadius && biomeSearchRunning; r += step) {
            biomeSearchRadius = r;
            biomeSearchProgress = (int)((float)r / maxRadius * 100.0f);

            // Scan the ring at radius r
            for (int dx = -r; dx <= r && biomeSearchRunning; dx += step) {
                for (int dz : { -r, r }) {
                    if (r == 0 && dz != 0) continue;
                    int x = px + dx;
                    int z = pz + dz;
                    std::string biome = gen.getBiome(x, 64, z);
                    if (biome == targetBiome) {
                        biomeFoundX = x;
                        biomeFoundZ = z;
                        biomeFound = true;
                        biomeSearchRunning = false;
                        return;
                    }
                }
            }
            if (r > 0) {
                for (int dz = -r + step; dz <= r - step && biomeSearchRunning; dz += step) {
                    for (int dx : { -r, r }) {
                        int x = px + dx;
                        int z = pz + dz;
                        std::string biome = gen.getBiome(x, 64, z);
                        if (biome == targetBiome) {
                            biomeFoundX = x;
                            biomeFoundZ = z;
                            biomeFound = true;
                            biomeSearchRunning = false;
                            return;
                        }
                    }
                }
            }
        }

        biomeSearchRunning = false;
    });
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

    // Search button (disabled while running)
    if (searchRunning) {
        button(g, font, 15, y, 180, 18, "Searching... (" + std::to_string(searchProgress.load()) + "%)", mouseX, mouseY);
    } else {
        if (button(g, font, 15, y, 180, 18, "Search Structures (2M radius)", mouseX, mouseY)) {
            startStructureSearch(mc);
        }
    }
    y += 24;

    // Live status
    if (searchRunning) {
        y = drawText(g, font, "Scanning: " + searchCurrentSet + " (" +
                     std::to_string(searchProgress.load()) + "%, found " +
                     std::to_string(searchFound.load()) + ")", 15, y, glm::vec4(1, 1, 0, 1));
        y += 4;

        // Progress bar
        int barW = 300;
        int barH = 8;
        g.fill(15, y, 15 + barW, y + barH, glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
        int fillW = (int)(barW * searchProgress.load() / 100.0f);
        g.fill(15, y, 15 + fillW, y + barH, glm::vec4(0.2f, 0.6f, 1.0f, 1.0f));
        y += barH + 4;
    } else if (!searchStatus.empty()) {
        y = drawText(g, font, searchStatus, 15, y, glm::vec4(1, 1, 0, 1));
        y += 4;
    }

    if (foundStructures.empty() && !searchRunning) {
        drawText(g, font, "Click 'Search' to find structures.", 15, y, glm::vec4(0.6f, 0.6f, 0.6f, 1));
        return;
    }

    // List structures sorted by distance, with TP buttons
    int btnW = 300;
    int btnH = 16;
    int listCount = 0;
    int maxList = 40;  // limit visible entries

    for (auto& loc : foundStructures) {
        if (listCount >= maxList) {
            y = drawText(g, font, "... and " + std::to_string(foundStructures.size() - maxList) +
                         " more (narrow search by moving closer)", 15, y, gray);
            break;
        }

        // Format: name @ blockX,blockZ (dist blocks)
        int distBlocks = (int)std::sqrt(
            (double)(loc.chunkX * 16 - (int)mc.player().x) * (loc.chunkX * 16 - (int)mc.player().x) +
            (double)(loc.chunkZ * 16 - (int)mc.player().z) * (loc.chunkZ * 16 - (int)mc.player().z));

        std::string label = loc.name + " @ " + std::to_string(loc.blockX) + "," +
                            std::to_string(loc.blockZ) + " (" + std::to_string(distBlocks) + "m)";
        if (loc.visited) label = "[V] " + label;

        if (button(g, font, 15, y, btnW, btnH, label, mouseX, mouseY)) {
            teleport(mc, (double)loc.blockX, 0, (double)loc.blockZ);
            loc.visited = true;
        }
        y += btnH + 2;
        ++listCount;
    }
}

void DebugOverlay::renderBiomesTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY) {
    int y = 44;

    // If biome search just finished, teleport
    if (!biomeSearchRunning && biomeFound) {
        biomeFound = false;
        teleport(mc, (double)biomeFoundX, 0, (double)biomeFoundZ);
        biomeSearchStatus = "Found! Teleported to " + std::to_string(biomeFoundX) + ", " + std::to_string(biomeFoundZ);
    }

    // Live status
    if (biomeSearchRunning) {
        y = drawText(g, font, "Searching... " + std::to_string(biomeSearchProgress.load()) +
                     "% (radius " + std::to_string(biomeSearchRadius.load()) + " blocks)", 15, y,
                     glm::vec4(1, 1, 0, 1));
        // Progress bar
        int barW = 300;
        int barH = 8;
        g.fill(15, y, 15 + barW, y + barH, glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
        int fillW = (int)(barW * biomeSearchProgress.load() / 100.0f);
        g.fill(15, y, 15 + fillW, y + barH, glm::vec4(0.2f, 0.6f, 1.0f, 1.0f));
        y += barH + 8;
    } else if (!biomeSearchStatus.empty()) {
        y = drawText(g, font, biomeSearchStatus, 15, y, glm::vec4(1, 1, 0, 1));
        y += 4;
    }

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

    // 3-column layout
    int colW = 160;
    int btnH = 15;
    int gap = 2;
    int startY = y;
    int availH = 680 - startY;
    int perCol = availH / (btnH + gap);
    if (perCol < 1) perCol = 1;

    for (int i = 0; i < biomeCount; ++i) {
        int col = i / perCol;
        int row = i % perCol;
        int bx = 15 + col * (colW + 6);
        int by = startY + row * (btnH + gap);

        std::string name = biomes[i];
        bool isSearching = biomeSearchRunning;
        std::string label = name;

        if (isSearching) {
            // Gray out during search
            g.fill(bx, by, bx + colW, by + btnH, glm::vec4(0.1f, 0.1f, 0.12f, 0.5f));
            g.fill(bx, by, bx + colW, by + 1, glm::vec4(0.3f, 0.3f, 0.35f, 0.5f));
            g.fill(bx, by + btnH - 1, bx + colW, by + btnH, glm::vec4(0.3f, 0.3f, 0.35f, 0.5f));
            g.fill(bx, by, bx + 1, by + btnH, glm::vec4(0.3f, 0.3f, 0.35f, 0.5f));
            g.fill(bx + colW - 1, by, bx + colW, by + btnH, glm::vec4(0.3f, 0.3f, 0.35f, 0.5f));
            int tw = font.width(label);
            font.drawString(g, label, (float)(bx + colW / 2 - tw / 2), (float)(by + 4),
                            {0.4f, 0.4f, 0.4f, 1}, false);
        } else {
            if (button(g, font, bx, by, colW, btnH, label, mouseX, mouseY)) {
                std::string fullBiome = "minecraft:" + name;
                biomeSearchStatus = "Searching for " + name + "...";
                startBiomeSearch(mc, fullBiome);
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

    int screenW = mc.guiScaledWidth();
    int screenH = mc.guiScaledHeight();
    int mouseX = (int)mc.guiMouseX();
    int mouseY = (int)mc.guiMouseY();

    // Panel: fit within the screen
    int panelW = std::min(screenW - 10, 640);
    int panelH = std::min(screenH - 10, screenH - 10);
    int panelX = 5;
    int panelY = 5;

    g.fill(panelX, panelY, panelX + panelW, panelY + panelH, glm::vec4(0.05f, 0.05f, 0.08f, 0.88f));
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

    // Tab content
    g.push();
    g.translate((float)panelX, (float)panelY, 0.0f);

    switch (currentTab) {
        case 0: renderInfoTab(g, font, mc, mouseX - panelX, mouseY - panelY); break;
        case 1: renderStructuresTab(g, font, mc, mouseX - panelX, mouseY - panelY); break;
        case 2: renderBiomesTab(g, font, mc, mouseX - panelX, mouseY - panelY); break;
    }

    g.pop();
    pendingClick = false;
}

} // namespace mc

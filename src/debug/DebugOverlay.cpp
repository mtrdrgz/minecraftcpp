// DebugOverlay — in-game debug overlay implementation.
// DEVELOPMENT TOOL, NOT part of the 1:1 port.

#include "DebugOverlay.h"
#include "../core/Log.h"

#include <algorithm>
#include <cmath>
#include <mutex>

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
    (void)y;  // callers pass 0; Y is resolved from the destination surface below.
    // Pick a safe Y at the destination column (surface + 1), falling back to 100.
    double ty = 100.0;
    if (mc.m_localGenerator) {
        ty = (double)mc.m_localGenerator->getBaseHeight((int)x, (int)z) + 1.0;
    }
    // Route through the camera (the real position authority). Do NOT clear m_chunks
    // here: this runs on the render path and would race the decoration worker; the
    // streaming pass unloads far chunks and loads the new area on its own.
    mc.requestTeleport(x, ty, z);
    MC_LOG_INFO("[DEBUG] Teleport requested to ({:.1f}, {:.1f}, {:.1f})", x, ty, z);
}

void DebugOverlay::startStructureSearch(Minecraft& mc) {
    if (searchRunning) return;
    if (searchThread.joinable()) searchThread.join();

    {
        std::lock_guard<std::mutex> lk(foundMutex);
        foundStructures.clear();
    }
    searchRunning = true;
    searchProgress = 0;
    searchFound = 0;
    searchStatus = "Starting search...";

    // Capture all needed data by value (the thread can't access mc directly)
    uint64_t seed = mc.m_worldSeed;
    std::string dataDir = mc.m_dataMinecraftDir;
    int px = (int)std::floor(mc.player().x / 16.0);
    int pz = (int)std::floor(mc.player().z / 16.0);

    // Bounded so the search actually TERMINATES (the old 125000-chunk = 2M-block
    // radius scanned ~10^12 cells and never finished, so results never appeared).
    // We scan outward ring-by-ring and stop each set once we've found the nearest
    // few — that is what a "locate" needs. ~1200 chunks ≈ 19k blocks covers every
    // overworld structure's spacing many times over.
    const int searchRadius = 1200;
    const int maxPerSet = 4;   // nearest N of each structure set

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

    searchThread = std::thread([this, seed, dataDir, px, pz, searchRadius, maxPerSet, numSets]() {
        levelgen::structure::StructureState state =
            levelgen::structure::StructureState::loadFromDirectory(
                dataDir + "/worldgen/structure_set", (int64_t)seed);

        int total = 0;
        int setIdx = 0;

        // Publish one find immediately (kept distance-sorted) so results appear
        // live instead of only at the very end.
        auto publish = [this, px, pz](const StructureLoc& loc) {
            std::lock_guard<std::mutex> lk(foundMutex);
            foundStructures.push_back(loc);
            std::sort(foundStructures.begin(), foundStructures.end(),
                      [px, pz](const StructureLoc& a, const StructureLoc& b) {
                          long da = (long)(a.chunkX - px) * (a.chunkX - px) + (long)(a.chunkZ - pz) * (a.chunkZ - pz);
                          long db = (long)(b.chunkX - px) * (b.chunkX - px) + (long)(b.chunkZ - pz) * (b.chunkZ - pz);
                          return da < db;
                      });
            searchFound = (int)foundStructures.size();
        };

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

            int foundForSet = 0;
            auto check = [&](int cx, int cz) {
                if (state.isStructureChunk(placement, cx, cz)) {
                    StructureLoc loc;
                    loc.name = name;
                    loc.chunkX = cx; loc.chunkZ = cz;
                    loc.blockX = cx * 16 + 8; loc.blockZ = cz * 16 + 8;
                    publish(loc);
                    ++foundForSet; ++total;
                }
            };

            // Scan outward ring-by-ring; stop this set once we've found the nearest
            // maxPerSet (or hit the radius cap). Nearest-first because rings grow.
            for (int r = 0; r <= searchRadius && searchRunning && foundForSet < maxPerSet; ++r) {
                searchProgress = (int)(((float)setIdx + (float)r / searchRadius) / numSets * 100.0f);
                // Top & bottom edges of the ring.
                for (int dx = -r; dx <= r && searchRunning && foundForSet < maxPerSet; ++dx) {
                    for (int dz : { -r, r }) {
                        if (r == 0 && dz != 0) continue;
                        check(px + dx, pz + dz);
                    }
                }
                // Left & right edges (excluding the corners already done above).
                for (int dz = -r + 1; dz <= r - 1 && searchRunning && foundForSet < maxPerSet; ++dz) {
                    check(px - r, pz + dz);
                    check(px + r, pz + dz);
                }
            }
            ++setIdx;
        }

        searchProgress = 100;
        searchRunning = false;
        searchStatus = "Found " + std::to_string(total) +
                       " structures within " + std::to_string(searchRadius * 16) + " blocks";
        searchCurrentSet.clear();
    });
}

void DebugOverlay::startBiomeSearch(Minecraft& mc, const std::string& targetBiome) {
    if (biomeSearchRunning) return;
    if (biomeThread.joinable()) biomeThread.join();

    biomeSearchRunning = true;
    biomeFound = false;
    biomeNotFound = false;
    biomeSearchProgress = 0;
    biomeSearchRadius = 0;
    biomeSearchStatus = "Searching for " + targetBiome + "...";

    // We can't call getBiome from a thread (it accesses the generator which
    // may not be thread-safe). Instead, create a standalone generator in the
    // thread.
    uint64_t seed = mc.m_worldSeed;
    int px = (int)mc.player().x;
    int pz = (int)mc.player().z;
    // Bounded so the search TERMINATES — the biome list includes nether/end biomes
    // that the overworld generator never produces; an unbounded scan would run
    // forever. 200k blocks covers any overworld biome's realistic distance.
    const int maxRadius = 200000;
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

        biomeNotFound = true;       // finished the whole radius without a match
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

void DebugOverlay::renderStructuresTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY, int panelW, int panelH) {
    int y = 44;

    // Search button (disabled while running)
    if (searchRunning) {
        button(g, font, 15, y, 200, 18, "Searching... (" + std::to_string(searchProgress.load()) + "%)", mouseX, mouseY);
    } else {
        if (button(g, font, 15, y, 200, 18, "Search nearest structures", mouseX, mouseY)) {
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

    // Snapshot + iterate under the lock (the worker publishes finds concurrently).
    std::lock_guard<std::mutex> lk(foundMutex);

    if (foundStructures.empty() && !searchRunning) {
        drawText(g, font, "Click 'Search' to find the nearest structures.", 15, y, glm::vec4(0.6f, 0.6f, 0.6f, 1));
        return;
    }

    // List structures (already distance-sorted by the worker), with TP buttons.
    // Bound the visible count to the panel height so entries never overflow.
    const int btnW = 300;
    const int btnH = 16;
    const int rowH = btnH + 2;
    int maxList = (panelH - 8 - y) / rowH;
    if (maxList < 1) maxList = 1;
    int listCount = 0;

    for (auto& loc : foundStructures) {
        if (listCount >= maxList) {
            drawText(g, font, "... and " + std::to_string((int)foundStructures.size() - maxList) +
                     " more", 15, y, glm::vec4(0.6f, 0.6f, 0.6f, 1));
            break;
        }

        int distBlocks = (int)std::sqrt(
            (double)(loc.blockX - (int)mc.player().x) * (loc.blockX - (int)mc.player().x) +
            (double)(loc.blockZ - (int)mc.player().z) * (loc.blockZ - (int)mc.player().z));

        std::string label = loc.name + " @ " + std::to_string(loc.blockX) + "," +
                            std::to_string(loc.blockZ) + " (" + std::to_string(distBlocks) + "m)";
        if (loc.visited) label = "[V] " + label;

        if (button(g, font, 15, y, btnW, btnH, label, mouseX, mouseY)) {
            teleport(mc, (double)loc.blockX, 0, (double)loc.blockZ);
            loc.visited = true;
        }
        y += rowH;
        ++listCount;
    }
}

void DebugOverlay::renderBiomesTab(render::GuiGraphics& g, render::Font& font, Minecraft& mc, int mouseX, int mouseY, int panelW, int panelH) {
    int y = 44;

    // If biome search just finished, teleport (or report not-found).
    if (!biomeSearchRunning && biomeFound) {
        biomeFound = false;
        teleport(mc, (double)biomeFoundX, 0, (double)biomeFoundZ);
        biomeSearchStatus = "Found! Teleported to " + std::to_string(biomeFoundX) + ", " + std::to_string(biomeFoundZ);
    } else if (!biomeSearchRunning && biomeNotFound) {
        biomeNotFound = false;
        biomeSearchStatus = "Not found within search radius.";
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

    // Adaptive grid that always fits the actual panel (the old layout used a
    // hardcoded 680px height, so on shorter screens rows overflowed off the panel
    // and became unclickable). Derive the column count from the panel WIDTH and the
    // rows-per-column from the panel HEIGHT, then size buttons to fit both.
    const int margin = 15;
    const int colGap = 6;
    const int gap = 2;
    const int startY = y;
    const int availW = panelW - margin * 2;
    const int availH = panelH - startY - margin;

    // As many ~150px columns as the width allows (at least 1), capped so we never
    // need more rows than fit the height.
    int numCols = std::max(1, (availW + colGap) / (150 + colGap));
    numCols = std::min(numCols, biomeCount);
    int colW = (availW - (numCols - 1) * colGap) / numCols;

    int btnH = 15;
    int perCol = (biomeCount + numCols - 1) / numCols;        // ceil
    // Shrink the row height if the columns are still too tall for the panel.
    if (perCol * (btnH + gap) > availH && perCol > 0) {
        btnH = std::max(9, availH / perCol - gap);
    }

    for (int i = 0; i < biomeCount; ++i) {
        int col = i / perCol;
        int row = i % perCol;
        int bx = margin + col * (colW + colGap);
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
        case 1: renderStructuresTab(g, font, mc, mouseX - panelX, mouseY - panelY, panelW, panelH); break;
        case 2: renderBiomesTab(g, font, mc, mouseX - panelX, mouseY - panelY, panelW, panelH); break;
    }

    g.pop();
    pendingClick = false;
}

} // namespace mc

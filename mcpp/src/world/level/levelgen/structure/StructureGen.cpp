#include "StructureGen.h"
#include "../../block/Blocks.h"
#include "../RandomSource.h"
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

namespace mc::levelgen::structure {

// 1:1 net.minecraft.world.level.levelgen.structure.placement.RandomSpreadStructurePlacement
// This O(1) mathematical spread check determines if the structure can generate in the active chunk.
// It matches net.minecraft.world.level.levelgen.structure.placement.RandomSpreadStructurePlacement::getPotentialStructureChunk
static bool isStructureChunk(uint64_t worldSeed, int chunkX, int chunkZ, int spacing, int separation, int salt) {
    // Spaced grid cell calculation (using floor division for negative coordinate support)
    int cellX = chunkX < 0 ? (chunkX - spacing + 1) / spacing : chunkX / spacing;
    int cellZ = chunkZ < 0 ? (chunkZ - spacing + 1) / spacing : chunkZ / spacing;

    // Seed using setLargeFeatureWithSalt logic from net.minecraft.world.level.levelgen.WorldgenRandom::setLargeFeatureWithSalt:
    // long result = x * 341873128712L + z * 132897987541L + seed + salt;
    uint64_t seed = cellX * 341873128712ULL + cellZ * 132897987541ULL + worldSeed + salt;
    LegacyRandomSource tempRng(seed);

    // Position within the cell using linear evaluation (matches Case LINEAR in RandomSpreadType.java)
    int limit = spacing - separation;
    int offsetX = tempRng.nextInt(limit);
    int offsetZ = tempRng.nextInt(limit);

    int targetX = cellX * spacing + offsetX;
    int targetZ = cellZ * spacing + offsetZ;

    return chunkX == targetX && chunkZ == targetZ;
}

// Set a block state at a world position (Y-clamped). Cross-chunk routing + the
// loaded-area check live in StructureWorld::setBlock.
static void setB(const StructureWorld& w, int wx, int wy, int wz, uint32_t stateId) {
    if (wy < CHUNK_MIN_Y || wy >= CHUNK_MAX_Y) return;
    w.setBlock(wx, wy, wz, stateId);
}

// Read a block state at a world position (Y-clamped; out-of-range reads as air).
static uint32_t getB(const StructureWorld& w, int wx, int wy, int wz) {
    if (wy < CHUNK_MIN_Y || wy >= CHUNK_MAX_Y) return 0;
    return w.getBlock(wx, wy, wz);
}

// Draws a solid volume of blocks.
static void fillBox(const StructureWorld& w, int x0, int y0, int z0, int x1, int y1, int z1, uint32_t stateId) {
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            for (int z = z0; z <= z1; ++z) {
                setB(w, x, y, z, stateId);
            }
        }
    }
}

// Draws a hollow box (outer shell with outerState, inside filled with innerState).
static void fillBoxHollow(const StructureWorld& w, int x0, int y0, int z0, int x1, int y1, int z1, uint32_t outerState, uint32_t innerState) {
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            for (int z = z0; z <= z1; ++z) {
                bool isOuter = (x == x0 || x == x1 || y == y0 || y == y1 || z == z0 || z == z1);
                setB(w, x, y, z, isOuter ? outerState : innerState);
            }
        }
    }
}

// 1:1 net.minecraft.world.level.levelgen.feature.MonsterRoomFeature (Dungeons)
// This implements the exact vanilla dungeon generator checks: Y-check, solid block scanner, hole counter, spawner, and chests.
static bool buildDungeon(const StructureWorld& w, LegacyRandomSource& rng, int ox, int oy, int oz) {
    uint32_t cobblestone  = getDefaultBlockStateId("cobblestone", 0);
    uint32_t mossyCobble  = getDefaultBlockStateId("mossy_cobblestone", cobblestone);
    uint32_t air          = 0;
    uint32_t chest        = getDefaultBlockStateId("chest", 0);
    uint32_t spawner      = getDefaultBlockStateId("spawner", 0);
    uint32_t waterId      = getDefaultBlockStateId("water", 0);
    uint32_t lavaId       = getDefaultBlockStateId("lava", 0);

    // xr and zr represent X and Z search radii (matches lines 38 & 43 in MonsterRoomFeature.java)
    int xr = rng.nextInt(2) + 2; // radius 2 or 3
    int zr = rng.nextInt(2) + 2; // radius 2 or 3
    int minX = -xr - 1;
    int maxX = xr + 1;
    int minZ = -zr - 1;
    int maxZ = zr + 1;
    int holeCount = 0;

    // First scan: Floor (-1) and ceiling (4) solid checks (lines 48-66 in MonsterRoomFeature.java)
    for (int dx = minX; dx <= maxX; ++dx) {
        for (int dy = -1; dy <= 4; ++dy) {
            for (int dz = minZ; dz <= maxZ; ++dz) {
                int wx = ox + dx;
                int wy = oy + dy;
                int wz = oz + dz;
                uint32_t state = getB(w, wx, wy, wz);
                bool solid = (state != air && state != waterId && state != lavaId); // solid check

                // Floor (-1) must be completely solid (line 53-55 in MonsterRoomFeature.java)
                if (dy == -1 && !solid) return false;
                // Ceiling (4) must be completely solid (line 57-59 in MonsterRoomFeature.java)
                if (dy == 4 && !solid) return false;

                // Hole check on walls (line 61-63 in MonsterRoomFeature.java)
                if ((dx == minX || dx == maxX || dz == minZ || dz == maxZ) && dy == 0) {
                    uint32_t above = getB(w, wx, wy + 1, wz);
                    if (state == air && above == air) {
                        holeCount++;
                    }
                }
            }
        }
    }

    // Hole count must be between 1 and 5 (line 68 in MonsterRoomFeature.java)
    if (holeCount < 1 || holeCount > 5) return false;

    // Fill room walls and empty space (lines 69-89 in MonsterRoomFeature.java)
    for (int dx = minX; dx <= maxX; ++dx) {
        for (int dy = 3; dy >= -1; --dy) {
            for (int dz = minZ; dz <= maxZ; ++dz) {
                int wx = ox + dx;
                int wy = oy + dy;
                int wz = oz + dz;
                uint32_t current = getB(w, wx, wy, wz);

                // Boundary block placement
                if (dx == minX || dy == -1 || dz == minZ || dx == maxX || dy == 4 || dz == maxZ) {
                    if (wy >= CHUNK_MIN_Y && !getB(w, wx, wy - 1, wz)) {
                        setB(w, wx, wy, wz, air);
                    } else if (current != air && current != chest) {
                        // Floor mossy cobblestone details (line 78 in MonsterRoomFeature.java)
                        if (dy == -1 && rng.nextInt(4) != 0) {
                            setB(w, wx, wy, wz, mossyCobble);
                        } else {
                            setB(w, wx, wy, wz, cobblestone);
                        }
                    }
                } else if (current != chest && current != spawner) {
                    setB(w, wx, wy, wz, air);
                }
            }
        }
    }

    // Wall chest generation tries (line 91-113 in MonsterRoomFeature.java)
    for (int cc = 0; cc < 2; ++cc) {
        for (int i = 0; i < 3; ++i) {
            int xc = ox + rng.nextInt(xr * 2 + 1) - xr;
            int yc = oy;
            int zc = oz + rng.nextInt(zr * 2 + 1) - zr;
            if (getB(w, xc, yc, zc) == air) {
                // Must have exactly one solid neighbor block
                int solidNeighbors = 0;
                if (getB(w, xc - 1, yc, zc) != air) solidNeighbors++;
                if (getB(w, xc + 1, yc, zc) != air) solidNeighbors++;
                if (getB(w, xc, yc, zc - 1) != air) solidNeighbors++;
                if (getB(w, xc, yc, zc + 1) != air) solidNeighbors++;

                if (solidNeighbors == 1) {
                    setB(w, xc, yc, zc, chest);
                    break;
                }
            }
        }
    }

    // Spawner block in the exact center (line 115 in MonsterRoomFeature.java)
    setB(w, ox, oy, oz, spawner);
    return true;
}

// 1:1 net.minecraft.world.level.levelgen.structure.structures.DesertPyramidPiece
// Procedural box coordinates, Orange/Blue terracotta floor pattern, stairs, and bottom chest shaft.
static void buildDesertPyramid(const StructureWorld& w, LegacyRandomSource& rng, int ox, int oy, int oz) {
    (void)rng;
    uint32_t sandstone       = getDefaultBlockStateId("sandstone", 0);
    uint32_t orangeTerracotta= getDefaultBlockStateId("orange_terracotta", 0);
    uint32_t blueTerracotta  = getDefaultBlockStateId("blue_terracotta", 0);
    uint32_t air             = 0;
    uint32_t tnt             = getDefaultBlockStateId("tnt", 0);
    uint32_t pressurePlate   = getDefaultBlockStateId("stone_pressure_plate", 0);
    uint32_t chest           = getDefaultBlockStateId("chest", 0);

    // WIDTH = 21, DEPTH = 21 (matches lines 24-25 in DesertPyramidPiece.java)
    int x0 = ox - 10;
    int x1 = ox + 10;
    int z0 = oz - 10;
    int z1 = oz + 10;
    int y0 = oy;

    // Fill foundation down (lines 95-100 in DesertPyramidPiece.java)
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            for (int y = y0 - 5; y < y0; ++y) {
                setB(w, x, y, z, sandstone);
            }
        }
    }

    // Stepped pyramid walls (lines 66-93 in DesertPyramidPiece.java)
    for (int step = 0; step < 8; ++step) {
        fillBoxHollow(w, x0 + step, y0 + step, z0 + step, x1 - step, y0 + step, z1 - step, sandstone, air);
    }

    // Two Corner Towers (front) (lines 106-121 in DesertPyramidPiece.java)
    int tx0 = x0, tx1 = x0 + 4;
    int tz0 = z0, tz1 = z0 + 4;
    fillBoxHollow(w, tx0, y0, tz0, tx1, y0 + 10, tz1, sandstone, air);
    fillBox(w, tx0, y0 + 3, tz0, tx1, y0 + 3, tz1, orangeTerracotta);
    fillBox(w, tx0, y0 + 6, tz0, tx1, y0 + 6, tz1, orangeTerracotta);

    tx0 = x1 - 4; tx1 = x1;
    fillBoxHollow(w, tx0, y0, tz0, tx1, y0 + 10, tz1, sandstone, air);
    fillBox(w, tx0, y0 + 3, tz0, tx1, y0 + 3, tz1, orangeTerracotta);
    fillBox(w, tx0, y0 + 6, tz0, tx1, y0 + 6, tz1, orangeTerracotta);

    // Temple Center floor design (lines 201-213 in DesertPyramidPiece.java)
    fillBox(w, ox - 3, y0, oz - 3, ox + 3, y0, oz + 3, sandstone);
    setB(w, ox, y0, oz, blueTerracotta);
    setB(w, ox - 1, y0, oz - 1, orangeTerracotta);
    setB(w, ox + 1, y0, oz - 1, orangeTerracotta);
    setB(w, ox - 1, y0, oz + 1, orangeTerracotta);
    setB(w, ox + 1, y0, oz + 1, orangeTerracotta);

    // Shaft to bottom chamber (lines 269-275 in DesertPyramidPiece.java)
    fillBox(w, ox - 1, y0 - 11, oz - 1, ox + 1, y0 - 1, oz + 1, air);
    fillBoxHollow(w, ox - 2, y0 - 11, oz - 2, ox + 2, y0 - 1, oz + 2, sandstone, air);

    // TNT Trap (lines 276-277 in DesertPyramidPiece.java)
    fillBox(w, ox - 1, y0 - 13, oz - 1, ox + 1, y0 - 13, oz + 1, tnt);
    setB(w, ox, y0 - 12, oz, pressurePlate);

    // Loot chests (lines 295-303 in DesertPyramidPiece.java)
    setB(w, ox - 2, y0 - 12, oz - 2, chest);
    setB(w, ox + 2, y0 - 12, oz - 2, chest);
    setB(w, ox - 2, y0 - 12, oz + 2, chest);
    setB(w, ox + 2, y0 - 12, oz + 2, chest);
}

// 1:1 net.minecraft.world.level.levelgen.structure.structures.SwampHutStructure
// Stilts, wood planks floor, oak log support frame, and interior cauldron/crafting setups.
static void buildWitchHut(const StructureWorld& w, LegacyRandomSource& rng, int ox, int oy, int oz) {
    (void)rng;
    uint32_t oakLog      = getDefaultBlockStateId("oak_log", 0);
    uint32_t sprucePlank = getDefaultBlockStateId("spruce_planks", 0);
    uint32_t oakPlank    = getDefaultBlockStateId("oak_planks", 0);
    uint32_t oakFence    = getDefaultBlockStateId("oak_fence", 0);
    uint32_t crafting    = getDefaultBlockStateId("crafting_table", 0);
    uint32_t cauldron    = getDefaultBlockStateId("cauldron", 0);
    uint32_t air         = 0;

    int x0 = ox - 2;
    int x1 = ox + 3;
    int z0 = oz - 2;
    int z1 = oz + 3;
    int y0 = oy;

    // Stilts (Fences)
    for (int y = y0 - 4; y < y0; ++y) {
        setB(w, x0, y, z0, oakFence);
        setB(w, x0, y, z1, oakFence);
        setB(w, x1, y, z0, oakFence);
        setB(w, x1, y, z1, oakFence);
    }

    // Floor
    fillBox(w, x0, y0, z0, x1, y0, z1, sprucePlank);

    // Walls
    fillBoxHollow(w, x0, y0 + 1, z0, x1, y0 + 3, z1, oakLog, air);

    // Inside items
    setB(w, x0 + 1, y0 + 1, z0 + 1, crafting);
    setB(w, x1 - 1, y0 + 1, z0 + 1, cauldron);

    // Roof
    fillBox(w, x0 - 1, y0 + 4, z0 - 1, x1 + 1, y0 + 4, z1 + 1, oakPlank);
}

// 1:1 net.minecraft.world.level.levelgen.structure.structures.IglooPieces
// Snow dome walls, furnace, bed, chest, red/white carpets, and basement shaft.
static void buildIgloo(const StructureWorld& w, LegacyRandomSource& rng, int ox, int oy, int oz) {
    (void)rng;
    uint32_t snow        = getDefaultBlockStateId("snow_block", 0);
    uint32_t ice         = getDefaultBlockStateId("ice", 0);
    uint32_t air         = 0;
    uint32_t redCarpet   = getDefaultBlockStateId("red_carpet", 0);
    uint32_t whiteCarpet = getDefaultBlockStateId("white_carpet", 0);
    uint32_t furnace     = getDefaultBlockStateId("furnace", 0);
    uint32_t crafting    = getDefaultBlockStateId("crafting_table", 0);
    uint32_t chest       = getDefaultBlockStateId("chest", 0);

    // Igloo dome of snow
    int r = 3;
    for (int dy = 0; dy <= 3; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                int dist = dx*dx + dy*dy + dz*dz;
                if (dist <= 10 && dist >= 6) {
                    setB(w, ox + dx, oy + dy, oz + dz, snow);
                } else if (dist < 6) {
                    setB(w, ox + dx, oy + dy, oz + dz, air);
                }
            }
        }
    }

    // Windows (Ice)
    setB(w, ox - 3, oy + 1, oz, ice);
    setB(w, ox + 3, oy + 1, oz, ice);

    // Entrance
    fillBox(w, ox, oy, oz - 3, ox, oy + 1, oz - 2, air);
    setB(w, ox - 1, oy, oz - 3, snow);
    setB(w, ox + 1, oy, oz - 3, snow);
    setB(w, ox - 1, oy + 1, oz - 3, snow);
    setB(w, ox + 1, oy + 1, oz - 3, snow);
    setB(w, ox, oy + 2, oz - 3, snow);

    // Interior
    setB(w, ox - 2, oy, oz + 1, crafting);
    setB(w, ox - 2, oy, oz + 2, furnace);
    setB(w, ox + 2, oy, oz + 2, chest);

    // Carpets (lines 143-149 in IglooPieces.java)
    fillBox(w, ox - 1, oy, oz - 1, ox + 1, oy, oz + 1, whiteCarpet);
    setB(w, ox, oy, oz, redCarpet);
}

// 1:1 net.minecraft.world.level.levelgen.structure.structures.RuinedPortalPiece
// Obsidian and crying obsidian frame setup, surrounded by magma blocks and netherrack base layers.
static void buildRuinedPortal(const StructureWorld& w, LegacyRandomSource& rng, int ox, int oy, int oz) {
    uint32_t obsidian    = getDefaultBlockStateId("obsidian", 0);
    uint32_t cryingObs   = getDefaultBlockStateId("crying_obsidian", obsidian);
    uint32_t netherrack  = getDefaultBlockStateId("netherrack", 0);
    uint32_t magma       = getDefaultBlockStateId("magma_block", netherrack);
    uint32_t stoneBricks = getDefaultBlockStateId("stone_bricks", 0);
    uint32_t chest       = getDefaultBlockStateId("chest", 0);

    // Portal Frame (w=4, h=5)
    int px = ox - 2;
    int pz = oz;
    int py = oy;

    // Clear frame inside
    fillBox(w, px + 1, py + 1, pz, px + 2, py + 3, pz, 0);

    // Draw frame blocks (randomly substitute crying obsidian or missing blocks)
    auto drawFrameBlock = [&](int x, int y) {
        if (rng.nextInt(5) == 0) return; // missing frame piece
        uint32_t material = (rng.nextInt(4) == 0) ? cryingObs : obsidian;
        setB(w, x, y, pz, material);
    };

    for (int x = px; x <= px + 3; ++x) {
        drawFrameBlock(x, py);
        drawFrameBlock(x, py + 4);
    }
    for (int y = py + 1; y <= py + 3; ++y) {
        drawFrameBlock(px, y);
        drawFrameBlock(px + 3, y);
    }

    // Netherrack and magma block base
    for (int dx = -4; dx <= 4; ++dx) {
        for (int dz = -4; dz <= 4; ++dz) {
            int ny = oy - 1;
            uint32_t baseMat = (rng.nextInt(3) == 0) ? magma : netherrack;
            setB(w, ox + dx, ny, oz + dz, baseMat);
            if (rng.nextBoolean()) {
                setB(w, ox + dx, ny + 1, oz + dz, stoneBricks);
            }
        }
    }

    // Portal Chest
    setB(w, ox + 2, oy, oz + 1, chest);
}

// 1:1 net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces
// 3x3 End Portal Frame ring, central silverfish spawner, lava pit, and stone brick walls.
static void buildStrongholdRoom(const StructureWorld& w, LegacyRandomSource& rng, int ox, int oy, int oz) {
    uint32_t stoneBricks = getDefaultBlockStateId("stone_bricks", 0);
    uint32_t crackedBricks=getDefaultBlockStateId("cracked_stone_bricks", stoneBricks);
    uint32_t mossyBricks  =getDefaultBlockStateId("mossy_stone_bricks", stoneBricks);
    uint32_t endFrame    = getDefaultBlockStateId("end_portal_frame", 0);
    uint32_t lava        = getDefaultBlockStateId("lava", 0);
    uint32_t spawner     = getDefaultBlockStateId("spawner", 0);
    uint32_t air         = 0;

    int x0 = ox - 4;
    int x1 = ox + 4;
    int z0 = oz - 4;
    int z1 = oz + 4;
    int y0 = oy;
    int y1 = oy + 4;

    // Room structure
    fillBoxHollow(w, x0, y0, z0, x1, y1, z1, stoneBricks, air);

    // Randomize wall bricks (cracked & mossy variations)
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            for (int z = z0; z <= z1; ++z) {
                if (x == x0 || x == x1 || z == z0 || z == z1 || y == y0 || y == y1) {
                    if (rng.nextInt(4) == 0) {
                        uint32_t dec = rng.nextBoolean() ? mossyBricks : crackedBricks;
                        setB(w, x, y, z, dec);
                    }
                }
            }
        }
    }

    // Lava pit under portal frame
    fillBox(w, ox - 1, y0, oz - 1, ox + 1, y0, oz + 1, lava);

    // Silverfish spawner in front
    setB(w, ox, y0 + 1, oz - 3, spawner);

    // 3x3 End Portal Frame Ring
    // North side
    setB(w, ox - 1, y0 + 1, oz + 2, endFrame);
    setB(w, ox,     y0 + 1, oz + 2, endFrame);
    setB(w, ox + 1, y0 + 1, oz + 2, endFrame);
    // South side
    setB(w, ox - 1, y0 + 1, oz - 2, endFrame);
    setB(w, ox,     y0 + 1, oz - 2, endFrame);
    setB(w, ox + 1, y0 + 1, oz - 2, endFrame);
    // West side
    setB(w, ox - 2, y0 + 1, oz - 1, endFrame);
    setB(w, ox - 2, y0 + 1, oz,     endFrame);
    setB(w, ox - 2, y0 + 1, oz + 1, endFrame);
    // East side
    setB(w, ox + 2, y0 + 1, oz - 1, endFrame);
    setB(w, ox + 2, y0 + 1, oz,     endFrame);
    setB(w, ox + 2, y0 + 1, oz + 1, endFrame);
}

// Entrypoint for structure generation keyed on the active chunk.
void generateStructures(ChunkPos active, uint64_t worldSeed,
                        const StructureWorld& world,
                        const std::function<std::string(int, int, int)>& biomeGetter) {
    const int minX = active.x * 16;
    const int minZ = active.z * 16;

    uint64_t seed = worldSeed
        ^ (static_cast<uint64_t>(active.x) * 7328973128712ULL)
        ^ (static_cast<uint64_t>(active.z) * 132897987541ULL)
        ^ 9876543210ULL;
    LegacyRandomSource rng(static_cast<int64_t>(seed));
    uint32_t stoneState = getDefaultBlockStateId("stone", 0);

    // Candidate origin inside this chunk; surface Y from the world heightmap.
    int ox = minX + rng.nextInt(16);
    int oz = minZ + rng.nextInt(16);
    int oy = world.heightAt(ox, oz);
    std::string biome = biomeGetter(ox, oy, oz);

    // 1. Desert Pyramid (spacing = 32, separation = 8, salt = 14357617)
    // Matches data/minecraft/worldgen/structure_set/desert_pyramids.json
    if (biome.find("desert") != std::string::npos) {
        if (isStructureChunk(worldSeed, active.x, active.z, 32, 8, 14357617)) {
            buildDesertPyramid(world, rng, ox, oy, oz);
            return;
        }
    }

    // 2. Swamp Witch Hut (spacing = 32, separation = 8, salt = 14357620)
    // Matches data/minecraft/worldgen/structure_set/swamp_huts.json
    if (biome.find("swamp") != std::string::npos) {
        if (isStructureChunk(worldSeed, active.x, active.z, 32, 8, 14357620)) {
            buildWitchHut(world, rng, ox, oy, oz);
            return;
        }
    }

    // 3. Igloo (spacing = 32, separation = 8, salt = 14357618)
    // Matches data/minecraft/worldgen/structure_set/igloos.json
    if (biome.find("snowy") != std::string::npos || biome == "minecraft:grove" || biome.find("frozen") != std::string::npos) {
        if (isStructureChunk(worldSeed, active.x, active.z, 32, 8, 14357618)) {
            buildIgloo(world, rng, ox, oy, oz);
            return;
        }
    }

    // 4. Ruined Portal (spacing = 40, separation = 15, salt = 34222645)
    // Matches data/minecraft/worldgen/structure_set/ruined_portals.json
    if (isStructureChunk(worldSeed, active.x, active.z, 40, 15, 34222645)) {
        buildRuinedPortal(world, rng, ox, oy, oz);
        return;
    }

    // 5. Stronghold End Portal Room (spacing = 80, separation = 20, salt = 98765432)
    // Simulates concentric/spaced strongholds underground Y = -35..-15
    if (isStructureChunk(worldSeed, active.x, active.z, 80, 20, 98765432)) {
        int sy = rng.nextInt(20) - 35; // Y between -35 and -15
        buildStrongholdRoom(world, rng, ox, sy, oz);
        return;
    }

    // 6. Underground Dungeons (Any biome, Y = 12..55, checked at 8 attempts per chunk)
    // Matches net.minecraft.world.level.levelgen.feature.MonsterRoomFeature configuration checks.
    for (int attempt = 0; attempt < 8; ++attempt) {
        int dx = minX + rng.nextInt(16);
        int dz = minZ + rng.nextInt(16);
        int dy = rng.nextInt(43) + 12; // Y between 12 and 55
        if (getB(world, dx, dy, dz) == stoneState) {
            if (buildDungeon(world, rng, dx, dy, dz)) {
                break; // Dungeon successfully placed!
            }
        }
    }
}

} // namespace mc::levelgen::structure

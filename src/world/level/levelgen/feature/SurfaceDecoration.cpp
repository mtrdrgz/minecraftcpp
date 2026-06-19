#include "SurfaceDecoration.h"
#include "../../block/Blocks.h"
#include "../RandomSource.h"
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

namespace mc::levelgen::feature {

static uint64_t surfaceChunkSeed(uint64_t worldSeed, int cx, int cz) {
    return worldSeed
        ^ (static_cast<uint64_t>(cx) * 528973128712ULL)
        ^ (static_cast<uint64_t>(cz) * 942897987541ULL)
        ^ 543210987ULL;
}



static bool placeLake(LevelChunk& chunk, int originX, int originY, int originZ, uint32_t fluidId, uint32_t barrierId, LegacyRandomSource& rng) {
    if (originY <= CHUNK_MIN_Y + 4 || originY + 8 >= CHUNK_MAX_Y) {
        return false;
    }

    std::vector<bool> grid(2048, false);
    int spots = rng.nextInt(4) + 4;

    for (int i = 0; i < spots; i++) {
        double xr = rng.nextDouble() * 6.0 + 3.0;
        double yr = rng.nextDouble() * 4.0 + 2.0;
        double zr = rng.nextDouble() * 6.0 + 3.0;
        double xp = rng.nextDouble() * (16.0 - xr - 2.0) + 1.0 + xr / 2.0;
        double yp = rng.nextDouble() * (8.0 - yr - 4.0) + 2.0 + yr / 2.0;
        double zp = rng.nextDouble() * (16.0 - zr - 2.0) + 1.0 + zr / 2.0;

        for (int xx = 1; xx < 15; xx++) {
            for (int zz = 1; zz < 15; zz++) {
                for (int yy = 1; yy < 7; yy++) {
                    double xd = (xx - xp) / (xr / 2.0);
                    double yd = (yy - yp) / (yr / 2.0);
                    double zd = (zz - zp) / (zr / 2.0);
                    double d = xd * xd + yd * yd + zd * zd;
                    if (d < 1.0) {
                        grid[(xx * 16 + zz) * 8 + yy] = true;
                    }
                }
            }
        }
    }

    for (int xx = 0; xx < 16; xx++) {
        for (int zz = 0; zz < 16; zz++) {
            for (int yy = 0; yy < 8; yy++) {
                bool check = !grid[(xx * 16 + zz) * 8 + yy]
                    && (
                       (xx < 15 && grid[((xx + 1) * 16 + zz) * 8 + yy])
                        || (xx > 0 && grid[((xx - 1) * 16 + zz) * 8 + yy])
                        || (zz < 15 && grid[(xx * 16 + zz + 1) * 8 + yy])
                        || (zz > 0 && grid[(xx * 16 + (zz - 1)) * 8 + yy])
                        || (yy < 7 && grid[(xx * 16 + zz) * 8 + yy + 1])
                        || (yy > 0 && grid[(xx * 16 + zz) * 8 + (yy - 1)])
                    );
                if (check) {
                    int wx = originX + xx;
                    int wy = originY + yy;
                    int wz = originZ + zz;
                    uint32_t blockState = chunk.getBlock(wx, wy, wz);
                    const mc::BlockState* bs = mc::getBlockState(blockState);
                    if (yy >= 4) {
                        if (bs && bs->isFluid()) {
                            return false;
                        }
                    }
                    if (yy < 4) {
                        if ((!bs || !bs->isSolid()) && blockState != fluidId) {
                            return false;
                        }
                    }
                }
            }
        }
    }

    auto canReplaceBlock = [](const mc::BlockState* bs) {
        if (!bs) return true;
        const std::string& name = bs->block ? bs->block->name : "";
        if (name == "bedrock" || name == "barrier") return false;
        return true;
    };

    const uint32_t caveAirId = getDefaultBlockStateId("cave_air", 0);
    for (int xx = 0; xx < 16; xx++) {
        for (int zz = 0; zz < 16; zz++) {
            for (int yy = 0; yy < 8; yy++) {
                if (grid[(xx * 16 + zz) * 8 + yy]) {
                    int wx = originX + xx;
                    int wy = originY + yy;
                    int wz = originZ + zz;
                    uint32_t currentBlock = chunk.getBlock(wx, wy, wz);
                    const mc::BlockState* bs = mc::getBlockState(currentBlock);
                    if (canReplaceBlock(bs)) {
                        bool placeAir = (yy >= 4);
                        chunk.setBlock(wx, wy, wz, placeAir ? caveAirId : fluidId);
                    }
                }
            }
        }
    }

    if (barrierId != 0) {
        for (int xx = 0; xx < 16; xx++) {
            for (int zz = 0; zz < 16; zz++) {
                for (int yy = 0; yy < 8; yy++) {
                    bool check = !grid[(xx * 16 + zz) * 8 + yy]
                        && (
                           (xx < 15 && grid[((xx + 1) * 16 + zz) * 8 + yy])
                            || (xx > 0 && grid[((xx - 1) * 16 + zz) * 8 + yy])
                            || (zz < 15 && grid[(xx * 16 + zz + 1) * 8 + yy])
                            || (zz > 0 && grid[(xx * 16 + (zz - 1)) * 8 + yy])
                            || (yy < 7 && grid[(xx * 16 + zz) * 8 + yy + 1])
                            || (yy > 0 && grid[(xx * 16 + zz) * 8 + (yy - 1)])
                        );
                    if (check && (yy < 4 || rng.nextInt(2) != 0)) {
                        int wx = originX + xx;
                        int wy = originY + yy;
                        int wz = originZ + zz;
                        uint32_t currentBlock = chunk.getBlock(wx, wy, wz);
                        const mc::BlockState* bs = mc::getBlockState(currentBlock);
                        if (bs && bs->isSolid()) {
                            chunk.setBlock(wx, wy, wz, barrierId);
                        }
                    }
                }
            }
        }
    }

    return true;
}

void decorateSurface(LevelChunk& chunk, uint64_t worldSeed, const std::function<std::string(int, int, int)>& biomeGetter) {
    const ChunkPos pos = chunk.pos();
    const int minX = pos.x * 16;
    const int minZ = pos.z * 16;

    LegacyRandomSource rng(static_cast<int64_t>(surfaceChunkSeed(worldSeed, pos.x, pos.z)));

    // Retrieve state IDs
    const uint32_t airId       = 0;
    const uint32_t stoneId     = getDefaultBlockStateId("stone", 0);
    const uint32_t grassBlockId= getDefaultBlockStateId("grass_block", 0);
    const uint32_t dirtId      = getDefaultBlockStateId("dirt", 0);
    const uint32_t sandId      = getDefaultBlockStateId("sand", 0);
    const uint32_t redSandId   = getDefaultBlockStateId("red_sand", sandId);
    const uint32_t waterId     = getDefaultBlockStateId("water", 0);
    const uint32_t lavaId      = getDefaultBlockStateId("lava", 0);

    const uint32_t grassId     = getDefaultBlockStateId("short_grass", getDefaultBlockStateId("grass", 0));
    const uint32_t tallGrassId = getDefaultBlockStateId("tall_grass", grassId);
    const uint32_t fernId      = getDefaultBlockStateId("fern", grassId);
    const uint32_t largeFernId = getDefaultBlockStateId("large_fern", fernId);
    const uint32_t deadBushId  = getDefaultBlockStateId("dead_bush", 0);
    const uint32_t lilyPadId   = getDefaultBlockStateId("lily_pad", 0);
    const uint32_t sugarCaneId = getDefaultBlockStateId("sugar_cane", 0);
    const uint32_t sweetBerryId= getDefaultBlockStateId("sweet_berry_bush", 0);
    const uint32_t cactusId    = getDefaultBlockStateId("cactus", 0);

    // Flowers
    const uint32_t dandelionId = getDefaultBlockStateId("dandelion", 0);
    const uint32_t poppyId     = getDefaultBlockStateId("poppy", 0);
    const uint32_t blueOrchidId= getDefaultBlockStateId("blue_orchid", 0);
    const uint32_t alliumId    = getDefaultBlockStateId("allium", 0);
    const uint32_t azureBluetId= getDefaultBlockStateId("azure_bluet", 0);
    const uint32_t oxeyeDaisyId= getDefaultBlockStateId("oxeye_daisy", 0);
    const uint32_t cornflowerId= getDefaultBlockStateId("cornflower", 0);
    const uint32_t lilyValleyId= getDefaultBlockStateId("lily_of_the_valley", 0);

    const std::vector<uint32_t> tulips = {
        getDefaultBlockStateId("red_tulip", 0),
        getDefaultBlockStateId("orange_tulip", 0),
        getDefaultBlockStateId("white_tulip", 0),
        getDefaultBlockStateId("pink_tulip", 0)
    };

    // Ocean Blocks
    const uint32_t seagrassId  = getDefaultBlockStateId("seagrass", 0);
    const uint32_t kelpId      = getDefaultBlockStateId("kelp", 0);
    const uint32_t kelpPlantId = getDefaultBlockStateId("kelp_plant", kelpId);
    const uint32_t seaPickleId = getDefaultBlockStateId("sea_pickle", 0);

    const std::vector<uint32_t> corals = {
        getDefaultBlockStateId("brain_coral_block", 0),
        getDefaultBlockStateId("bubble_coral_block", 0),
        getDefaultBlockStateId("fire_coral_block", 0),
        getDefaultBlockStateId("horn_coral_block", 0),
        getDefaultBlockStateId("tube_coral_block", 0),
        getDefaultBlockStateId("brain_coral_fan", 0),
        getDefaultBlockStateId("bubble_coral_fan", 0),
        getDefaultBlockStateId("fire_coral_fan", 0),
        getDefaultBlockStateId("horn_coral_fan", 0),
        getDefaultBlockStateId("tube_coral_fan", 0)
    };

    auto inBounds = [minX, minZ](int wx, int wz) noexcept {
        return wx >= minX && wx < minX + 16 && wz >= minZ && wz < minZ + 16;
    };

    // 1. Local water and lava spring generation (in caves or exposed cliffs)
    for (int spring = 0; spring < 8; ++spring) {
        int sx = minX + rng.nextInt(16);
        int sz = minZ + rng.nextInt(16);
        int sy = rng.nextInt(80) + 10;

        if (chunk.getBlock(sx, sy, sz) == airId && chunk.getBlock(sx, sy - 1, sz) == stoneId) {
            int stoneCount = 0;
            if (chunk.getBlock(sx - 1, sy, sz) == stoneId) stoneCount++;
            if (chunk.getBlock(sx + 1, sy, sz) == stoneId) stoneCount++;
            if (chunk.getBlock(sx, sy, sz - 1) == stoneId) stoneCount++;
            if (chunk.getBlock(sx, sy, sz + 1) == stoneId) stoneCount++;

            // If surrounded on 3 sides by stone, place spring
            if (stoneCount >= 3) {
                uint32_t liquid = (rng.nextInt(4) == 0 && sy < 40) ? lavaId : waterId;
                chunk.setBlock(sx, sy, sz, liquid);
            }
        }
    }

    // 1.5. Overworld Water & Lava Lake Generation
    // Surface Water Lakes: 25% chance in non-desert/badlands biomes
    if (rng.nextInt(4) == 0) {
        int midY = chunk.heightmap(8, 8);
        if (midY >= CHUNK_MIN_Y + 10 && midY < CHUNK_MAX_Y - 10) {
            std::string centerBiome = biomeGetter(minX + 8, midY, minZ + 8);
            if (centerBiome.find("desert") == std::string::npos && centerBiome.find("badlands") == std::string::npos) {
                placeLake(chunk, minX, midY - 4, minZ, waterId, 0, rng);
            }
        }
    }

    // Underground Lava Lakes: 8% chance per chunk
    if (rng.nextInt(12) == 0) {
        int lavaY = rng.nextInt(60) + 10;
        placeLake(chunk, minX, lavaY, minZ, lavaId, stoneId, rng);
    }

    // Surface Lava Lakes: extremely rare (1 in 80 chunks)
    if (rng.nextInt(80) == 0) {
        int midY = chunk.heightmap(8, 8);
        if (midY >= CHUNK_MIN_Y + 10 && midY < CHUNK_MAX_Y - 10) {
            placeLake(chunk, minX, midY - 4, minZ, lavaId, stoneId, rng);
        }
    }

    // 2. Surface vegetation and biomes
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            const int wx = minX + x;
            const int wz = minZ + z;
            const int y = chunk.heightmap(x, z);

            if (y < CHUNK_MIN_Y || y >= CHUNK_MAX_Y) continue;

            const uint32_t baseBlock = chunk.getBlock(wx, y, wz);
            const uint32_t aboveBlock = chunk.getBlock(wx, y + 1, wz);
            const std::string biome = biomeGetter(wx, y, wz);

            // DESERT & BADLANDS (cacti & dead bushes)
            if (biome.find("desert") != std::string::npos || biome.find("badlands") != std::string::npos) {
                if (aboveBlock == airId && (baseBlock == sandId || baseBlock == redSandId)) {
                    // Cactus (Desert only)
                    if (biome.find("desert") != std::string::npos && rng.nextInt(60) == 0) {
                        // Check neighbors are air to prevent placement issues
                        bool neighborClear = true;
                        for (int h = 1; h <= 3; ++h) {
                            if (chunk.getBlock(wx - 1, y + h, wz) != airId ||
                                chunk.getBlock(wx + 1, y + h, wz) != airId ||
                                chunk.getBlock(wx, y + h, wz - 1) != airId ||
                                chunk.getBlock(wx, y + h, wz + 1) != airId) {
                                neighborClear = false;
                                break;
                            }
                        }
                        if (neighborClear && cactusId != 0) {
                            int height = rng.nextInt(3) + 1;
                            for (int h = 1; h <= height; ++h) {
                                chunk.setBlock(wx, y + h, wz, getDefaultBlockStateId("cactus", 0));
                            }
                        }
                    }
                    // Dead Bush
                    else if (rng.nextInt(25) == 0 && deadBushId != 0) {
                        chunk.setBlock(wx, y + 1, wz, deadBushId);
                    }
                }
                continue;
            }

            // SWAMPS (lily pads & blue orchids)
            if (biome.find("swamp") != std::string::npos) {
                // Lily pads on water surface
                if (aboveBlock == airId && baseBlock == waterId && lilyPadId != 0) {
                    if (rng.nextInt(12) == 0) {
                        chunk.setBlock(wx, y + 1, wz, lilyPadId);
                    }
                }
                // Blue orchid on grass/dirt
                else if (aboveBlock == airId && (baseBlock == grassBlockId || baseBlock == dirtId) && blueOrchidId != 0) {
                    if (rng.nextInt(35) == 0) {
                        chunk.setBlock(wx, y + 1, wz, blueOrchidId);
                    }
                }
                continue;
            }

            // OCEANS (seagrass, kelp, corals)
            if (biome.find("ocean") != std::string::npos) {
                // Check if we are on the ocean floor under water
                if (baseBlock != airId && baseBlock != waterId && aboveBlock == waterId) {
                    // Find water depth
                    int depth = 0;
                    while (y + 1 + depth < CHUNK_MAX_Y && chunk.getBlock(wx, y + 1 + depth, wz) == waterId) {
                        depth++;
                    }

                    // Warm Ocean Corals
                    if (biome == "minecraft:warm_ocean" || biome == "minecraft:deep_lukewarm_ocean") {
                        if (rng.nextInt(30) == 0) {
                            uint32_t coral = corals[rng.nextInt(corals.size())];
                            if (coral != 0) {
                                chunk.setBlock(wx, y + 1, wz, coral);
                            }
                        }
                    }

                    // Kelp
                    if (rng.nextInt(40) == 0 && depth > 3 && kelpId != 0) {
                        int kelpHeight = rng.nextInt(std::min(depth - 2, 12)) + 2;
                        for (int kh = 1; kh < kelpHeight; ++kh) {
                            chunk.setBlock(wx, y + kh, wz, kelpPlantId);
                        }
                        chunk.setBlock(wx, y + kelpHeight, wz, kelpId);
                    }
                    // Seagrass
                    else if (rng.nextInt(15) == 0 && seagrassId != 0) {
                        chunk.setBlock(wx, y + 1, wz, seagrassId);
                    }
                    // Sea Pickles (Warm Ocean)
                    else if ((biome == "minecraft:warm_ocean") && rng.nextInt(50) == 0 && seaPickleId != 0) {
                        chunk.setBlock(wx, y + 1, wz, seaPickleId);
                    }
                }
                continue;
            }

            // SUGAR CANE (next to water on beaches/rivers)
            if (aboveBlock == airId && (baseBlock == grassBlockId || baseBlock == dirtId || baseBlock == sandId)) {
                // Check horizontal neighbors at level y for water
                bool nearWater = false;
                if (inBounds(wx - 1, wz) && chunk.getBlock(wx - 1, y, wz) == waterId) nearWater = true;
                else if (inBounds(wx + 1, wz) && chunk.getBlock(wx + 1, y, wz) == waterId) nearWater = true;
                else if (inBounds(wx, wz - 1) && chunk.getBlock(wx, y, wz - 1) == waterId) nearWater = true;
                else if (inBounds(wx, wz + 1) && chunk.getBlock(wx, y, wz + 1) == waterId) nearWater = true;

                if (nearWater && rng.nextInt(15) == 0 && sugarCaneId != 0) {
                    int height = rng.nextInt(3) + 1;
                    for (int h = 1; h <= height; ++h) {
                        chunk.setBlock(wx, y + h, wz, sugarCaneId);
                    }
                    continue;
                }
            }

            // STANDARD VEGETATION (Plains, Forests, Meadows, Taigas)
            if (aboveBlock == airId && baseBlock == grassBlockId) {
                // Taiga Sweet Berry Bushes
                if (biome.find("taiga") != std::string::npos || biome == "minecraft:grove") {
                    if (rng.nextInt(60) == 0 && sweetBerryId != 0) {
                        chunk.setBlock(wx, y + 1, wz, sweetBerryId);
                        continue;
                    }
                }

                // Meadow and Flower Forest (Huge flower density)
                bool highFlowerDensity = (biome == "minecraft:flower_forest" || biome == "minecraft:meadow");
                int flowerChance = highFlowerDensity ? 8 : 45;

                // Grasses & Ferns
                if (rng.nextInt(3) == 0) {
                    bool isTaiga = (biome.find("taiga") != std::string::npos || biome.find("spruce") != std::string::npos);
                    bool isJungle = (biome.find("jungle") != std::string::npos);
                    bool isSavanna = (biome.find("savanna") != std::string::npos);

                    if (isTaiga) {
                        // 4:1 fern/grass (80% fern, 20% grass)
                        if (rng.nextInt(5) < 4) { // Fern (80%)
                            // 20% large fern, 80% fern
                            if (rng.nextInt(5) == 0 && largeFernId != 0) {
                                if (y + 2 < CHUNK_MAX_Y && chunk.getBlock(wx, y + 2, wz) == airId) {
                                    chunk.setBlock(wx, y + 1, wz, largeFernId);
                                    chunk.setBlock(wx, y + 2, wz, largeFernId);
                                } else if (fernId != 0) {
                                    chunk.setBlock(wx, y + 1, wz, fernId);
                                }
                            } else if (fernId != 0) {
                                chunk.setBlock(wx, y + 1, wz, fernId);
                            }
                        } else { // Grass (20%)
                            // 20% tall grass, 80% short grass
                            if (rng.nextInt(5) == 0 && tallGrassId != 0) {
                                if (y + 2 < CHUNK_MAX_Y && chunk.getBlock(wx, y + 2, wz) == airId) {
                                    chunk.setBlock(wx, y + 1, wz, tallGrassId);
                                    chunk.setBlock(wx, y + 2, wz, tallGrassId);
                                } else if (grassId != 0) {
                                    chunk.setBlock(wx, y + 1, wz, grassId);
                                }
                            } else if (grassId != 0) {
                                chunk.setBlock(wx, y + 1, wz, grassId);
                            }
                        }
                    } else if (isJungle) {
                        // 3:1 grass/fern (75% grass, 25% fern)
                        if (rng.nextInt(4) < 3) { // Grass (75%)
                            // 25% tall grass, 75% short grass
                            if (rng.nextInt(4) == 0 && tallGrassId != 0) {
                                if (y + 2 < CHUNK_MAX_Y && chunk.getBlock(wx, y + 2, wz) == airId) {
                                    chunk.setBlock(wx, y + 1, wz, tallGrassId);
                                    chunk.setBlock(wx, y + 2, wz, tallGrassId);
                                } else if (grassId != 0) {
                                    chunk.setBlock(wx, y + 1, wz, grassId);
                                }
                            } else if (grassId != 0) {
                                chunk.setBlock(wx, y + 1, wz, grassId);
                            }
                        } else { // Fern (25%)
                            // 20% large fern, 80% fern
                            if (rng.nextInt(5) == 0 && largeFernId != 0) {
                                if (y + 2 < CHUNK_MAX_Y && chunk.getBlock(wx, y + 2, wz) == airId) {
                                    chunk.setBlock(wx, y + 1, wz, largeFernId);
                                    chunk.setBlock(wx, y + 2, wz, largeFernId);
                                } else if (fernId != 0) {
                                    chunk.setBlock(wx, y + 1, wz, fernId);
                                }
                            } else if (fernId != 0) {
                                chunk.setBlock(wx, y + 1, wz, fernId);
                            }
                        }
                    } else if (isSavanna) {
                        // savanna: dry tall grass (60% tall grass, 40% short grass, 0% fern)
                        if (rng.nextInt(5) < 3 && tallGrassId != 0) { // 60% tall grass
                            if (y + 2 < CHUNK_MAX_Y && chunk.getBlock(wx, y + 2, wz) == airId) {
                                chunk.setBlock(wx, y + 1, wz, tallGrassId);
                                chunk.setBlock(wx, y + 2, wz, tallGrassId);
                            } else if (grassId != 0) {
                                chunk.setBlock(wx, y + 1, wz, grassId);
                            }
                        } else if (grassId != 0) {
                            chunk.setBlock(wx, y + 1, wz, grassId);
                        }
                    } else {
                        // plains / forest: grass-focused (90% grass, 10% fern)
                        if (rng.nextInt(10) < 9) { // Grass (90%)
                            // 15% tall grass, 85% short grass
                            if (rng.nextInt(100) < 15 && tallGrassId != 0) {
                                if (y + 2 < CHUNK_MAX_Y && chunk.getBlock(wx, y + 2, wz) == airId) {
                                    chunk.setBlock(wx, y + 1, wz, tallGrassId);
                                    chunk.setBlock(wx, y + 2, wz, tallGrassId);
                                } else if (grassId != 0) {
                                    chunk.setBlock(wx, y + 1, wz, grassId);
                                }
                            } else if (grassId != 0) {
                                chunk.setBlock(wx, y + 1, wz, grassId);
                            }
                        } else { // Fern (10%)
                            // 10% large fern, 90% fern
                            if (rng.nextInt(10) == 0 && largeFernId != 0) {
                                if (y + 2 < CHUNK_MAX_Y && chunk.getBlock(wx, y + 2, wz) == airId) {
                                    chunk.setBlock(wx, y + 1, wz, largeFernId);
                                    chunk.setBlock(wx, y + 2, wz, largeFernId);
                                } else if (fernId != 0) {
                                    chunk.setBlock(wx, y + 1, wz, fernId);
                                }
                            } else if (fernId != 0) {
                                chunk.setBlock(wx, y + 1, wz, fernId);
                            }
                        }
                    }
                }
                // Flowers
                else if (rng.nextInt(flowerChance) == 0) {
                    uint32_t flower = dandelionId;
                    int roll = rng.nextInt(10);
                    if (roll == 0 && dandelionId != 0) flower = dandelionId;
                    else if (roll == 1 && poppyId != 0) flower = poppyId;
                    else if (roll == 2 && alliumId != 0) flower = alliumId;
                    else if (roll == 3 && azureBluetId != 0) flower = azureBluetId;
                    else if (roll == 4 && oxeyeDaisyId != 0) flower = oxeyeDaisyId;
                    else if (roll == 5 && cornflowerId != 0) flower = cornflowerId;
                    else if (roll == 6 && lilyValleyId != 0) flower = lilyValleyId;
                    else if (roll >= 7) {
                        flower = tulips[rng.nextInt(tulips.size())];
                    }

                    if (flower != 0) {
                        chunk.setBlock(wx, y + 1, wz, flower);
                    }
                }
            }
        }
    }
}

} // namespace mc::levelgen::feature

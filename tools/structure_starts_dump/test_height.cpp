#include "world/level/levelgen/NoiseBasedChunkGenerator.h"
#include "world/level/block/Blocks.h"
#include <cstdio>
int main() {
    mc::initBlocks();
    mc::levelgen::NoiseBasedChunkGenerator gen(1ULL);
    // Ocean ruin at chunk (-9,-17), TPX=-150, TPZ=-272
    int h = gen.getOceanFloorHeight(-150, -272);
    int ws = gen.getBaseHeight(-150, -272);
    printf("(-150,-272): OCEAN_FLOOR=%d WORLD_SURFACE=%d\n", h, ws);
    // Also check the footprint corners
    return 0;
}

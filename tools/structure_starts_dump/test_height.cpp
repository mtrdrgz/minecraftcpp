#include "world/level/levelgen/NoiseBasedChunkGenerator.h"
#include "world/level/block/Blocks.h"
#include <cstdio>
int main() {
    mc::initBlocks();
    mc::levelgen::NoiseBasedChunkGenerator gen(1ULL);
    int chunks[][2] = {{-9,-17},{-13,1},{-12,22},{22,-18},{9,-16},{3,22},{43,-9},{69,-18}};
    for (auto& c : chunks) {
        int midX = c[0]*16+8, midZ = c[1]*16+8;
        int h = gen.getOceanFloorHeight(midX, midZ);
        int ws = gen.getBaseHeight(midX, midZ);
        printf("chunk=(%d,%d) midX=%d midZ=%d OCEAN_FLOOR=%d WORLD_SURFACE=%d\n", c[0], c[1], midX, midZ, h, ws);
    }
    return 0;
}

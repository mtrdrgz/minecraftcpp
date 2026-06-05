#include "StructureGen.h"

namespace mc::levelgen::structure {

void generateStructures(ChunkPos, uint64_t,
                        const StructureWorld&,
                        const std::function<std::string(int, int, int)>&) {
    // Intentionally disabled for strict 1:1 worldgen cleanup.
    //
    // The previous implementation placed hand-built approximations of desert
    // pyramids, swamp huts, igloos, ruined portals, a stronghold room and dungeons.
    // That is useful for visual prototyping, but it cannot produce seed-exact
    // parity with Minecraft. Structures must be restored by porting the vanilla
    // StructureSet / StructurePlacement / StructureStart / StructurePiece /
    // template / processor / jigsaw pipeline from 26.1.2 source and data.
}

} // namespace mc::levelgen::structure

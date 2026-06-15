#pragma once

// Deterministic, non-world-touching slice of
// net.minecraft.world.level.levelgen.structure.structures.NetherFossilPieces.
//
// Java source of truth (26.1.2):
//   NetherFossilPieces.FOSSILS = fossil_1 ... fossil_14
//   addPieces(...):
//     Rotation nextRotation = Rotation.getRandom(random);
//     Util.getRandom(FOSSILS, random)
//
// This file intentionally does NOT place blocks yet. It captures the exact RNG
// consumption order and selected template/rotation so the eventual runtime wiring
// can call into a small certified unit instead of inlining approximate logic in
// StructureGen.cpp.

#include "../templatesystem/StructureTransforms.h"
#include "../../RandomSource.h"

#include <array>
#include <cstddef>

namespace mc::levelgen::structure::structures {

struct NetherFossilPieceSelection {
    const char* templateId = "minecraft:nether_fossils/fossil_1";
    Rotation rotation = Rotation::NONE;
};

inline constexpr std::array<const char*, 14> kNetherFossilTemplates{
    "minecraft:nether_fossils/fossil_1",
    "minecraft:nether_fossils/fossil_2",
    "minecraft:nether_fossils/fossil_3",
    "minecraft:nether_fossils/fossil_4",
    "minecraft:nether_fossils/fossil_5",
    "minecraft:nether_fossils/fossil_6",
    "minecraft:nether_fossils/fossil_7",
    "minecraft:nether_fossils/fossil_8",
    "minecraft:nether_fossils/fossil_9",
    "minecraft:nether_fossils/fossil_10",
    "minecraft:nether_fossils/fossil_11",
    "minecraft:nether_fossils/fossil_12",
    "minecraft:nether_fossils/fossil_13",
    "minecraft:nether_fossils/fossil_14",
};

inline constexpr std::size_t netherFossilTemplateCount() noexcept {
    return kNetherFossilTemplates.size();
}

inline constexpr Rotation netherFossilRotationByJavaIndex(int index) noexcept {
    switch (index & 3) {
        case 1:  return Rotation::CLOCKWISE_90;
        case 2:  return Rotation::CLOCKWISE_180;
        case 3:  return Rotation::COUNTERCLOCKWISE_90;
        default: return Rotation::NONE;
    }
}

inline NetherFossilPieceSelection selectNetherFossilPiece(mc::levelgen::RandomSource& random) {
    // Rotation.getRandom(random) -> Util.getRandom(Rotation.values(), random).
    // Rotation enum declaration order is NONE, CLOCKWISE_90, CLOCKWISE_180,
    // COUNTERCLOCKWISE_90, matching StructureTransforms.h ordinals.
    const Rotation rotation = netherFossilRotationByJavaIndex(random.nextInt(4));

    // Util.getRandom(FOSSILS, random) consumes AFTER the rotation draw.
    const int templateIndex = random.nextInt(static_cast<int>(kNetherFossilTemplates.size()));
    return NetherFossilPieceSelection{kNetherFossilTemplates[static_cast<std::size_t>(templateIndex)], rotation};
}

} // namespace mc::levelgen::structure::structures

#pragma once

// Deterministic, non-world-touching slice of
// net.minecraft.world.level.levelgen.structure.structures.NetherFossilPieces.
//
// Java source of truth (26.1.2):
//   NetherFossilPieces.FOSSILS = fossil_1 ... fossil_14
//   addPieces(...):
//     Rotation nextRotation = Rotation.getRandom(random);
//     Util.getRandom(FOSSILS, random)
//   NetherFossilPiece.placeDriedGhast(...):
//     RandomSource.createThreadLocalInstance(level.getSeed())
//       .forkPositional().at(fossilBB.getCenter())
//     chance < 0.5F, random x/z within fossil BB, y=minY,
//     air + chunkBB gate, then dried_ghast rotated by Rotation.getRandom.
//
// This file intentionally does NOT own full template placement yet. It captures
// exact RNG consumption and deterministic selections so runtime wiring can call
// certified units instead of inlining approximate logic in StructureGen.cpp.

#include "../templatesystem/StructureTransforms.h"
#include "../../RandomSource.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace mc::levelgen::structure::structures {

struct NetherFossilPieceSelection {
    const char* templateId = "minecraft:nether_fossils/fossil_1";
    Rotation rotation = Rotation::NONE;
};

struct NetherFossilDriedGhastPlacement {
    BlockPos pos{};
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

inline constexpr BlockPos netherFossilBoxCenter(const BoundingBox& box) noexcept {
    // BoundingBox.getCenter(): min + (max - min + 1) / 2, component-wise.
    return {box.minX + (box.maxX - box.minX + 1) / 2,
            box.minY + (box.maxY - box.minY + 1) / 2,
            box.minZ + (box.maxZ - box.minZ + 1) / 2};
}

inline constexpr int netherFossilBoxXSpan(const BoundingBox& box) noexcept {
    return box.maxX - box.minX + 1;
}

inline constexpr int netherFossilBoxZSpan(const BoundingBox& box) noexcept {
    return box.maxZ - box.minZ + 1;
}

inline std::optional<NetherFossilDriedGhastPlacement> selectNetherFossilDriedGhastFromPositionalRandom(
    mc::levelgen::RandomSource& positionalRandom,
    const BoundingBox& fossilBB,
    const BoundingBox& chunkBB,
    bool selectedPositionIsAir) {
    // Java: if (positionalRandom.nextFloat() < 0.5F) { ... }
    if (!(positionalRandom.nextFloat() < 0.5F)) return std::nullopt;

    // Java: x=minX + nextInt(getXSpan()), y=minY, z=minZ + nextInt(getZSpan()).
    const BlockPos randomPos{
        fossilBB.minX + positionalRandom.nextInt(netherFossilBoxXSpan(fossilBB)),
        fossilBB.minY,
        fossilBB.minZ + positionalRandom.nextInt(netherFossilBoxZSpan(fossilBB))};

    // Java checks air and chunkBB.isInside BEFORE consuming Rotation.getRandom.
    if (!selectedPositionIsAir || !chunkBB.isInside(randomPos)) return std::nullopt;

    return NetherFossilDriedGhastPlacement{
        randomPos,
        netherFossilRotationByJavaIndex(positionalRandom.nextInt(4))};
}

inline std::optional<NetherFossilDriedGhastPlacement> selectNetherFossilDriedGhast(
    int64_t levelSeed,
    const BoundingBox& fossilBB,
    const BoundingBox& chunkBB,
    bool selectedPositionIsAir) {
    // Java: RandomSource.createThreadLocalInstance(level.getSeed()).forkPositional().at(fossilBB.getCenter()).
    std::shared_ptr<mc::levelgen::RandomSource> source = mc::levelgen::RandomSource::createThreadLocalInstance(levelSeed);
    std::shared_ptr<mc::levelgen::PositionalRandomFactory> positional = source->forkPositional();
    const BlockPos center = netherFossilBoxCenter(fossilBB);
    std::shared_ptr<mc::levelgen::RandomSource> random = positional->at(center.x, center.y, center.z);
    return selectNetherFossilDriedGhastFromPositionalRandom(*random, fossilBB, chunkBB, selectedPositionIsAir);
}

} // namespace mc::levelgen::structure::structures

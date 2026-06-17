#pragma once

// Deterministic slice of
// net.minecraft.world.level.levelgen.structure.structures.NetherFossilStructure.
//
// Java source of truth (26.1.2):
//   findGenerationPoint(context):
//     blockX = chunk.minBlockX + random.nextInt(16)
//     blockZ = chunk.minBlockZ + random.nextInt(16)
//     y = height.sample(random, generationContext)
//     while (y > seaLevel):
//       current = column.getBlock(y)
//       below = column.getBlock(--y)
//       if current.isAir && (below.is(SOUL_SAND) || below.isFaceSturdy(UP)) break
//     if y <= seaLevel -> empty
//     position = BlockPos(blockX, y, blockZ)
//     NetherFossilPieces.addPieces(..., random, position)
//
// This header stays world-agnostic: callers provide the column/block predicates.
// It deliberately consumes RNG in the same order as the Java structure before
// returning the selected fossil template/rotation.

#include "NetherFossilPieces.h"

#include <cstdint>
#include <functional>
#include <optional>

namespace mc::levelgen::structure::structures {

enum class NetherFossilHeightKind { Constant, Uniform };

struct NetherFossilHeightProvider {
    NetherFossilHeightKind kind = NetherFossilHeightKind::Constant;
    int minInclusive = 0;
    int maxInclusive = 0;

    static constexpr NetherFossilHeightProvider constant(int value) noexcept {
        return NetherFossilHeightProvider{NetherFossilHeightKind::Constant, value, value};
    }

    static constexpr NetherFossilHeightProvider uniform(int minY, int maxY) noexcept {
        return NetherFossilHeightProvider{NetherFossilHeightKind::Uniform, minY, maxY};
    }

    int sample(mc::levelgen::RandomSource& random) const {
        if (kind == NetherFossilHeightKind::Uniform && maxInclusive >= minInclusive) {
            return minInclusive + random.nextInt(maxInclusive - minInclusive + 1);
        }
        return minInclusive;
    }
};

struct NetherFossilGenerationConfig {
    NetherFossilHeightProvider height = NetherFossilHeightProvider::constant(0);
    int seaLevel = 0;
};

struct NetherFossilColumnView {
    std::function<bool(int, int, int)> isAir;
    std::function<bool(int, int, int)> isSoulSand;
    std::function<bool(int, int, int)> isFaceSturdyUp;
};

struct NetherFossilGenerationPoint {
    BlockPos position{};
    NetherFossilPieceSelection piece{};
};

inline bool netherFossilColumnSupportsStart(const NetherFossilColumnView& column, int x, int y, int z) {
    const bool soulSand = column.isSoulSand ? column.isSoulSand(x, y, z) : false;
    if (soulSand) return true;
    return column.isFaceSturdyUp ? column.isFaceSturdyUp(x, y, z) : false;
}

inline std::optional<NetherFossilGenerationPoint> findNetherFossilGenerationPoint(
    int chunkX,
    int chunkZ,
    const NetherFossilGenerationConfig& cfg,
    const NetherFossilColumnView& column,
    mc::levelgen::RandomSource& random) {
    const int blockX = chunkX * 16 + random.nextInt(16);
    const int blockZ = chunkZ * 16 + random.nextInt(16);
    int y = cfg.height.sample(random);

    while (y > cfg.seaLevel) {
        const bool currentIsAir = column.isAir ? column.isAir(blockX, y, blockZ) : false;
        --y;
        if (currentIsAir && netherFossilColumnSupportsStart(column, blockX, y, blockZ)) {
            break;
        }
    }

    if (y <= cfg.seaLevel) return std::nullopt;

    NetherFossilGenerationPoint out;
    out.position = {blockX, y, blockZ};
    out.piece = selectNetherFossilPiece(random);
    return out;
}

} // namespace mc::levelgen::structure::structures

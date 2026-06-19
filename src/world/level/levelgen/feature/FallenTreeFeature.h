#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.FallenTreeFeature with the
// decorators its configured features use (fallen_oak_tree / fallen_birch_tree):
//   TrunkVineDecorator      (stump_decorators of fallen_oak_tree)
//   AttachedToLogsDecorator (log_decorators: 0.1 up-facing red/brown mushrooms)
//
// RNG order (FallenTreeFeature.java:36-45 placeFallenTree):
//   placeStump: placeLogBlock (simple provider: no draw) + markAboveForPostProcessing;
//     stump decorators run on Set.of(stump):
//       trunk_vine (TrunkVineDecorator.java:18-49): 4x nextInt(3) per log (w/e/n/s),
//       each >0 arm gated by isAir before placeVine
//   direction  = Direction.Plane.HORIZONTAL.getRandomDirection(random)   (nextInt(4),
//                faces order NORTH,EAST,SOUTH,WEST — Direction.java Plane.HORIZONTAL)
//   logLength  = config.logLength.sample(random) - 2                     (uniform: 1 draw)
//   logStartPos= origin.relative(direction, 2 + random.nextInt(2))       (1 draw)
//   setGroundHeightForFallenLogStartPos / canPlaceEntireFallenLog: world reads only
//   placeFallenLog: per log placeLogBlock (no draw) + markAbove; then log decorators:
//     attached_to_logs (AttachedToLogsDecorator.java:34-46): Util.shuffledCopy of the
//     logs list (n-1 nextInt draws over the Y-sorted, HashSet-ordered Context list),
//     then per log: Util.getRandom(directions) = nextInt(size) (1 draw even for a
//     single direction), nextFloat() <= probability && isAir gate, provider getState
//     (weighted: nextInt(totalWeight)) ONLY on success.
//
// The fallen log positions share one Y, so the Context sort by Y keeps the raw
// java.util.HashSet iteration order — replicated via JavaBlockPosHashSet.

#include "TreeFeature.h"

namespace mc::levelgen::feature {

struct FallenTreeDecoratorConfig {
    enum class Kind { TrunkVine, AttachedToLogs };
    Kind kind = Kind::TrunkVine;
    float probability = 0.0f;                 // attached_to_logs
    DiskStateProvider provider;               // attached_to_logs block_provider
    std::vector<int> directions;              // attached_to_logs directions (Direction indices)
};

struct FallenTreeConfig {
    DiskStateProvider trunkProvider;
    mc::valueproviders::IntProviderPtr logLength;
    std::vector<FallenTreeDecoratorConfig> stumpDecorators;
    std::vector<FallenTreeDecoratorConfig> logDecorators;
};

namespace fallen_detail {

// Feature.markAboveForPostProcessing (Feature.java:206-217) with the real
// BlockStateBase.isAir (cave_air counts).
inline void markAboveForPostProcessing(const TreeHooks& hooks, WorldGenLevel& level, BlockPos placePos) {
    BlockPos pos = placePos;
    for (int i = 0; i < 2; ++i) {
        pos = BlockPos{ pos.x, pos.y + 1, pos.z };
        if (hooks.isAir(level.getBlockState(pos))) {
            return;
        }
        level.markPosForPostprocessing(pos);
    }
}

// FallenTreeFeature.isOverSolidGround (FallenTreeFeature.java:111-113):
// level.getBlockState(pos.below()).isFaceSturdy(level, pos, UP) — supplied by the
// harness as a state predicate (full-cube table).
// FallenTreeFeature.placeLogBlock (FallenTreeFeature.java:115-125); the sideways
// AXIS modifier is id-invisible.
inline BlockPos placeLogBlock(const FallenTreeConfig& config, const TreeHooks& hooks,
                              WorldGenLevel& level, RandomSource& random, BlockPos pos) {
    level.setBlock(pos, config.trunkProvider(level, random, pos).value(), 3);
    markAboveForPostProcessing(hooks, level, pos);
    return pos;
}

// TrunkVineDecorator.place (TrunkVineDecorator.java:18-49). placeVine sets the
// VINE state with one face property (EAST for the west neighbour, etc.); the
// face goes into the harness side map when the write lands.
inline void placeTrunkVine(TreeDecoratorContext& ctx) {
    RandomSource& random = *ctx.random;
    auto placeVine = [&](BlockPos at, int faceDir) {
        if (ctx.isAir(at) && ctx.setBlock(at, "minecraft:vine")) {
            ctx.hooks->putVineFace(at, faceDir);
        }
    };
    for (const BlockPos& pos : ctx.logs) {
        if (random.nextInt(3) > 0) {
            placeVine(BlockPos{ pos.x - 1, pos.y, pos.z }, 5);   // west neighbour, VineBlock.EAST
        }
        if (random.nextInt(3) > 0) {
            placeVine(BlockPos{ pos.x + 1, pos.y, pos.z }, 4);   // east neighbour, VineBlock.WEST
        }
        if (random.nextInt(3) > 0) {
            placeVine(BlockPos{ pos.x, pos.y, pos.z - 1 }, 3);   // north neighbour, VineBlock.SOUTH
        }
        if (random.nextInt(3) > 0) {
            placeVine(BlockPos{ pos.x, pos.y, pos.z + 1 }, 2);   // south neighbour, VineBlock.NORTH
        }
    }
}

// AttachedToLogsDecorator.place (AttachedToLogsDecorator.java:34-46).
inline void placeAttachedToLogs(TreeDecoratorContext& ctx, const FallenTreeDecoratorConfig& cfg) {
    RandomSource& random = *ctx.random;
    std::vector<BlockPos> shuffled = ctx.logs;     // Util.shuffledCopy
    javaShuffle(shuffled, random);
    for (const BlockPos& logsPos : shuffled) {
        const int direction = cfg.directions[static_cast<std::size_t>(
            random.nextInt(static_cast<std::int32_t>(cfg.directions.size())))];   // Util.getRandom
        const BlockPos placementPos = treeRelative(logsPos, direction);
        if (random.nextFloat() <= cfg.probability && ctx.isAir(placementPos)) {
            ctx.setBlock(placementPos, cfg.provider(*ctx.level, random, placementPos).value());
        }
    }
}

// FallenTreeFeature.decorateLogs (FallenTreeFeature.java:127-136): a fresh
// TreeDecorator.Context over (logs, {}, {}) whose decoration setter is
// setBlock(pos, state, 19) without decoration tracking.
inline void decorateLogs(const TreeHooks& hooks, WorldGenLevel& level, RandomSource& random,
                         const JavaBlockPosHashSet& logs,
                         const std::vector<FallenTreeDecoratorConfig>& decorators) {
    if (decorators.empty()) return;
    TreeDecoratorContext ctx;
    ctx.level = &level;
    ctx.setBlock = [&level](BlockPos pos, const std::string& state) {
        return level.setBlockChecked(pos, state, 19);
    };
    ctx.random = &random;
    ctx.logs = sortedByYJavaOrder(logs);
    ctx.hooks = &hooks;
    for (const FallenTreeDecoratorConfig& dec : decorators) {
        if (dec.kind == FallenTreeDecoratorConfig::Kind::TrunkVine) placeTrunkVine(ctx);
        else placeAttachedToLogs(ctx, dec);
    }
}

} // namespace fallen_detail

// FallenTreeFeature.place (FallenTreeFeature.java:30-105). Always returns true.
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeFallenTreePlacer(
        std::shared_ptr<const FallenTreeConfig> config, std::shared_ptr<const TreeHooks> hooks,
        std::function<bool(const std::string&)> isFaceSturdyUpState) {
    return [config = std::move(config), hooks = std::move(hooks),
            isFaceSturdyUpState = std::move(isFaceSturdyUpState)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        using namespace fallen_detail;
        auto isOverSolidGround = [&](BlockPos pos) {
            return isFaceSturdyUpState(level.getBlockState(BlockPos{ pos.x, pos.y - 1, pos.z }));
        };
        auto mayPlaceOn = [&](BlockPos pos) {
            return hooks->validTreePosState(level.getBlockState(pos)) && isOverSolidGround(pos);
        };

        // placeStump (FallenTreeFeature.java:59-62)
        {
            JavaBlockPosHashSet stump;
            stump.add(placeLogBlock(*config, *hooks, level, random, origin));
            decorateLogs(*hooks, level, random, stump, config->stumpDecorators);
        }
        // Direction.Plane.HORIZONTAL.getRandomDirection: faces NORTH,EAST,SOUTH,WEST
        static constexpr int horizontal[4] = { 2, 5, 3, 4 };   // N, E, S, W as Direction indices
        const int direction = horizontal[random.nextInt(4)];
        const int logLength = config->logLength->sample(random) - 2;
        BlockPos logStartPos = treeRelative(treeRelative(origin, direction), direction);   // relative(direction, 2)
        {
            const int extra = random.nextInt(2);
            for (int i = 0; i < extra; ++i) logStartPos = treeRelative(logStartPos, direction);
        }
        // setGroundHeightForFallenLogStartPos (FallenTreeFeature.java:47-57)
        logStartPos = BlockPos{ logStartPos.x, logStartPos.y + 1, logStartPos.z };
        for (int i = 0; i < 6; ++i) {
            if (mayPlaceOn(logStartPos)) break;
            logStartPos = BlockPos{ logStartPos.x, logStartPos.y - 1, logStartPos.z };
        }
        // canPlaceEntireFallenLog (FallenTreeFeature.java:64-87)
        bool canPlace = true;
        {
            BlockPos scan = logStartPos;
            int gapInGround = 0;
            for (int i = 0; i < logLength; ++i) {
                if (!hooks->validTreePosState(level.getBlockState(scan))) { canPlace = false; break; }
                if (!isOverSolidGround(scan)) {
                    if (++gapInGround > 2) { canPlace = false; break; }
                } else {
                    gapInGround = 0;
                }
                scan = treeRelative(scan, direction);
            }
        }
        if (canPlace) {
            // placeFallenLog (FallenTreeFeature.java:89-105)
            JavaBlockPosHashSet fallenLog;
            BlockPos cursor = logStartPos;
            for (int i = 0; i < logLength; ++i) {
                fallenLog.add(placeLogBlock(*config, *hooks, level, random, cursor));
                cursor = treeRelative(cursor, direction);
            }
            decorateLogs(*hooks, level, random, fallenLog, config->logDecorators);
        }
        return true;   // FallenTreeFeature.place returns true unconditionally
    };
}

} // namespace mc::levelgen::feature

#pragma once

// Port of net.minecraft.world.level.levelgen.feature.BlockColumnFeature: places a
// vertical column of blocks (e.g. sugar cane, cactus) in `direction`, sized by
// per-layer IntProvider heights, truncated to the space the `allowedPlacement`
// predicate permits. Does not call canSurvive — placement validity is gated by
// the placed-feature's modifiers + the allowedPlacement predicate.

#include "../IntProvider.h"
#include "../RandomSource.h"
#include "../placement/PlacementContext.h"
#include "stateproviders/BlockStateProvider.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;
using mc::valueproviders::IntProviderPtr;
using mc::levelgen::feature::stateproviders::BlockStateProviderPtr;

struct BlockColumnConfiguration {
    struct Layer {
        IntProviderPtr height;
        BlockStateProviderPtr state;
    };
    std::vector<Layer> layers;
    BlockPos direction{ 0, 1, 0 }; // "up"
    std::function<bool(WorldGenLevel&, BlockPos)> allowedPlacement;
    bool prioritizeTip = false;
};

class BlockColumnFeature {
public:
    bool place(WorldGenLevel& level, RandomSource& random, BlockPos origin, const BlockColumnConfiguration& config) const {
        const int layerCount = static_cast<int>(config.layers.size());
        std::vector<int> layerHeights(static_cast<std::size_t>(layerCount));
        int totalHeight = 0;
        for (int i = 0; i < layerCount; ++i) {
            layerHeights[i] = config.layers[i].height->sample(random);
            totalHeight += layerHeights[i];
        }
        if (totalHeight == 0) {
            return false;
        }

        BlockPos placePos = origin;
        BlockPos nextPos = add(origin, config.direction);
        for (int y = 0; y < totalHeight; ++y) {
            if (!config.allowedPlacement(level, nextPos)) {
                truncate(layerHeights, totalHeight, y, config.prioritizeTip);
                break;
            }
            nextPos = add(nextPos, config.direction);
        }

        for (int i = 0; i < layerCount; ++i) {
            const int count = layerHeights[i];
            if (count != 0) {
                const BlockColumnConfiguration::Layer& layer = config.layers[i];
                for (int y = 0; y < count; ++y) {
                    level.setBlock(placePos, layer.state->getState(random, placePos), 2);
                    placePos = add(placePos, config.direction);
                }
            }
        }
        return true;
    }

private:
    static BlockPos add(BlockPos a, BlockPos d) { return BlockPos{ a.x + d.x, a.y + d.y, a.z + d.z }; }

    static void truncate(std::vector<int>& layerHeights, int totalHeight, int newHeight, bool prioritizeTip) {
        int amountToRemove = totalHeight - newHeight;
        const int direction = prioritizeTip ? 1 : -1;
        const int start = prioritizeTip ? 0 : static_cast<int>(layerHeights.size()) - 1;
        const int end = prioritizeTip ? static_cast<int>(layerHeights.size()) : -1;
        for (int i = start; i != end && amountToRemove > 0; i += direction) {
            const int toRemove = std::min(layerHeights[i], amountToRemove);
            amountToRemove -= toRemove;
            layerHeights[i] -= toRemove;
        }
    }
};

// ---------------------------------------------------------------------------
// Harness-style factory over level-aware providers (BlockStateProvider.getState
// takes the WorldGenLevel in vanilla — BlockColumnFeature.java:50). Same 1:1
// algorithm as the class above (BlockColumnFeature.java:14-71):
//   RNG: one height.sample per layer (biased_to_bottom / weighted_list draws);
//   totalHeight == 0 -> false; the allowedPlacement scan and the writes draw
//   nothing (simple providers).
struct BlockColumnLayerFn {
    IntProviderPtr height;
    std::function<std::optional<std::string>(WorldGenLevel&, RandomSource&, BlockPos)> provider;
};

inline std::function<bool(WorldGenLevel&, RandomSource&, BlockPos)> makeBlockColumnPlacer(
        std::vector<BlockColumnLayerFn> layers, BlockPos direction, bool prioritizeTip,
        std::function<bool(WorldGenLevel&, BlockPos)> allowedPlacement) {
    return [layers = std::move(layers), direction, prioritizeTip,
            allowedPlacement = std::move(allowedPlacement)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        const int layerCount = static_cast<int>(layers.size());
        std::vector<int> layerHeights(static_cast<std::size_t>(layerCount));
        int totalHeight = 0;
        for (int i = 0; i < layerCount; ++i) {
            layerHeights[static_cast<std::size_t>(i)] = layers[static_cast<std::size_t>(i)].height->sample(random);
            totalHeight += layerHeights[static_cast<std::size_t>(i)];
        }
        if (totalHeight == 0) return false;

        auto add = [](BlockPos a, BlockPos d) { return BlockPos{ a.x + d.x, a.y + d.y, a.z + d.z }; };
        const bool dbg = std::getenv("MCPP_BLOCKCOL_DEBUG") != nullptr;
        if (dbg) {
            fprintf(stderr, "BLOCKCOL origin=%d,%d,%d total=%d heights=", origin.x, origin.y, origin.z, totalHeight);
            for (int h : layerHeights) fprintf(stderr, "%d,", h);
            fprintf(stderr, "\n");
        }
        BlockPos placePos = origin;
        BlockPos nextPos = add(origin, direction);
        for (int y = 0; y < totalHeight; ++y) {
            if (!allowedPlacement(level, nextPos)) {
                if (dbg) fprintf(stderr, "BLOCKCOL truncate at y=%d nextPos=%d,%d,%d state=%s\n",
                                 y, nextPos.x, nextPos.y, nextPos.z, level.getBlockState(nextPos).c_str());
                // BlockColumnFeature.truncate (:59-71)
                int amountToRemove = totalHeight - y;
                const int dir = prioritizeTip ? 1 : -1;
                const int start = prioritizeTip ? 0 : layerCount - 1;
                const int end = prioritizeTip ? layerCount : -1;
                for (int i = start; i != end && amountToRemove > 0; i += dir) {
                    const int toRemove = std::min(layerHeights[static_cast<std::size_t>(i)], amountToRemove);
                    amountToRemove -= toRemove;
                    layerHeights[static_cast<std::size_t>(i)] -= toRemove;
                }
                break;
            }
            nextPos = add(nextPos, direction);
        }

        for (int i = 0; i < layerCount; ++i) {
            const int count = layerHeights[static_cast<std::size_t>(i)];
            if (count != 0) {
                const BlockColumnLayerFn& layer = layers[static_cast<std::size_t>(i)];
                for (int y = 0; y < count; ++y) {
                    level.setBlock(placePos, layer.provider(level, random, placePos).value(), 2);
                    placePos = add(placePos, direction);
                }
            }
        }
        return true;
    };
}

} // namespace mc::levelgen::feature

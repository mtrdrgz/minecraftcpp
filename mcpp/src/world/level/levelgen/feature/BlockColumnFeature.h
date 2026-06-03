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

} // namespace mc::levelgen::feature

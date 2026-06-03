#pragma once

// Port of net.minecraft.world.level.levelgen.feature.Feature / FeaturePlaceContext
// and SimpleBlockFeature (the core single-block vegetation feature: grass,
// flowers, ferns, ...). canSurvive() is delegated to WorldGenLevel (the
// block-behaviour boundary). Double-plant / mossy-carpet special-casing and
// scheduleTick are deferred until those block behaviours are ported.

#include "../RandomSource.h"
#include "../placement/PlacementContext.h"
#include "stateproviders/BlockStateProvider.h"
#include "../../block/BlockBehaviour.h"

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;
using mc::levelgen::feature::stateproviders::BlockStateProviderPtr;

template <typename FC>
struct FeaturePlaceContext {
    WorldGenLevel* level;
    RandomSource* random;
    BlockPos origin;
    const FC* config;
};

struct SimpleBlockConfiguration {
    BlockStateProviderPtr toPlace;
    bool scheduleTick = false;
};

class SimpleBlockFeature {
public:
    bool place(const FeaturePlaceContext<SimpleBlockConfiguration>& context) const {
        const SimpleBlockConfiguration& config = *context.config;
        WorldGenLevel& level = *context.level;
        const BlockPos origin = context.origin;
        const std::string state = config.toPlace->getState(*context.random, origin); // getOptionalState (never null here)

        if (level.canSurvive(state, origin)) {
            if (mc::block::isDoublePlant(state)) {
                // DoublePlantBlock: needs space above, then placeAt sets the lower
                // half here and the upper half above (NOTE: MossyCarpetBlock and
                // scheduleTick branches are still deferred).
                const BlockPos above{ origin.x, origin.y + 1, origin.z };
                if (!level.isEmptyBlock(above)) {
                    return false;
                }
                level.setBlock(origin, mc::block::setProperty(state, "half", "lower"), 2);
                level.setBlock(above, mc::block::setProperty(state, "half", "upper"), 2);
            } else {
                level.setBlock(origin, state, 2);
            }
            return true;
        }
        return false;
    }
};

} // namespace mc::levelgen::feature

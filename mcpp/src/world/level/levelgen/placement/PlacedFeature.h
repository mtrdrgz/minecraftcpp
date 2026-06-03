#pragma once

// Port of net.minecraft.world.level.levelgen.placement.PlacedFeature.place: chain
// the placement modifiers over the position stream (Stream.of(origin) flatMapped
// through each modifier), then run the configured feature at every resulting
// position. Plus two small modifiers used by vegetation chains:
//   BiomeFilter           - keeps the position if it's in a matching biome
//                           (single-biome generation: a pass-through predicate)
//   BlockPredicateFilter  - keeps the position if a BlockPredicate holds
// modelled with std::function so the world/biome/predicate backings stay
// decoupled from this composition.

#include "../RandomSource.h"
#include "PlacementContext.h"
#include "PlacementModifier.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace mc::levelgen::placement {

// minecraft:biome — keeps origin if the biome at the position matches the
// feature's biomes. Backed by a predicate (single-biome generation => always true).
class BiomeFilter final : public PlacementModifier {
public:
    using Predicate = std::function<bool(WorldGenLevel&, BlockPos)>;
    explicit BiomeFilter(Predicate pred) : m_pred(std::move(pred)) {}
    std::vector<BlockPos> getPositions(PlacementContext* ctx, RandomSource&, BlockPos origin) const override {
        return m_pred(*ctx->getLevel(), origin) ? std::vector<BlockPos>{ origin } : std::vector<BlockPos>{};
    }

private:
    Predicate m_pred;
};

// minecraft:block_predicate_filter — keeps origin if predicate(level, pos) holds.
class BlockPredicateFilter final : public PlacementModifier {
public:
    using Predicate = std::function<bool(WorldGenLevel&, BlockPos)>;
    explicit BlockPredicateFilter(Predicate pred) : m_pred(std::move(pred)) {}
    std::vector<BlockPos> getPositions(PlacementContext* ctx, RandomSource&, BlockPos origin) const override {
        return m_pred(*ctx->getLevel(), origin) ? std::vector<BlockPos>{ origin } : std::vector<BlockPos>{};
    }

private:
    Predicate m_pred;
};

class PlacedFeature {
public:
    using FeaturePlacer = std::function<bool(WorldGenLevel&, RandomSource&, BlockPos)>;

    PlacedFeature(FeaturePlacer feature, std::vector<std::shared_ptr<const PlacementModifier>> placement)
        : m_feature(std::move(feature)), m_placement(std::move(placement)) {}

    bool place(WorldGenLevel& level, RandomSource& random, BlockPos origin, int minGenY, int genDepth) const {
        PlacementContext context(&level, minGenY, genDepth);
        std::vector<BlockPos> positions{ origin };
        for (const auto& modifier : m_placement) {
            std::vector<BlockPos> next;
            for (const BlockPos& p : positions) {
                std::vector<BlockPos> produced = modifier->getPositions(&context, random, p);
                next.insert(next.end(), produced.begin(), produced.end());
            }
            positions.swap(next);
        }
        bool placedAny = false;
        for (const BlockPos& pos : positions) {
            if (m_feature(level, random, pos)) {
                placedAny = true;
            }
        }
        return placedAny;
    }

private:
    FeaturePlacer m_feature;
    std::vector<std::shared_ptr<const PlacementModifier>> m_placement;
};

} // namespace mc::levelgen::placement

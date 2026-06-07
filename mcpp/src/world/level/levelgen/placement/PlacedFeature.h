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
        // Java composes the modifiers as lazy Stream.flatMap chains terminated by
        // forEach(feature). Laziness makes evaluation DEPTH-FIRST: modifier[i]
        // produces its list for one upstream position, then each produced position
        // is driven through modifiers[i+1..] and the feature before the next
        // upstream position is processed. This ordering is RNG-observable (ores,
        // for instance, consume RNG in in_square + height_range per position and
        // then in the feature), so it must match Java exactly — a breadth-first
        // pass over the whole vector would consume RNG in the wrong order.
        bool placedAny = false;
        const std::size_t modCount = m_placement.size();
        std::function<void(std::size_t, const BlockPos&)> drive =
            [&](std::size_t i, const BlockPos& pos) {
                if (i == modCount) {
                    if (m_feature(level, random, pos)) placedAny = true;
                    return;
                }
                std::vector<BlockPos> produced = m_placement[i]->getPositions(&context, random, pos);
                for (const BlockPos& p : produced) drive(i + 1, p);
            };
        drive(0, origin);
        return placedAny;
    }

private:
    FeaturePlacer m_feature;
    std::vector<std::shared_ptr<const PlacementModifier>> m_placement;
};

} // namespace mc::levelgen::placement

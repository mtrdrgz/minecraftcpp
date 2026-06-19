#pragma once

// Port of net.minecraft.world.level.levelgen.feature.stateproviders.*
// (the BlockStateProvider hierarchy). A BlockState is represented by its
// canonical serialized id string (e.g. "minecraft:short_grass" or
// "minecraft:x[prop=val,...]") — sufficient for the placement logic and for
// 1:1 verification of which state is chosen. None of these providers read the
// level, so getState takes only (random, pos).

#include "../../RandomSource.h"
#include "../../../../../core/Math.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mc::levelgen::feature::stateproviders {

using mc::BlockPos;
using mc::levelgen::RandomSource;

using BlockState = std::string; // canonical serialized block-state id

class BlockStateProvider {
public:
    virtual ~BlockStateProvider() = default;
    virtual BlockState getState(RandomSource& random, BlockPos pos) const = 0;
};

using BlockStateProviderPtr = std::shared_ptr<const BlockStateProvider>;

class SimpleStateProvider final : public BlockStateProvider {
public:
    explicit SimpleStateProvider(BlockState state) : m_state(std::move(state)) {}
    static BlockStateProviderPtr of(BlockState state) { return std::make_shared<SimpleStateProvider>(std::move(state)); }
    BlockState getState(RandomSource&, BlockPos) const override { return m_state; }

private:
    BlockState m_state;
};

class WeightedStateProvider final : public BlockStateProvider {
public:
    struct Entry {
        BlockState state;
        int32_t weight;
    };

    explicit WeightedStateProvider(std::vector<Entry> entries) : m_entries(std::move(entries)) {
        m_totalWeight = 0;
        for (const auto& e : m_entries) m_totalWeight += e.weight;
    }

    // WeightedList.getRandomOrThrow: nextInt(totalWeight) then cumulative walk.
    BlockState getState(RandomSource& random, BlockPos) const override {
        int32_t selection = random.nextInt(m_totalWeight);
        for (const auto& e : m_entries) {
            selection -= e.weight;
            if (selection < 0) return e.state;
        }
        throw std::runtime_error("WeightedStateProvider: selection exceeded total weight");
    }

private:
    std::vector<Entry> m_entries;
    int32_t m_totalWeight;
};

} // namespace mc::levelgen::feature::stateproviders

#pragma once

// Port of net.minecraft.world.level.levelgen.placement.PlacementModifier and the
// world-independent ("pure") modifiers. These compute *where* a placed feature
// is attempted from an origin + RandomSource, with no world queries. The
// world-dependent modifiers (heightmap, height_range, *_filter, environment_scan,
// noise_*_count, ...) are added with the PlacementContext / WorldGenLevel surface.

#include "../RandomSource.h"
#include "../IntProvider.h"
#include "../../../../core/Math.h"

#include <memory>
#include <utility>
#include <vector>

namespace mc::levelgen::placement {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::valueproviders::IntProviderPtr;

// World query surface used by world-dependent placement modifiers. Pure
// modifiers ignore it; fleshed out when those modifiers are ported.
class PlacementContext;

class PlacementModifier {
public:
    virtual ~PlacementModifier() = default;
    virtual std::vector<BlockPos> getPositions(PlacementContext* context, RandomSource& random, BlockPos origin) const = 0;
};

// random.nextInt(16) for x and z, keeping origin.y.
class InSquarePlacement final : public PlacementModifier {
public:
    std::vector<BlockPos> getPositions(PlacementContext*, RandomSource& random, BlockPos origin) const override {
        const int32_t x = random.nextInt(16) + origin.x;
        const int32_t z = random.nextInt(16) + origin.z;
        return { BlockPos{ x, origin.y, z } };
    }
};

// RepeatingPlacement/CountPlacement: emit `origin` count.sample(random) times.
class CountPlacement final : public PlacementModifier {
public:
    explicit CountPlacement(IntProviderPtr count) : m_count(std::move(count)) {}
    std::vector<BlockPos> getPositions(PlacementContext*, RandomSource& random, BlockPos origin) const override {
        const int32_t n = m_count->sample(random);
        return std::vector<BlockPos>(n < 0 ? 0 : static_cast<std::size_t>(n), origin);
    }

private:
    IntProviderPtr m_count;
};

// PlacementFilter + RarityFilter: keep origin with probability 1/chance.
class RarityFilter final : public PlacementModifier {
public:
    explicit RarityFilter(int32_t chance) : m_chance(chance) {}
    std::vector<BlockPos> getPositions(PlacementContext*, RandomSource& random, BlockPos origin) const override {
        if (random.nextFloat() < 1.0F / static_cast<float>(m_chance)) {
            return { origin };
        }
        return {};
    }

private:
    int32_t m_chance;
};

// xz/y scatter; note xzSpread is sampled separately for X and Z (two draws).
class RandomOffsetPlacement final : public PlacementModifier {
public:
    RandomOffsetPlacement(IntProviderPtr xzSpread, IntProviderPtr ySpread)
        : m_xzSpread(std::move(xzSpread)), m_ySpread(std::move(ySpread)) {}
    std::vector<BlockPos> getPositions(PlacementContext*, RandomSource& random, BlockPos origin) const override {
        const int32_t x = origin.x + m_xzSpread->sample(random);
        const int32_t y = origin.y + m_ySpread->sample(random);
        const int32_t z = origin.z + m_xzSpread->sample(random);
        return { BlockPos{ x, y, z } };
    }

private:
    IntProviderPtr m_xzSpread, m_ySpread;
};

// FixedPlacement (FixedPlacement.java): returns the fixed positions that fall
// in the origin's chunk. No RNG draws.
class FixedPlacement final : public PlacementModifier {
public:
    explicit FixedPlacement(std::vector<BlockPos> positions) : m_positions(std::move(positions)) {}
    std::vector<BlockPos> getPositions(PlacementContext* /*ctx*/, RandomSource&, BlockPos origin) const override {
        const int chunkX = origin.x >> 4;
        const int chunkZ = origin.z >> 4;
        std::vector<BlockPos> out;
        for (const BlockPos& pos : m_positions) {
            if ((pos.x >> 4) == chunkX && (pos.z >> 4) == chunkZ) {
                out.push_back(pos);
            }
        }
        return out;
    }

private:
    std::vector<BlockPos> m_positions;
};

} // namespace mc::levelgen::placement

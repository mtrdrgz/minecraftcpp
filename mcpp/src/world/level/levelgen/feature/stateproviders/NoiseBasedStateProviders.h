#pragma once

// Port of the noise-based BlockStateProviders (NoiseThresholdProvider, ...),
// used by e.g. flower_plain. The noise is a NormalNoise built from
// WorldgenRandom(LegacyRandomSource(seed)) exactly as Java's
// NoiseBasedStateProvider does.
//
// UNVERIFIED: this is a faithful structural port, but the underlying C++
// NormalNoise does NOT yet match Java bit-for-bit (found via the noise provider
// work — see AGENTS.md "NormalNoise parity gap"). So the noise-driven branch of
// getState is not yet certified 1:1. The RNG list-picking around it is correct.

#include "../../Noise.h"
#include "../../RandomSource.h"
#include "BlockStateProvider.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace mc::levelgen::feature::stateproviders {

using mc::levelgen::NormalNoise;
using mc::levelgen::NoiseParameters;

// Base: builds the NormalNoise from the seed (NoiseBasedStateProvider).
class NoiseBasedStateProvider : public BlockStateProvider {
public:
    NoiseBasedStateProvider(std::int64_t seed, NoiseParameters parameters, float scale) : m_scale(scale) {
        auto source = std::make_shared<mc::levelgen::LegacyRandomSource>(seed);
        mc::levelgen::WorldgenRandom random(source);
        m_noise = NormalNoise::create(random, parameters);
    }

protected:
    // getNoiseValue(pos, this.scale): this.scale is a float widened to double.
    double getNoiseValue(BlockPos pos, double scale) const {
        return m_noise.getValue(static_cast<double>(pos.x) * scale, static_cast<double>(pos.y) * scale,
                                static_cast<double>(pos.z) * scale);
    }

    NormalNoise m_noise;
    float m_scale;
};

class NoiseThresholdProvider final : public NoiseBasedStateProvider {
public:
    NoiseThresholdProvider(std::int64_t seed, NoiseParameters parameters, float scale, float threshold, float highChance,
                           BlockState defaultState, std::vector<BlockState> lowStates, std::vector<BlockState> highStates)
        : NoiseBasedStateProvider(seed, std::move(parameters), scale),
          m_threshold(threshold), m_highChance(highChance), m_default(std::move(defaultState)),
          m_lowStates(std::move(lowStates)), m_highStates(std::move(highStates)) {}

    BlockState getState(RandomSource& random, BlockPos pos) const override {
        const double localValue = getNoiseValue(pos, static_cast<double>(m_scale));
        if (localValue < static_cast<double>(m_threshold)) {
            return m_lowStates[random.nextInt(static_cast<int>(m_lowStates.size()))];
        }
        return random.nextFloat() < m_highChance
                   ? m_highStates[random.nextInt(static_cast<int>(m_highStates.size()))]
                   : m_default;
    }

private:
    float m_threshold;
    float m_highChance;
    BlockState m_default;
    std::vector<BlockState> m_lowStates;
    std::vector<BlockState> m_highStates;
};

} // namespace mc::levelgen::feature::stateproviders

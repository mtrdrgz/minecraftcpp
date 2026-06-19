#pragma once

// Ports of the noise-count placement modifiers (RepeatingPlacement subclasses)
// that vegetation uses to vary density: NoiseThresholdCountPlacement and
// NoiseBasedCountPlacement. Both sample Biome.BIOME_INFO_NOISE (a fixed-seed
// PerlinSimplexNoise) by world XZ — no RandomSource use.

#include "../Noise.h"
#include "../RandomSource.h"
#include "PlacementModifier.h"

#include <cmath>
#include <memory>
#include <vector>

namespace mc::levelgen::placement {

using mc::levelgen::PerlinSimplexNoise;

// Biome.BIOME_INFO_NOISE = new PerlinSimplexNoise(WorldgenRandom(LegacyRandomSource(2345)), [0]).
inline const PerlinSimplexNoise& biomeInfoNoise() {
    static PerlinSimplexNoise noise = [] {
        auto src = std::make_shared<mc::levelgen::LegacyRandomSource>(2345);
        mc::levelgen::WorldgenRandom random(src);
        return PerlinSimplexNoise(random, std::vector<int32_t>{ 0 });
    }();
    return noise;
}

class NoiseThresholdCountPlacement final : public PlacementModifier {
public:
    NoiseThresholdCountPlacement(double noiseLevel, int belowNoise, int aboveNoise)
        : m_noiseLevel(noiseLevel), m_belowNoise(belowNoise), m_aboveNoise(aboveNoise) {}

    std::vector<BlockPos> getPositions(PlacementContext*, RandomSource&, BlockPos origin) const override {
        const double flowerNoise = biomeInfoNoise().getValue(origin.x / 200.0, origin.z / 200.0, false);
        const int n = flowerNoise < m_noiseLevel ? m_belowNoise : m_aboveNoise;
        return std::vector<BlockPos>(n < 0 ? 0 : static_cast<std::size_t>(n), origin);
    }

private:
    double m_noiseLevel;
    int m_belowNoise;
    int m_aboveNoise;
};

class NoiseBasedCountPlacement final : public PlacementModifier {
public:
    NoiseBasedCountPlacement(int noiseToCountRatio, double noiseFactor, double noiseOffset)
        : m_noiseToCountRatio(noiseToCountRatio), m_noiseFactor(noiseFactor), m_noiseOffset(noiseOffset) {}

    std::vector<BlockPos> getPositions(PlacementContext*, RandomSource&, BlockPos origin) const override {
        const double flowerNoise = biomeInfoNoise().getValue(origin.x / m_noiseFactor, origin.z / m_noiseFactor, false);
        const int n = static_cast<int>(std::ceil((flowerNoise + m_noiseOffset) * static_cast<double>(m_noiseToCountRatio)));
        return std::vector<BlockPos>(n < 0 ? 0 : static_cast<std::size_t>(n), origin);
    }

private:
    int m_noiseToCountRatio;
    double m_noiseFactor;
    double m_noiseOffset;
};

} // namespace mc::levelgen::placement

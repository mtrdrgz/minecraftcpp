#pragma once

#include "DensityFunction.h"
#include "RandomSource.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mc::levelgen {

class SimplexNoise {
public:
    static constexpr std::array<std::array<int32_t, 3>, 16> GRADIENT{{
        {{ 1,  1,  0}},
        {{-1,  1,  0}},
        {{ 1, -1,  0}},
        {{-1, -1,  0}},
        {{ 1,  0,  1}},
        {{-1,  0,  1}},
        {{ 1,  0, -1}},
        {{-1,  0, -1}},
        {{ 0,  1,  1}},
        {{ 0, -1,  1}},
        {{ 0,  1, -1}},
        {{ 0, -1, -1}},
        {{ 1,  1,  0}},
        {{ 0, -1,  1}},
        {{-1,  1,  0}},
        {{ 0, -1, -1}},
    }};

    explicit SimplexNoise(RandomSource& random);

    double getValue(double xin, double yin) const;
    double getValue(double xin, double yin, double zin) const;

    static double dot(const std::array<int32_t, 3>& g, double x, double y, double z);

    double xo = 0.0;
    double yo = 0.0;
    double zo = 0.0;

private:
    std::array<int32_t, 256> m_p{};

    int32_t p(int32_t x) const;
    double getCornerNoise3D(int32_t index, double x, double y, double z, double base) const;
};

// Port of net.minecraft.world.level.levelgen.synth.PerlinSimplexNoise.
class PerlinSimplexNoise {
public:
    PerlinSimplexNoise(RandomSource& random, std::vector<int32_t> octaveSet);

    double getValue(double x, double y, bool useNoiseStart) const;

private:
    std::vector<std::unique_ptr<SimplexNoise>> m_noiseLevels;
    double m_highestFreqValueFactor = 0.0;
    double m_highestFreqInputFactor = 0.0;
};

// Port of net.minecraft.world.level.levelgen.synth.ImprovedNoise.
class ImprovedNoise {
public:
    explicit ImprovedNoise(RandomSource& random);

    double noise(double x, double y, double z) const;
    double noise(double x, double y, double z, double yScale, double yFudge) const;
    double noiseWithDerivative(double x, double y, double z, std::array<double, 3>& derivativeOut) const;

    double xo = 0.0;
    double yo = 0.0;
    double zo = 0.0;

private:
    std::array<uint8_t, 256> m_p{};

    int32_t p(int32_t x) const;
    static double gradDot(int32_t hash, double x, double y, double z);
    double sampleAndLerp(int32_t x, int32_t y, int32_t z, double xr, double yr, double zr, double yrOriginal) const;
    double sampleWithDerivative(int32_t x, int32_t y, int32_t z, double xr, double yr, double zr, std::array<double, 3>& derivativeOut) const;
};

class PerlinNoise {
public:
    PerlinNoise() = default;

    static PerlinNoise createLegacyForBlendedNoise(RandomSource& random, const std::vector<int32_t>& octaves);
    static PerlinNoise createLegacyForLegacyNetherBiome(RandomSource& random, int32_t firstOctave, const std::vector<double>& amplitudes);
    static PerlinNoise create(RandomSource& random, const std::vector<int32_t>& octaveSet);
    static PerlinNoise create(RandomSource& random, int32_t firstOctave, const std::vector<double>& amplitudes);

    double getValue(double x, double y, double z) const;
    double getValue(double x, double y, double z, double yScale, double yFudge) const;
    double maxBrokenValue(double yScale) const;
    double maxValue() const { return m_maxValue; }
    const ImprovedNoise* getOctaveNoise(int32_t i) const;

    static double wrap(double x);

private:
    PerlinNoise(RandomSource& random, int32_t firstOctave, std::vector<double> amplitudes, bool useNewInitialization);

    static std::pair<int32_t, std::vector<double>> makeAmplitudes(std::vector<int32_t> octaveSet);
    static void skipOctave(RandomSource& random);
    double edgeValue(double noiseValue) const;

    int32_t m_firstOctave = 0;
    std::vector<double> m_amplitudes;
    std::vector<std::unique_ptr<ImprovedNoise>> m_noiseLevels;
    double m_lowestFreqValueFactor = 0.0;
    double m_lowestFreqInputFactor = 0.0;
    double m_maxValue = 0.0;
};

struct NoiseParameters {
    int32_t firstOctave = 0;
    std::vector<double> amplitudes;
};

class NormalNoise {
public:
    NormalNoise() = default;

    static NormalNoise create(RandomSource& random, const NoiseParameters& parameters);
    static NormalNoise createLegacyNetherBiome(RandomSource& random, const NoiseParameters& parameters);

    double getValue(double x, double y, double z) const;
    double maxValue() const { return m_maxValue; }
    const NoiseParameters& parameters() const { return m_parameters; }

private:
    NormalNoise(RandomSource& random, NoiseParameters parameters, bool useNewInitialization);

    static double expectedDeviation(int32_t octaveSpan);

    double m_valueFactor = 0.0;
    PerlinNoise m_first;
    PerlinNoise m_second;
    double m_maxValue = 0.0;
    NoiseParameters m_parameters;
};

class BlendedNoise {
public:
    static BlendedNoise createUnseeded(double xzScale, double yScale, double xzFactor, double yFactor, double smearScaleMultiplier);

    BlendedNoise(std::shared_ptr<RandomSource> random, double xzScale, double yScale, double xzFactor, double yFactor, double smearScaleMultiplier);

    double compute(int32_t blockX, int32_t blockY, int32_t blockZ) const;
    double minValue() const { return -maxValue(); }
    double maxValue() const { return m_maxValue; }

private:
    BlendedNoise(
        PerlinNoise minLimitNoise,
        PerlinNoise maxLimitNoise,
        PerlinNoise mainNoise,
        double xzScale,
        double yScale,
        double xzFactor,
        double yFactor,
        double smearScaleMultiplier);

    PerlinNoise m_minLimitNoise;
    PerlinNoise m_maxLimitNoise;
    PerlinNoise m_mainNoise;
    double m_xzMultiplier = 0.0;
    double m_yMultiplier = 0.0;
    double m_xzFactor = 0.0;
    double m_yFactor = 0.0;
    double m_smearScaleMultiplier = 0.0;
    double m_maxValue = 0.0;
    double m_xzScale = 0.0;
    double m_yScale = 0.0;
};

class BlendedNoiseFunction final : public DensityFunction {
public:
    explicit BlendedNoiseFunction(BlendedNoise noise);

    double compute(const DensityFunctionContext& context) const override;
    double minValue() const override;
    double maxValue() const override;

private:
    BlendedNoise m_noise;
};

namespace DensityFunctions {
    DensityFunctionPtr blendedNoise(BlendedNoise noise);
}

class HeightmapGenerator {
public:
    static void generateChunkHeightmap(int chunkX, int chunkZ, std::vector<int>& heightmap, const BlendedNoise& noise);
};

} // namespace mc::levelgen

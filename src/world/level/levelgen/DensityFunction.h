#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace mc::levelgen {

class NormalNoise;
class DensityFunction;

using DensityFunctionPtr = std::shared_ptr<const DensityFunction>;

class DensityFunctionInterpolationResolver {
public:
    virtual ~DensityFunctionInterpolationResolver() = default;
    virtual double computeInterpolated(const DensityFunctionPtr& function, const struct DensityFunctionContext& context) const = 0;
    virtual double computeCacheOnce(const DensityFunctionPtr& function, const struct DensityFunctionContext& context) const = 0;
    virtual double computeCacheAllInCell(const DensityFunctionPtr& function, const struct DensityFunctionContext& context) const = 0;
    virtual double computeCache2D(const DensityFunctionPtr& function, const struct DensityFunctionContext& context) const = 0;
    virtual double computeFlatCache(const DensityFunctionPtr& function, const struct DensityFunctionContext& context) const = 0;
};

struct DensityFunctionContext {
    int blockX = 0;
    int blockY = 0;
    int blockZ = 0;
    const DensityFunctionInterpolationResolver* interpolationResolver = nullptr;
};

class DensityFunction {
public:
    virtual ~DensityFunction() = default;

    virtual double compute(const DensityFunctionContext& context) const = 0;
    virtual double minValue() const = 0;
    virtual double maxValue() const = 0;

    virtual void fillArray(std::vector<double>& output, const std::vector<DensityFunctionContext>& contexts) const;
};

namespace DensityFunctions {
    enum class MapType {
        Abs,
        Square,
        Cube,
        HalfNegative,
        QuarterNegative,
        Invert,
        Squeeze
    };

    DensityFunctionPtr zero();
    DensityFunctionPtr constant(double value);
    DensityFunctionPtr y();
    DensityFunctionPtr interpolated(DensityFunctionPtr input);
    DensityFunctionPtr cacheOnce(DensityFunctionPtr input);
    DensityFunctionPtr cacheAllInCell(DensityFunctionPtr input);
    DensityFunctionPtr cache2d(DensityFunctionPtr input);
    DensityFunctionPtr flatCache(DensityFunctionPtr input);
    DensityFunctionPtr yClampedGradient(int fromY, int toY, double fromValue, double toValue);
    DensityFunctionPtr add(DensityFunctionPtr a, DensityFunctionPtr b);
    DensityFunctionPtr mul(DensityFunctionPtr a, DensityFunctionPtr b);
    DensityFunctionPtr min(DensityFunctionPtr a, DensityFunctionPtr b);
    DensityFunctionPtr max(DensityFunctionPtr a, DensityFunctionPtr b);
    DensityFunctionPtr map(DensityFunctionPtr input, MapType type);
    DensityFunctionPtr clamp(DensityFunctionPtr input, double minValue, double maxValue);
    DensityFunctionPtr rangeChoice(
        DensityFunctionPtr input,
        double minInclusive,
        double maxExclusive,
        DensityFunctionPtr whenInRange,
        DensityFunctionPtr whenOutOfRange);
    DensityFunctionPtr lerp(DensityFunctionPtr alpha, double first, DensityFunctionPtr second);
    DensityFunctionPtr lerp(DensityFunctionPtr alpha, DensityFunctionPtr first, DensityFunctionPtr second);
    DensityFunctionPtr noise(std::shared_ptr<const NormalNoise> noise, double xzScale = 1.0, double yScale = 1.0);
    DensityFunctionPtr mappedNoise(std::shared_ptr<const NormalNoise> noise, double xzScale, double yScale, double minTarget, double maxTarget);
    DensityFunctionPtr mappedNoise(std::shared_ptr<const NormalNoise> noise, double minTarget, double maxTarget);
    DensityFunctionPtr shift(std::shared_ptr<const NormalNoise> offsetNoise);
    DensityFunctionPtr shiftA(std::shared_ptr<const NormalNoise> offsetNoise);
    DensityFunctionPtr shiftB(std::shared_ptr<const NormalNoise> offsetNoise);
    DensityFunctionPtr shiftedNoise2d(DensityFunctionPtr shiftX, DensityFunctionPtr shiftZ, double xzScale, std::shared_ptr<const NormalNoise> noise);
    DensityFunctionPtr endIslands(uint64_t seed);
    DensityFunctionPtr findTopSurface(DensityFunctionPtr density, DensityFunctionPtr upperBound, int lowerBound, int cellHeight);

    enum class RarityValueMapper {
        Type1,
        Type2
    };
    DensityFunctionPtr weirdScaledSampler(DensityFunctionPtr input, std::shared_ptr<const NormalNoise> noise, RarityValueMapper rarityValueMapper);

    DensityFunctionPtr peaksAndValleys(DensityFunctionPtr input);
    double peaksAndValleys(double weirdness);
}

} // namespace mc::levelgen

#include "DensityFunction.h"
#include "Noise.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace mc::levelgen {

void DensityFunction::fillArray(std::vector<double>& output, const std::vector<DensityFunctionContext>& contexts) const {
    output.resize(contexts.size());
    for (size_t i = 0; i < contexts.size(); ++i) {
        output[i] = compute(contexts[i]);
    }
}

namespace {
    double clampedMap(double value, double fromLow, double fromHigh, double toLow, double toHigh) {
        if (fromLow == fromHigh) return value < fromLow ? toLow : toHigh;
        double t = (value - fromLow) / (fromHigh - fromLow);
        t = std::clamp(t, 0.0, 1.0);
        return toLow + t * (toHigh - toLow);
    }

    class Constant final : public DensityFunction {
    public:
        explicit Constant(double value) : m_value(value) {}

        double compute(const DensityFunctionContext&) const override { return m_value; }
        double minValue() const override { return m_value; }
        double maxValue() const override { return m_value; }

        void fillArray(std::vector<double>& output, const std::vector<DensityFunctionContext>& contexts) const override {
            output.assign(contexts.size(), m_value);
        }

        double value() const { return m_value; }

    private:
        double m_value;
    };

    class YFunction final : public DensityFunction {
    public:
        double compute(const DensityFunctionContext& context) const override {
            return context.blockY;
        }

        double minValue() const override { return -4096.0; }
        double maxValue() const override { return 4096.0; }
    };

    class Interpolated final : public DensityFunction {
    public:
        explicit Interpolated(DensityFunctionPtr input) : m_input(std::move(input)) {}

        double compute(const DensityFunctionContext& context) const override {
            if (context.interpolationResolver) {
                return context.interpolationResolver->computeInterpolated(m_input, context);
            }
            return m_input->compute(context);
        }

        double minValue() const override { return m_input->minValue(); }
        double maxValue() const override { return m_input->maxValue(); }

    private:
        DensityFunctionPtr m_input;
    };

    enum class CacheMarkerType {
        CacheOnce,
        CacheAllInCell,
        Cache2D,
        FlatCache
    };

    class CacheMarker final : public DensityFunction {
    public:
        CacheMarker(CacheMarkerType type, DensityFunctionPtr input)
            : m_type(type), m_input(std::move(input)) {
        }

        double compute(const DensityFunctionContext& context) const override {
            if (!context.interpolationResolver) {
                return m_input->compute(context);
            }

            switch (m_type) {
            case CacheMarkerType::CacheOnce:
                return context.interpolationResolver->computeCacheOnce(m_input, context);
            case CacheMarkerType::CacheAllInCell:
                return context.interpolationResolver->computeCacheAllInCell(m_input, context);
            case CacheMarkerType::Cache2D:
                return context.interpolationResolver->computeCache2D(m_input, context);
            case CacheMarkerType::FlatCache:
                return context.interpolationResolver->computeFlatCache(m_input, context);
            }
            return m_input->compute(context);
        }

        double minValue() const override { return m_input->minValue(); }
        double maxValue() const override { return m_input->maxValue(); }

    private:
        CacheMarkerType m_type;
        DensityFunctionPtr m_input;
    };

    class YClampedGradient final : public DensityFunction {
    public:
        YClampedGradient(int fromY, int toY, double fromValue, double toValue)
            : m_fromY(fromY), m_toY(toY), m_fromValue(fromValue), m_toValue(toValue) {}

        double compute(const DensityFunctionContext& context) const override {
            return clampedMap((double)context.blockY, (double)m_fromY, (double)m_toY, m_fromValue, m_toValue);
        }

        double minValue() const override { return std::min(m_fromValue, m_toValue); }
        double maxValue() const override { return std::max(m_fromValue, m_toValue); }

    private:
        int m_fromY;
        int m_toY;
        double m_fromValue;
        double m_toValue;
    };

    enum class TwoArgType { Add, Mul, Min, Max };

    class TwoArgument final : public DensityFunction {
    public:
        TwoArgument(TwoArgType type, DensityFunctionPtr a, DensityFunctionPtr b, double minValue, double maxValue)
            : m_type(type), m_a(std::move(a)), m_b(std::move(b)), m_minValue(minValue), m_maxValue(maxValue) {}

        double compute(const DensityFunctionContext& context) const override {
            double v1 = m_a->compute(context);
            switch (m_type) {
            case TwoArgType::Add:
                return v1 + m_b->compute(context);
            case TwoArgType::Mul:
                return v1 == 0.0 ? 0.0 : v1 * m_b->compute(context);
            case TwoArgType::Min:
                return v1 < m_b->minValue() ? v1 : std::min(v1, m_b->compute(context));
            case TwoArgType::Max:
                return v1 > m_b->maxValue() ? v1 : std::max(v1, m_b->compute(context));
            }
            return 0.0;
        }

        double minValue() const override { return m_minValue; }
        double maxValue() const override { return m_maxValue; }

    private:
        TwoArgType m_type;
        DensityFunctionPtr m_a;
        DensityFunctionPtr m_b;
        double m_minValue;
        double m_maxValue;
    };

    class Mapped final : public DensityFunction {
    public:
        Mapped(DensityFunctions::MapType type, DensityFunctionPtr input, double minValue, double maxValue)
            : m_type(type), m_input(std::move(input)), m_minValue(minValue), m_maxValue(maxValue) {}

        static double transform(DensityFunctions::MapType type, double input) {
            switch (type) {
            case DensityFunctions::MapType::Abs:
                return std::abs(input);
            case DensityFunctions::MapType::Square:
                return input * input;
            case DensityFunctions::MapType::Cube:
                return input * input * input;
            case DensityFunctions::MapType::HalfNegative:
                return input > 0.0 ? input : input * 0.5;
            case DensityFunctions::MapType::QuarterNegative:
                return input > 0.0 ? input : input * 0.25;
            case DensityFunctions::MapType::Invert:
                return 1.0 / input;
            case DensityFunctions::MapType::Squeeze: {
                double c = std::clamp(input, -1.0, 1.0);
                return c / 2.0 - c * c * c / 24.0;
            }
            }
            return input;
        }

        double compute(const DensityFunctionContext& context) const override {
            return transform(m_type, m_input->compute(context));
        }

        double minValue() const override { return m_minValue; }
        double maxValue() const override { return m_maxValue; }

    private:
        DensityFunctions::MapType m_type;
        DensityFunctionPtr m_input;
        double m_minValue;
        double m_maxValue;
    };

    class Clamp final : public DensityFunction {
    public:
        Clamp(DensityFunctionPtr input, double minValue, double maxValue)
            : m_input(std::move(input)), m_minValue(minValue), m_maxValue(maxValue) {}

        double compute(const DensityFunctionContext& context) const override {
            return std::clamp(m_input->compute(context), m_minValue, m_maxValue);
        }

        double minValue() const override { return m_minValue; }
        double maxValue() const override { return m_maxValue; }

    private:
        DensityFunctionPtr m_input;
        double m_minValue;
        double m_maxValue;
    };

    class RangeChoice final : public DensityFunction {
    public:
        RangeChoice(DensityFunctionPtr input, double minInclusive, double maxExclusive,
                    DensityFunctionPtr whenInRange, DensityFunctionPtr whenOutOfRange)
            : m_input(std::move(input)), m_minInclusive(minInclusive), m_maxExclusive(maxExclusive),
              m_whenInRange(std::move(whenInRange)), m_whenOutOfRange(std::move(whenOutOfRange)) {}

        double compute(const DensityFunctionContext& context) const override {
            double inputValue = m_input->compute(context);
            return inputValue >= m_minInclusive && inputValue < m_maxExclusive
                ? m_whenInRange->compute(context)
                : m_whenOutOfRange->compute(context);
        }

        double minValue() const override {
            return std::min(m_whenInRange->minValue(), m_whenOutOfRange->minValue());
        }

        double maxValue() const override {
            return std::max(m_whenInRange->maxValue(), m_whenOutOfRange->maxValue());
        }

    private:
        DensityFunctionPtr m_input;
        double m_minInclusive;
        double m_maxExclusive;
        DensityFunctionPtr m_whenInRange;
        DensityFunctionPtr m_whenOutOfRange;
    };

    class NoiseFunction final : public DensityFunction {
    public:
        NoiseFunction(std::shared_ptr<const NormalNoise> noise, double xzScale, double yScale)
            : m_noise(std::move(noise)), m_xzScale(xzScale), m_yScale(yScale) {}

        double compute(const DensityFunctionContext& context) const override {
            return m_noise->getValue(context.blockX * m_xzScale, context.blockY * m_yScale, context.blockZ * m_xzScale);
        }

        double minValue() const override { return -maxValue(); }
        double maxValue() const override { return m_noise->maxValue(); }

    private:
        std::shared_ptr<const NormalNoise> m_noise;
        double m_xzScale;
        double m_yScale;
    };

    class ShiftNoiseFunction final : public DensityFunction {
    public:
        enum class Axis {
            XYZ,
            X0Z,
            ZX0
        };

        ShiftNoiseFunction(std::shared_ptr<const NormalNoise> noise, Axis axis)
            : m_noise(std::move(noise)), m_axis(axis) {}

        double compute(const DensityFunctionContext& context) const override {
            switch (m_axis) {
            case Axis::XYZ:
                return computeShift(context.blockX, context.blockY, context.blockZ);
            case Axis::X0Z:
                return computeShift(context.blockX, 0.0, context.blockZ);
            case Axis::ZX0:
                return computeShift(context.blockZ, context.blockX, 0.0);
            }
            return 0.0;
        }

        double minValue() const override { return -maxValue(); }
        double maxValue() const override { return m_noise->maxValue() * 4.0; }

    private:
        double computeShift(double localX, double localY, double localZ) const {
            return m_noise->getValue(localX * 0.25, localY * 0.25, localZ * 0.25) * 4.0;
        }

        std::shared_ptr<const NormalNoise> m_noise;
        Axis m_axis;
    };

    class ShiftedNoiseFunction final : public DensityFunction {
    public:
        ShiftedNoiseFunction(DensityFunctionPtr shiftX, DensityFunctionPtr shiftY, DensityFunctionPtr shiftZ, double xzScale, double yScale, std::shared_ptr<const NormalNoise> noise)
            : m_shiftX(std::move(shiftX)), m_shiftY(std::move(shiftY)), m_shiftZ(std::move(shiftZ)),
              m_xzScale(xzScale), m_yScale(yScale), m_noise(std::move(noise)) {}

        double compute(const DensityFunctionContext& context) const override {
            double x = context.blockX * m_xzScale + m_shiftX->compute(context);
            double y = context.blockY * m_yScale + m_shiftY->compute(context);
            double z = context.blockZ * m_xzScale + m_shiftZ->compute(context);
            return m_noise->getValue(x, y, z);
        }

        double minValue() const override { return -maxValue(); }
        double maxValue() const override { return m_noise->maxValue(); }

    private:
        DensityFunctionPtr m_shiftX;
        DensityFunctionPtr m_shiftY;
        DensityFunctionPtr m_shiftZ;
        double m_xzScale;
        double m_yScale;
        std::shared_ptr<const NormalNoise> m_noise;
    };

    class WeirdScaledSamplerFunction final : public DensityFunction {
    public:
        WeirdScaledSamplerFunction(DensityFunctionPtr input, std::shared_ptr<const NormalNoise> noise, DensityFunctions::RarityValueMapper mapper)
            : m_input(std::move(input)), m_noise(std::move(noise)), m_mapper(mapper) {}

        double compute(const DensityFunctionContext& context) const override {
            double input = m_input->compute(context);
            double rarity = rarityValue(input);
            return rarity * std::abs(m_noise->getValue(context.blockX / rarity, context.blockY / rarity, context.blockZ / rarity));
        }

        double minValue() const override { return 0.0; }
        double maxValue() const override { return maxRarity() * m_noise->maxValue(); }

    private:
        double rarityValue(double rarityFactor) const {
            if (m_mapper == DensityFunctions::RarityValueMapper::Type1) {
                if (rarityFactor < -0.5) return 0.75;
                if (rarityFactor < 0.0) return 1.0;
                return rarityFactor < 0.5 ? 1.5 : 2.0;
            }

            if (rarityFactor < -0.75) return 0.5;
            if (rarityFactor < -0.5) return 0.75;
            if (rarityFactor < 0.5) return 1.0;
            return rarityFactor < 0.75 ? 2.0 : 3.0;
        }

        double maxRarity() const {
            return m_mapper == DensityFunctions::RarityValueMapper::Type1 ? 2.0 : 3.0;
        }

        DensityFunctionPtr m_input;
        std::shared_ptr<const NormalNoise> m_noise;
        DensityFunctions::RarityValueMapper m_mapper;
    };

    class EndIslandDensityFunction final : public DensityFunction {
    public:
        explicit EndIslandDensityFunction(uint64_t seed)
            : m_islandNoise(makeIslandNoise(seed)) {}

        double compute(const DensityFunctionContext& context) const override {
            return (getHeightValue(context.blockX / 8, context.blockZ / 8) - 8.0f) / 128.0;
        }

        double minValue() const override { return -0.84375; }
        double maxValue() const override { return 0.5625; }

    private:
        static SimplexNoise makeIslandNoise(uint64_t seed) {
            LegacyRandomSource islandRandom(static_cast<int64_t>(seed));
            islandRandom.consumeCount(17292);
            return SimplexNoise(islandRandom);
        }

        float getHeightValue(int sectionX, int sectionZ) const {
            int chunkX = sectionX / 2;
            int chunkZ = sectionZ / 2;
            int subSectionX = sectionX % 2;
            int subSectionZ = sectionZ % 2;
            float doffs = 100.0f - std::sqrt((float)(sectionX * sectionX + sectionZ * sectionZ)) * 8.0f;
            doffs = std::clamp(doffs, -100.0f, 80.0f);

            for (int xo = -12; xo <= 12; ++xo) {
                for (int zo = -12; zo <= 12; ++zo) {
                    int64_t totalChunkX = (int64_t)chunkX + xo;
                    int64_t totalChunkZ = (int64_t)chunkZ + zo;
                    if (totalChunkX * totalChunkX + totalChunkZ * totalChunkZ > 4096LL
                        && m_islandNoise.getValue((double)totalChunkX, (double)totalChunkZ) < -0.9) {
                        float islandSize = std::fmod(std::abs((float)totalChunkX) * 3439.0f + std::abs((float)totalChunkZ) * 147.0f, 13.0f) + 9.0f;
                        float xd = (float)(subSectionX - xo * 2);
                        float zd = (float)(subSectionZ - zo * 2);
                        float newDoffs = 100.0f - std::sqrt(xd * xd + zd * zd) * islandSize;
                        newDoffs = std::clamp(newDoffs, -100.0f, 80.0f);
                        doffs = std::max(doffs, newDoffs);
                    }
                }
            }

            return doffs;
        }

        SimplexNoise m_islandNoise;
    };

    class FindTopSurfaceFunction final : public DensityFunction {
    public:
        FindTopSurfaceFunction(DensityFunctionPtr density, DensityFunctionPtr upperBound, int lowerBound, int cellHeight)
            : m_density(std::move(density)), m_upperBound(std::move(upperBound)), m_lowerBound(lowerBound), m_cellHeight(cellHeight) {}

        double compute(const DensityFunctionContext& context) const override {
            int topY = (int)std::floor(m_upperBound->compute(context) / m_cellHeight) * m_cellHeight;
            if (topY <= m_lowerBound) {
                return m_lowerBound;
            }

            for (int blockY = topY; blockY >= m_lowerBound; blockY -= m_cellHeight) {
                if (m_density->compute(DensityFunctionContext{ context.blockX, blockY, context.blockZ }) > 0.0) {
                    return blockY;
                }
            }

            return m_lowerBound;
        }

        double minValue() const override { return m_lowerBound; }
        double maxValue() const override { return std::max((double)m_lowerBound, m_upperBound->maxValue()); }

    private:
        DensityFunctionPtr m_density;
        DensityFunctionPtr m_upperBound;
        int m_lowerBound;
        int m_cellHeight;
    };

    class PeaksAndValleysFunction final : public DensityFunction {
    public:
        explicit PeaksAndValleysFunction(DensityFunctionPtr input) : m_input(std::move(input)) {}

        double compute(const DensityFunctionContext& context) const override {
            return DensityFunctions::peaksAndValleys(m_input->compute(context));
        }

        double minValue() const override { return -1.0; }
        double maxValue() const override { return 1.0; }

    private:
        DensityFunctionPtr m_input;
    };

    DensityFunctionPtr makeTwoArg(TwoArgType type, DensityFunctionPtr a, DensityFunctionPtr b) {
        double min1 = a->minValue();
        double min2 = b->minValue();
        double max1 = a->maxValue();
        double max2 = b->maxValue();

        double minValue = 0.0;
        double maxValue = 0.0;
        switch (type) {
        case TwoArgType::Add:
            minValue = min1 + min2;
            maxValue = max1 + max2;
            break;
        case TwoArgType::Mul:
            minValue = min1 > 0.0 && min2 > 0.0 ? min1 * min2
                : (max1 < 0.0 && max2 < 0.0 ? max1 * max2 : std::min(min1 * max2, max1 * min2));
            maxValue = min1 > 0.0 && min2 > 0.0 ? max1 * max2
                : (max1 < 0.0 && max2 < 0.0 ? min1 * min2 : std::max(min1 * min2, max1 * max2));
            break;
        case TwoArgType::Min:
            minValue = std::min(min1, min2);
            maxValue = std::min(max1, max2);
            break;
        case TwoArgType::Max:
            minValue = std::max(min1, min2);
            maxValue = std::max(max1, max2);
            break;
        }

        return std::make_shared<TwoArgument>(type, std::move(a), std::move(b), minValue, maxValue);
    }
}

namespace DensityFunctions {

DensityFunctionPtr zero() {
    static DensityFunctionPtr value = std::make_shared<Constant>(0.0);
    return value;
}

DensityFunctionPtr constant(double value) {
    if (value == 0.0) return zero();
    return std::make_shared<Constant>(value);
}

DensityFunctionPtr y() {
    static DensityFunctionPtr value = std::make_shared<YFunction>();
    return value;
}

DensityFunctionPtr interpolated(DensityFunctionPtr input) {
    return std::make_shared<Interpolated>(std::move(input));
}

DensityFunctionPtr cacheOnce(DensityFunctionPtr input) {
    return std::make_shared<CacheMarker>(CacheMarkerType::CacheOnce, std::move(input));
}

DensityFunctionPtr cacheAllInCell(DensityFunctionPtr input) {
    return std::make_shared<CacheMarker>(CacheMarkerType::CacheAllInCell, std::move(input));
}

DensityFunctionPtr cache2d(DensityFunctionPtr input) {
    return std::make_shared<CacheMarker>(CacheMarkerType::Cache2D, std::move(input));
}

DensityFunctionPtr flatCache(DensityFunctionPtr input) {
    return std::make_shared<CacheMarker>(CacheMarkerType::FlatCache, std::move(input));
}

DensityFunctionPtr yClampedGradient(int fromY, int toY, double fromValue, double toValue) {
    return std::make_shared<YClampedGradient>(fromY, toY, fromValue, toValue);
}

DensityFunctionPtr add(DensityFunctionPtr a, DensityFunctionPtr b) {
    return makeTwoArg(TwoArgType::Add, std::move(a), std::move(b));
}

DensityFunctionPtr mul(DensityFunctionPtr a, DensityFunctionPtr b) {
    return makeTwoArg(TwoArgType::Mul, std::move(a), std::move(b));
}

DensityFunctionPtr min(DensityFunctionPtr a, DensityFunctionPtr b) {
    return makeTwoArg(TwoArgType::Min, std::move(a), std::move(b));
}

DensityFunctionPtr max(DensityFunctionPtr a, DensityFunctionPtr b) {
    return makeTwoArg(TwoArgType::Max, std::move(a), std::move(b));
}

DensityFunctionPtr map(DensityFunctionPtr input, MapType type) {
    double minValue = input->minValue();
    double maxValue = input->maxValue();
    double minImage = Mapped::transform(type, minValue);
    double maxImage = Mapped::transform(type, maxValue);

    if (type == MapType::Invert) {
        if (minValue < 0.0 && maxValue > 0.0) {
            return std::make_shared<Mapped>(type, input, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
        }
        return std::make_shared<Mapped>(type, input, maxImage, minImage);
    }

    if (type == MapType::Abs || type == MapType::Square) {
        return std::make_shared<Mapped>(type, input, std::max(0.0, minValue), std::max(minImage, maxImage));
    }

    return std::make_shared<Mapped>(type, input, minImage, maxImage);
}

DensityFunctionPtr clamp(DensityFunctionPtr input, double minValue, double maxValue) {
    return std::make_shared<Clamp>(std::move(input), minValue, maxValue);
}

DensityFunctionPtr rangeChoice(DensityFunctionPtr input, double minInclusive, double maxExclusive,
                               DensityFunctionPtr whenInRange, DensityFunctionPtr whenOutOfRange) {
    return std::make_shared<RangeChoice>(
        std::move(input), minInclusive, maxExclusive, std::move(whenInRange), std::move(whenOutOfRange));
}

DensityFunctionPtr lerp(DensityFunctionPtr alpha, double first, DensityFunctionPtr second) {
    return add(mul(alpha, add(second, constant(-first))), constant(first));
}

DensityFunctionPtr lerp(DensityFunctionPtr alpha, DensityFunctionPtr first, DensityFunctionPtr second) {
    auto oneMinusAlpha = add(mul(alpha, constant(-1.0)), constant(1.0));
    return add(mul(std::move(first), std::move(oneMinusAlpha)), mul(std::move(second), std::move(alpha)));
}

DensityFunctionPtr noise(std::shared_ptr<const NormalNoise> noise, double xzScale, double yScale) {
    return std::make_shared<NoiseFunction>(std::move(noise), xzScale, yScale);
}

DensityFunctionPtr mappedNoise(std::shared_ptr<const NormalNoise> noise, double xzScale, double yScale, double minTarget, double maxTarget) {
    double middle = (minTarget + maxTarget) * 0.5;
    double factor = (maxTarget - minTarget) * 0.5;
    return add(constant(middle), mul(constant(factor), DensityFunctions::noise(std::move(noise), xzScale, yScale)));
}

DensityFunctionPtr mappedNoise(std::shared_ptr<const NormalNoise> noise, double minTarget, double maxTarget) {
    return mappedNoise(std::move(noise), 1.0, 1.0, minTarget, maxTarget);
}

DensityFunctionPtr shift(std::shared_ptr<const NormalNoise> offsetNoise) {
    return std::make_shared<ShiftNoiseFunction>(std::move(offsetNoise), ShiftNoiseFunction::Axis::XYZ);
}

DensityFunctionPtr shiftA(std::shared_ptr<const NormalNoise> offsetNoise) {
    return std::make_shared<ShiftNoiseFunction>(std::move(offsetNoise), ShiftNoiseFunction::Axis::X0Z);
}

DensityFunctionPtr shiftB(std::shared_ptr<const NormalNoise> offsetNoise) {
    return std::make_shared<ShiftNoiseFunction>(std::move(offsetNoise), ShiftNoiseFunction::Axis::ZX0);
}

DensityFunctionPtr shiftedNoise2d(DensityFunctionPtr shiftX, DensityFunctionPtr shiftZ, double xzScale, std::shared_ptr<const NormalNoise> noise) {
    return std::make_shared<ShiftedNoiseFunction>(std::move(shiftX), zero(), std::move(shiftZ), xzScale, 0.0, std::move(noise));
}

DensityFunctionPtr endIslands(uint64_t seed) {
    return std::make_shared<EndIslandDensityFunction>(seed);
}

DensityFunctionPtr findTopSurface(DensityFunctionPtr density, DensityFunctionPtr upperBound, int lowerBound, int cellHeight) {
    return std::make_shared<FindTopSurfaceFunction>(std::move(density), std::move(upperBound), lowerBound, cellHeight);
}

DensityFunctionPtr weirdScaledSampler(DensityFunctionPtr input, std::shared_ptr<const NormalNoise> noise, RarityValueMapper rarityValueMapper) {
    return std::make_shared<WeirdScaledSamplerFunction>(std::move(input), std::move(noise), rarityValueMapper);
}

DensityFunctionPtr peaksAndValleys(DensityFunctionPtr input) {
    return std::make_shared<PeaksAndValleysFunction>(std::move(input));
}

double peaksAndValleys(double weirdness) {
    // RIDGES_FOLDED density function (NoiseRouterData.peaksAndValleys(DensityFunction)):
    // double arithmetic mul(add(abs(add(abs(w), -2/3)), -1/3), -3.0). The -2/3 and -1/3
    // are the DOUBLE constants, not the float helper's 0.6666667F / 0.33333334F.
    return (std::abs(std::abs(weirdness) - 0.6666666666666666) - 0.3333333333333333) * -3.0;
}

} // namespace DensityFunctions

} // namespace mc::levelgen

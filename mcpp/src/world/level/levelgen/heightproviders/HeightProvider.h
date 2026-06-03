#pragma once

// Port of net.minecraft.world.level.levelgen.heightproviders.HeightProvider and
// its concrete types (constant, uniform, biased_to_bottom,
// very_biased_to_bottom, trapezoid). Runtime sampling only.

#include "../RandomSource.h"
#include "../VerticalAnchor.h"
#include "../WorldGenerationContext.h"

#include <memory>
#include <utility>

namespace mc::levelgen::heightproviders {

using mc::levelgen::RandomSource;
using mc::levelgen::VerticalAnchorPtr;
using mc::levelgen::WorldGenerationContext;

// Mth.nextInt / Mth.randomBetweenInclusive: random.nextInt(max - min + 1) + min.
inline int mthNextInt(RandomSource& r, int min, int max) { return r.nextInt(max - min + 1) + min; }

class HeightProvider {
public:
    virtual ~HeightProvider() = default;
    virtual int sample(RandomSource& random, const WorldGenerationContext& context) const = 0;
};

using HeightProviderPtr = std::shared_ptr<const HeightProvider>;

class ConstantHeight final : public HeightProvider {
public:
    explicit ConstantHeight(VerticalAnchorPtr value) : m_value(std::move(value)) {}
    int sample(RandomSource&, const WorldGenerationContext& c) const override { return m_value->resolveY(c); }

private:
    VerticalAnchorPtr m_value;
};

class UniformHeight final : public HeightProvider {
public:
    UniformHeight(VerticalAnchorPtr min, VerticalAnchorPtr max) : m_min(std::move(min)), m_max(std::move(max)) {}
    int sample(RandomSource& r, const WorldGenerationContext& c) const override {
        const int min = m_min->resolveY(c);
        const int max = m_max->resolveY(c);
        if (min > max) return min;
        return mthNextInt(r, min, max);
    }

private:
    VerticalAnchorPtr m_min, m_max;
};

class BiasedToBottomHeight final : public HeightProvider {
public:
    BiasedToBottomHeight(VerticalAnchorPtr min, VerticalAnchorPtr max, int inner)
        : m_min(std::move(min)), m_max(std::move(max)), m_inner(inner) {}
    int sample(RandomSource& r, const WorldGenerationContext& c) const override {
        const int min = m_min->resolveY(c);
        const int max = m_max->resolveY(c);
        if (max - min - m_inner + 1 <= 0) return min;
        const int limit = r.nextInt(max - min - m_inner + 1);
        return r.nextInt(limit + m_inner) + min;
    }

private:
    VerticalAnchorPtr m_min, m_max;
    int m_inner;
};

class VeryBiasedToBottomHeight final : public HeightProvider {
public:
    VeryBiasedToBottomHeight(VerticalAnchorPtr min, VerticalAnchorPtr max, int inner)
        : m_min(std::move(min)), m_max(std::move(max)), m_inner(inner) {}
    int sample(RandomSource& r, const WorldGenerationContext& c) const override {
        const int min = m_min->resolveY(c);
        const int max = m_max->resolveY(c);
        if (max - min - m_inner + 1 <= 0) return min;
        const int upperInclusive = mthNextInt(r, min + m_inner, max);
        const int biasedUpperInclusive = mthNextInt(r, min, upperInclusive - 1);
        return mthNextInt(r, min, biasedUpperInclusive - 1 + m_inner);
    }

private:
    VerticalAnchorPtr m_min, m_max;
    int m_inner;
};

class TrapezoidHeight final : public HeightProvider {
public:
    TrapezoidHeight(VerticalAnchorPtr min, VerticalAnchorPtr max, int plateau)
        : m_min(std::move(min)), m_max(std::move(max)), m_plateau(plateau) {}
    int sample(RandomSource& r, const WorldGenerationContext& c) const override {
        const int min = m_min->resolveY(c);
        const int max = m_max->resolveY(c);
        if (min > max) return min;
        const int range = max - min;
        if (m_plateau >= range) return mthNextInt(r, min, max);
        const int plateauStart = (range - m_plateau) / 2;
        const int plateauEnd = range - plateauStart;
        const int e = mthNextInt(r, 0, plateauEnd);
        const int s = mthNextInt(r, 0, plateauStart);
        return min + e + s;
    }

private:
    VerticalAnchorPtr m_min, m_max;
    int m_plateau;
};

} // namespace mc::levelgen::heightproviders

#pragma once

// Port of net.minecraft.util.valueproviders.FloatProvider and its concrete
// types (constant, uniform, clamped_normal, trapezoid). Runtime sampling only.

#include "RandomSource.h"

#include <algorithm>
#include <cstdint>
#include <memory>

namespace mc::valueproviders {

using mc::levelgen::RandomSource;

// Mth helpers used by the float providers.
inline float mthRandomBetween(RandomSource& r, float min, float maxExclusive) {
    return r.nextFloat() * (maxExclusive - min) + min;
}
inline float mthNormal(RandomSource& r, float mean, float deviation) {
    return mean + static_cast<float>(r.nextGaussian()) * deviation;
}
inline float mthClampF(float value, float min, float max) {
    return value < min ? min : std::min(value, max);
}

class FloatProvider {
public:
    virtual ~FloatProvider() = default;
    virtual float sample(RandomSource& random) const = 0;
};

using FloatProviderPtr = std::shared_ptr<const FloatProvider>;

class ConstantFloat final : public FloatProvider {
public:
    explicit ConstantFloat(float value) : m_value(value) {}
    static FloatProviderPtr of(float value) { return std::make_shared<ConstantFloat>(value); }
    float sample(RandomSource&) const override { return m_value; }

private:
    float m_value;
};

class UniformFloat final : public FloatProvider {
public:
    UniformFloat(float min, float maxExclusive) : m_min(min), m_max(maxExclusive) {}
    static FloatProviderPtr of(float min, float maxExclusive) { return std::make_shared<UniformFloat>(min, maxExclusive); }
    float sample(RandomSource& r) const override { return mthRandomBetween(r, m_min, m_max); }

private:
    float m_min, m_max;
};

class ClampedNormalFloat final : public FloatProvider {
public:
    ClampedNormalFloat(float mean, float deviation, float min, float max)
        : m_mean(mean), m_deviation(deviation), m_min(min), m_max(max) {}
    static FloatProviderPtr of(float mean, float deviation, float min, float max) {
        return std::make_shared<ClampedNormalFloat>(mean, deviation, min, max);
    }
    float sample(RandomSource& r) const override {
        return mthClampF(mthNormal(r, m_mean, m_deviation), m_min, m_max);
    }

private:
    float m_mean, m_deviation, m_min, m_max;
};

class TrapezoidFloat final : public FloatProvider {
public:
    TrapezoidFloat(float min, float max, float plateau) : m_min(min), m_max(max), m_plateau(plateau) {}
    static FloatProviderPtr of(float min, float max, float plateau) {
        return std::make_shared<TrapezoidFloat>(min, max, plateau);
    }
    // min + nextFloat()*plateauEnd + nextFloat()*plateauStart (two draws, in order)
    float sample(RandomSource& r) const override {
        const float range = m_max - m_min;
        const float plateauStart = (range - m_plateau) / 2.0F;
        const float plateauEnd = range - plateauStart;
        const float a = r.nextFloat();
        const float b = r.nextFloat();
        return m_min + a * plateauEnd + b * plateauStart;
    }

private:
    float m_min, m_max, m_plateau;
};

} // namespace mc::valueproviders

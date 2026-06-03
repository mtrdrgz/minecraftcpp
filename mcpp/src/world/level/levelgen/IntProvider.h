#pragma once

// Port of net.minecraft.util.valueproviders.IntProvider and its concrete types.
// These are the count/spread value samplers used throughout worldgen (placement
// modifiers, feature configs). Only the runtime sampling behaviour is modelled
// here; JSON loading is added with the feature loaders.

#include "RandomSource.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mc::valueproviders {

using mc::levelgen::RandomSource;

class IntProvider {
public:
    virtual ~IntProvider() = default;
    virtual int32_t sample(RandomSource& random) const = 0;
    virtual int32_t minInclusive() const = 0;
    virtual int32_t maxInclusive() const = 0;
};

using IntProviderPtr = std::shared_ptr<const IntProvider>;

class ConstantInt final : public IntProvider {
public:
    explicit ConstantInt(int32_t value) : m_value(value) {}
    static IntProviderPtr of(int32_t value) { return std::make_shared<ConstantInt>(value); }
    int32_t sample(RandomSource&) const override { return m_value; }
    int32_t minInclusive() const override { return m_value; }
    int32_t maxInclusive() const override { return m_value; }
    int32_t value() const { return m_value; }

private:
    int32_t m_value;
};

class UniformInt final : public IntProvider {
public:
    UniformInt(int32_t minInclusive, int32_t maxInclusive) : m_min(minInclusive), m_max(maxInclusive) {}
    static IntProviderPtr of(int32_t lo, int32_t hi) { return std::make_shared<UniformInt>(lo, hi); }
    // Mth.randomBetweenInclusive: random.nextInt(max - min + 1) + min
    int32_t sample(RandomSource& random) const override { return random.nextInt(m_max - m_min + 1) + m_min; }
    int32_t minInclusive() const override { return m_min; }
    int32_t maxInclusive() const override { return m_max; }

private:
    int32_t m_min, m_max;
};

class BiasedToBottomInt final : public IntProvider {
public:
    BiasedToBottomInt(int32_t minInclusive, int32_t maxInclusive) : m_min(minInclusive), m_max(maxInclusive) {}
    static IntProviderPtr of(int32_t lo, int32_t hi) { return std::make_shared<BiasedToBottomInt>(lo, hi); }
    // min + random.nextInt(random.nextInt(max - min + 1) + 1)
    int32_t sample(RandomSource& random) const override {
        const int32_t inner = random.nextInt(m_max - m_min + 1);
        return m_min + random.nextInt(inner + 1);
    }
    int32_t minInclusive() const override { return m_min; }
    int32_t maxInclusive() const override { return m_max; }

private:
    int32_t m_min, m_max;
};

class ClampedInt final : public IntProvider {
public:
    ClampedInt(IntProviderPtr source, int32_t minInclusive, int32_t maxInclusive)
        : m_source(std::move(source)), m_min(minInclusive), m_max(maxInclusive) {}
    static IntProviderPtr of(IntProviderPtr src, int32_t lo, int32_t hi) {
        return std::make_shared<ClampedInt>(std::move(src), lo, hi);
    }
    int32_t sample(RandomSource& random) const override {
        const int32_t v = m_source->sample(random);
        return std::min(std::max(v, m_min), m_max);
    }
    int32_t minInclusive() const override { return std::max(m_min, m_source->minInclusive()); }
    int32_t maxInclusive() const override { return std::min(m_max, m_source->maxInclusive()); }

private:
    IntProviderPtr m_source;
    int32_t m_min, m_max;
};

class TrapezoidInt final : public IntProvider {
public:
    TrapezoidInt(int32_t minInclusive, int32_t maxInclusive, int32_t plateau)
        : m_min(minInclusive), m_max(maxInclusive), m_plateau(plateau) {}
    static IntProviderPtr of(int32_t lo, int32_t hi, int32_t plateau) {
        return std::make_shared<TrapezoidInt>(lo, hi, plateau);
    }
    int32_t sample(RandomSource& random) const override {
        if (m_plateau == 0 && m_max == -m_min) {
            const int32_t a = random.nextInt(m_max + 1);
            const int32_t b = random.nextInt(m_max + 1);
            return a - b;
        }
        const int32_t range = m_max - m_min;
        if (m_plateau == range) {
            return random.nextInt(range + 1) + m_min; // Mth.randomBetweenInclusive(min,max)
        }
        const int32_t plateauStart = (range - m_plateau) / 2;
        const int32_t plateauEnd = range - plateauStart;
        const int32_t e = random.nextInt(plateauEnd + 1); // randomBetweenInclusive(0, plateauEnd)
        const int32_t s = random.nextInt(plateauStart + 1); // randomBetweenInclusive(0, plateauStart)
        return m_min + e + s;
    }
    int32_t minInclusive() const override { return m_min; }
    int32_t maxInclusive() const override { return m_max; }

private:
    int32_t m_min, m_max, m_plateau;
};

class WeightedListInt final : public IntProvider {
public:
    struct Entry {
        IntProviderPtr value;
        int32_t weight;
    };

    explicit WeightedListInt(std::vector<Entry> distribution) : m_distribution(std::move(distribution)) {
        m_totalWeight = 0;
        m_min = std::numeric_limits<int32_t>::max();
        m_max = std::numeric_limits<int32_t>::min();
        for (const auto& e : m_distribution) {
            m_totalWeight += e.weight;
            m_min = std::min(m_min, e.value->minInclusive());
            m_max = std::max(m_max, e.value->maxInclusive());
        }
    }

    // getRandomOrThrow: selection = random.nextInt(totalWeight); walk the cumulative
    // weights (matches both WeightedList.Flat and WeightedList.Compact). Then sample
    // the chosen provider.
    int32_t sample(RandomSource& random) const override {
        int32_t selection = random.nextInt(m_totalWeight);
        for (const auto& e : m_distribution) {
            selection -= e.weight;
            if (selection < 0) {
                return e.value->sample(random);
            }
        }
        throw std::runtime_error("WeightedListInt: selection exceeded total weight");
    }
    int32_t minInclusive() const override { return m_min; }
    int32_t maxInclusive() const override { return m_max; }

private:
    std::vector<Entry> m_distribution;
    int32_t m_totalWeight, m_min, m_max;
};

} // namespace mc::valueproviders

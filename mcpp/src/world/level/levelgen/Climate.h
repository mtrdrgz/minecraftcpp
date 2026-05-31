#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mc::levelgen::Climate {

inline int64_t quantizeCoord(float coord) {
    return static_cast<int64_t>(coord * 10000.0f);
}

inline float unquantizeCoord(int64_t coord) {
    return static_cast<float>(coord) / 10000.0f;
}

struct Parameter {
    int64_t min = 0;
    int64_t max = 0;

    static Parameter point(float value) {
        return span(value, value);
    }

    static Parameter span(float minValue, float maxValue) {
        if (minValue > maxValue) {
            throw std::invalid_argument("Climate::Parameter min > max");
        }
        return { quantizeCoord(minValue), quantizeCoord(maxValue) };
    }

    static Parameter span(Parameter minValue, Parameter maxValue) {
        if (minValue.min > maxValue.max) {
            throw std::invalid_argument("Climate::Parameter min > max");
        }
        return { minValue.min, maxValue.max };
    }

    int64_t distance(int64_t target) const {
        const int64_t above = target - max;
        const int64_t below = min - target;
        return above > 0 ? above : (below > 0 ? below : 0);
    }

    int64_t distance(Parameter target) const {
        const int64_t above = target.min - max;
        const int64_t below = min - target.max;
        return above > 0 ? above : (below > 0 ? below : 0);
    }

    Parameter span(Parameter other) const {
        return { min < other.min ? min : other.min, max > other.max ? max : other.max };
    }
};

struct TargetPoint {
    int64_t temperature = 0;
    int64_t humidity = 0;
    int64_t continentalness = 0;
    int64_t erosion = 0;
    int64_t depth = 0;
    int64_t weirdness = 0;
};

inline TargetPoint target(float temperature, float humidity, float continentalness,
                          float erosion, float depth, float weirdness) {
    return {
        quantizeCoord(temperature),
        quantizeCoord(humidity),
        quantizeCoord(continentalness),
        quantizeCoord(erosion),
        quantizeCoord(depth),
        quantizeCoord(weirdness)
    };
}

struct ParameterPoint {
    Parameter temperature;
    Parameter humidity;
    Parameter continentalness;
    Parameter erosion;
    Parameter depth;
    Parameter weirdness;
    int64_t offset = 0;

    int64_t fitness(const TargetPoint& targetPoint) const {
        auto square = [](int64_t value) {
            return value * value;
        };
        return square(temperature.distance(targetPoint.temperature))
            + square(humidity.distance(targetPoint.humidity))
            + square(continentalness.distance(targetPoint.continentalness))
            + square(erosion.distance(targetPoint.erosion))
            + square(depth.distance(targetPoint.depth))
            + square(weirdness.distance(targetPoint.weirdness))
            + square(offset);
    }
};

inline ParameterPoint parameters(Parameter temperature, Parameter humidity,
                                 Parameter continentalness, Parameter erosion,
                                 Parameter depth, Parameter weirdness, float offset) {
    return { temperature, humidity, continentalness, erosion, depth, weirdness, quantizeCoord(offset) };
}

inline ParameterPoint parameters(float temperature, float humidity, float continentalness,
                                 float erosion, float depth, float weirdness, float offset) {
    return parameters(Parameter::point(temperature), Parameter::point(humidity),
                      Parameter::point(continentalness), Parameter::point(erosion),
                      Parameter::point(depth), Parameter::point(weirdness), offset);
}

template <typename T>
class ParameterList {
public:
    using Entry = std::pair<ParameterPoint, T>;

    ParameterList() = default;
    explicit ParameterList(std::vector<Entry> values) : m_values(std::move(values)) {}

    const std::vector<Entry>& values() const {
        return m_values;
    }

    bool empty() const {
        return m_values.empty();
    }

    const T& findValue(const TargetPoint& targetPoint) const {
        if (m_values.empty()) {
            throw std::runtime_error("Climate::ParameterList needs at least one value");
        }

        const Entry* best = &m_values.front();
        int64_t bestFitness = best->first.fitness(targetPoint);
        for (const Entry& entry : m_values) {
            const int64_t fitness = entry.first.fitness(targetPoint);
            if (fitness < bestFitness) {
                bestFitness = fitness;
                best = &entry;
            }
        }
        return best->second;
    }

private:
    std::vector<Entry> m_values;
};

} // namespace mc::levelgen::Climate

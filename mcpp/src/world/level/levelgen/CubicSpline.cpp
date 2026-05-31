#include "CubicSpline.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace mc::levelgen {

namespace {
    float identity(float value) {
        return value;
    }

    float lerp(float alpha, float p0, float p1) {
        return p0 + alpha * (p1 - p0);
    }

    int findIntervalStart(const std::vector<float>& locations, float input) {
        int from = 0;
        int len = static_cast<int>(locations.size());
        while (len > 0) {
            int half = len / 2;
            int middle = from + half;
            if (input < locations[static_cast<size_t>(middle)]) {
                len = half;
            } else {
                from = middle + 1;
                len -= half + 1;
            }
        }
        return from - 1;
    }

    float linearExtend(float input, const std::vector<float>& locations, float value, const std::vector<float>& derivatives, int index) {
        float derivative = derivatives[static_cast<size_t>(index)];
        return derivative == 0.0f ? value : value + derivative * (input - locations[static_cast<size_t>(index)]);
    }

    class ConstantSpline final : public CubicSpline {
    public:
        explicit ConstantSpline(float value) : m_value(value) {}

        float apply(const DensityFunctionContext&) const override { return m_value; }
        float minValue() const override { return m_value; }
        float maxValue() const override { return m_value; }

    private:
        float m_value;
    };

    class MultipointSpline final : public CubicSpline {
    public:
        MultipointSpline(BoundedFloatFunctionPtr coordinate, std::vector<float> locations, std::vector<CubicSplinePtr> values, std::vector<float> derivatives)
            : m_coordinate(std::move(coordinate)),
              m_locations(std::move(locations)),
              m_values(std::move(values)),
              m_derivatives(std::move(derivatives)) {
            validateSizes();
            computeBounds();
        }

        float apply(const DensityFunctionContext& context) const override {
            float input = m_coordinate->apply(context);
            int start = findIntervalStart(m_locations, input);
            int lastIndex = static_cast<int>(m_locations.size()) - 1;
            if (start < 0) {
                return linearExtend(input, m_locations, m_values.front()->apply(context), m_derivatives, 0);
            }

            if (start == lastIndex) {
                return linearExtend(input, m_locations, m_values.back()->apply(context), m_derivatives, lastIndex);
            }

            float x1 = m_locations[static_cast<size_t>(start)];
            float x2 = m_locations[static_cast<size_t>(start + 1)];
            float t = (input - x1) / (x2 - x1);
            const CubicSplinePtr& f1 = m_values[static_cast<size_t>(start)];
            const CubicSplinePtr& f2 = m_values[static_cast<size_t>(start + 1)];
            float d1 = m_derivatives[static_cast<size_t>(start)];
            float d2 = m_derivatives[static_cast<size_t>(start + 1)];
            float y1 = f1->apply(context);
            float y2 = f2->apply(context);
            float a = d1 * (x2 - x1) - (y2 - y1);
            float b = -d2 * (x2 - x1) + (y2 - y1);
            return lerp(t, y1, y2) + t * (1.0f - t) * lerp(t, a, b);
        }

        float minValue() const override { return m_minValue; }
        float maxValue() const override { return m_maxValue; }

    private:
        void validateSizes() const {
            if (m_locations.size() != m_values.size() || m_locations.size() != m_derivatives.size()) {
                throw std::invalid_argument("All spline point arrays must have equal lengths");
            }
            if (m_locations.empty()) {
                throw std::invalid_argument("Cannot create a multipoint spline with no points");
            }
        }

        void computeBounds() {
            int lastIndex = static_cast<int>(m_locations.size()) - 1;
            m_minValue = std::numeric_limits<float>::infinity();
            m_maxValue = -std::numeric_limits<float>::infinity();
            float minInput = m_coordinate->minValue();
            float maxInput = m_coordinate->maxValue();
            if (minInput < m_locations.front()) {
                float edge1 = linearExtend(minInput, m_locations, m_values.front()->minValue(), m_derivatives, 0);
                float edge2 = linearExtend(minInput, m_locations, m_values.front()->maxValue(), m_derivatives, 0);
                m_minValue = std::min(m_minValue, std::min(edge1, edge2));
                m_maxValue = std::max(m_maxValue, std::max(edge1, edge2));
            }

            if (maxInput > m_locations.back()) {
                float edge1 = linearExtend(maxInput, m_locations, m_values.back()->minValue(), m_derivatives, lastIndex);
                float edge2 = linearExtend(maxInput, m_locations, m_values.back()->maxValue(), m_derivatives, lastIndex);
                m_minValue = std::min(m_minValue, std::min(edge1, edge2));
                m_maxValue = std::max(m_maxValue, std::max(edge1, edge2));
            }

            for (const CubicSplinePtr& value : m_values) {
                m_minValue = std::min(m_minValue, value->minValue());
                m_maxValue = std::max(m_maxValue, value->maxValue());
            }

            for (int i = 0; i < lastIndex; ++i) {
                float x1 = m_locations[static_cast<size_t>(i)];
                float x2 = m_locations[static_cast<size_t>(i + 1)];
                float xDiff = x2 - x1;
                const CubicSplinePtr& v1 = m_values[static_cast<size_t>(i)];
                const CubicSplinePtr& v2 = m_values[static_cast<size_t>(i + 1)];
                float min1 = v1->minValue();
                float max1 = v1->maxValue();
                float min2 = v2->minValue();
                float max2 = v2->maxValue();
                float d1 = m_derivatives[static_cast<size_t>(i)];
                float d2 = m_derivatives[static_cast<size_t>(i + 1)];
                if (d1 != 0.0f || d2 != 0.0f) {
                    float p1 = d1 * xDiff;
                    float p2 = d2 * xDiff;
                    float minLerp1 = std::min(min1, min2);
                    float maxLerp1 = std::max(max1, max2);
                    float minA = p1 - max2 + min1;
                    float maxA = p1 - min2 + max1;
                    float minB = -p2 + min2 - max1;
                    float maxB = -p2 + max2 - min1;
                    float minLerp2 = std::min(minA, minB);
                    float maxLerp2 = std::max(maxA, maxB);
                    m_minValue = std::min(m_minValue, minLerp1 + 0.25f * minLerp2);
                    m_maxValue = std::max(m_maxValue, maxLerp1 + 0.25f * maxLerp2);
                }
            }
        }

        BoundedFloatFunctionPtr m_coordinate;
        std::vector<float> m_locations;
        std::vector<CubicSplinePtr> m_values;
        std::vector<float> m_derivatives;
        float m_minValue = 0.0f;
        float m_maxValue = 0.0f;
    };

    class DensityCoordinateFunction final : public BoundedFloatFunction {
    public:
        explicit DensityCoordinateFunction(DensityFunctionPtr function) : m_function(std::move(function)) {}

        float apply(const DensityFunctionContext& context) const override {
            return static_cast<float>(m_function->compute(context));
        }

        float minValue() const override { return static_cast<float>(m_function->minValue()); }
        float maxValue() const override { return static_cast<float>(m_function->maxValue()); }

    private:
        DensityFunctionPtr m_function;
    };

    class SplineDensityFunction final : public DensityFunction {
    public:
        explicit SplineDensityFunction(CubicSplinePtr spline) : m_spline(std::move(spline)) {}

        double compute(const DensityFunctionContext& context) const override {
            return m_spline->apply(context);
        }

        double minValue() const override { return m_spline->minValue(); }
        double maxValue() const override { return m_spline->maxValue(); }

    private:
        CubicSplinePtr m_spline;
    };
}

CubicSpline::Builder::Builder(BoundedFloatFunctionPtr coordinate, ValueTransform valueTransform)
    : m_coordinate(std::move(coordinate)), m_valueTransform(std::move(valueTransform)) {
    if (!m_valueTransform) {
        m_valueTransform = identity;
    }
}

CubicSpline::Builder& CubicSpline::Builder::addPoint(float location, float value) {
    return addPoint(location, CubicSpline::constant(m_valueTransform(value)), 0.0f);
}

CubicSpline::Builder& CubicSpline::Builder::addPoint(float location, float value, float derivative) {
    return addPoint(location, CubicSpline::constant(m_valueTransform(value)), derivative);
}

CubicSpline::Builder& CubicSpline::Builder::addPoint(float location, CubicSplinePtr sampler) {
    return addPoint(location, std::move(sampler), 0.0f);
}

CubicSpline::Builder& CubicSpline::Builder::addPoint(float location, CubicSplinePtr sampler, float derivative) {
    if (!m_locations.empty() && location <= m_locations.back()) {
        throw std::invalid_argument("Please register spline points in ascending order");
    }

    m_locations.push_back(location);
    m_values.push_back(std::move(sampler));
    m_derivatives.push_back(derivative);
    return *this;
}

CubicSplinePtr CubicSpline::Builder::build() const {
    if (m_locations.empty()) {
        throw std::runtime_error("No spline elements added");
    }
    return std::make_shared<MultipointSpline>(m_coordinate, m_locations, m_values, m_derivatives);
}

CubicSplinePtr CubicSpline::constant(float value) {
    return std::make_shared<ConstantSpline>(value);
}

CubicSpline::Builder CubicSpline::builder(BoundedFloatFunctionPtr coordinate) {
    return Builder(std::move(coordinate), identity);
}

CubicSpline::Builder CubicSpline::builder(BoundedFloatFunctionPtr coordinate, ValueTransform valueTransform) {
    return Builder(std::move(coordinate), std::move(valueTransform));
}

BoundedFloatFunctionPtr densityCoordinate(DensityFunctionPtr function) {
    return std::make_shared<DensityCoordinateFunction>(std::move(function));
}

DensityFunctionPtr spline(CubicSplinePtr spline) {
    return std::make_shared<SplineDensityFunction>(std::move(spline));
}

} // namespace mc::levelgen

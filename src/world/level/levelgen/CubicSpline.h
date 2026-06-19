#pragma once

#include "DensityFunction.h"

#include <functional>
#include <memory>
#include <vector>

namespace mc::levelgen {

class BoundedFloatFunction {
public:
    virtual ~BoundedFloatFunction() = default;

    virtual float apply(const DensityFunctionContext& context) const = 0;
    virtual float minValue() const = 0;
    virtual float maxValue() const = 0;
};

using BoundedFloatFunctionPtr = std::shared_ptr<const BoundedFloatFunction>;

class CubicSpline;
using CubicSplinePtr = std::shared_ptr<const CubicSpline>;

class CubicSpline : public BoundedFloatFunction {
public:
    using ValueTransform = std::function<float(float)>;

    class Builder {
    public:
        Builder(BoundedFloatFunctionPtr coordinate, ValueTransform valueTransform);

        Builder& addPoint(float location, float value);
        Builder& addPoint(float location, float value, float derivative);
        Builder& addPoint(float location, CubicSplinePtr sampler);
        Builder& addPoint(float location, CubicSplinePtr sampler, float derivative);
        CubicSplinePtr build() const;

    private:
        BoundedFloatFunctionPtr m_coordinate;
        ValueTransform m_valueTransform;
        std::vector<float> m_locations;
        std::vector<CubicSplinePtr> m_values;
        std::vector<float> m_derivatives;
    };

    static CubicSplinePtr constant(float value);
    static Builder builder(BoundedFloatFunctionPtr coordinate);
    static Builder builder(BoundedFloatFunctionPtr coordinate, ValueTransform valueTransform);
};

BoundedFloatFunctionPtr densityCoordinate(DensityFunctionPtr function);
DensityFunctionPtr spline(CubicSplinePtr spline);

} // namespace mc::levelgen

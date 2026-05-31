#pragma once

#include "CubicSpline.h"

namespace mc::levelgen::TerrainProvider {

CubicSplinePtr overworldOffset(
    BoundedFloatFunctionPtr continents,
    BoundedFloatFunctionPtr erosion,
    BoundedFloatFunctionPtr ridges,
    bool amplified);

CubicSplinePtr overworldFactor(
    BoundedFloatFunctionPtr continents,
    BoundedFloatFunctionPtr erosion,
    BoundedFloatFunctionPtr weirdness,
    BoundedFloatFunctionPtr ridges,
    bool amplified);

CubicSplinePtr overworldJaggedness(
    BoundedFloatFunctionPtr continents,
    BoundedFloatFunctionPtr erosion,
    BoundedFloatFunctionPtr weirdness,
    BoundedFloatFunctionPtr ridges,
    bool amplified);

} // namespace mc::levelgen::TerrainProvider

#pragma once

#include "DensityFunction.h"

namespace mc::levelgen {

// Port of net.minecraft.world.level.levelgen.NoiseRouter.
struct NoiseRouter {
    DensityFunctionPtr barrierNoise;
    DensityFunctionPtr fluidLevelFloodednessNoise;
    DensityFunctionPtr fluidLevelSpreadNoise;
    DensityFunctionPtr lavaNoise;
    DensityFunctionPtr temperature;
    DensityFunctionPtr vegetation;
    DensityFunctionPtr continents;
    DensityFunctionPtr erosion;
    DensityFunctionPtr depth;
    DensityFunctionPtr ridges;
    DensityFunctionPtr preliminarySurfaceLevel;
    DensityFunctionPtr finalDensity;
    DensityFunctionPtr veinToggle;
    DensityFunctionPtr veinRidged;
    DensityFunctionPtr veinGap;
};

} // namespace mc::levelgen

#pragma once

#include "NoiseRouter.h"

namespace mc::levelgen {
class RandomState;
}

namespace mc::levelgen::NoiseRouterData {

NoiseRouter none();
NoiseRouter overworld(RandomState& randomState, bool largeBiomes = false, bool amplified = false);
NoiseRouter nether(RandomState& randomState);
NoiseRouter end(RandomState& randomState);

DensityFunctionPtr slide(
    DensityFunctionPtr caves,
    int minY,
    int height,
    int topStartY,
    int topEndY,
    double topTarget,
    int bottomStartY,
    int bottomEndY,
    double bottomTarget);

DensityFunctionPtr slideOverworld(bool amplified, DensityFunctionPtr caves);
DensityFunctionPtr slideNetherLike(DensityFunctionPtr base3dNoiseNether, int minY, int height);
DensityFunctionPtr slideEndLike(DensityFunctionPtr caves, int minY, int height);
DensityFunctionPtr postProcess(DensityFunctionPtr slide);

double peaksAndValleys(double weirdness);

} // namespace mc::levelgen::NoiseRouterData

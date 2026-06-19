#pragma once

#include "DensityFunction.h"
#include "NoiseGeneratorSettings.h"
#include "NoiseRouter.h"
#include "RandomSource.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace mc::levelgen {

class Aquifer {
public:
    struct FluidStatus {
        int fluidLevel = 0;
        uint32_t fluidType = 0;

        uint32_t at(int blockY, uint32_t air) const;
        bool operator==(const FluidStatus& other) const = default;
    };

    using FluidPicker = std::function<FluidStatus(int blockX, int blockY, int blockZ)>;
    using PreliminarySurfaceGetter = std::function<int(int blockX, int blockZ)>;

    virtual ~Aquifer() = default;
    virtual std::optional<uint32_t> computeSubstance(const DensityFunctionContext& context, double density) = 0;
    virtual bool shouldScheduleFluidUpdate() const = 0;

    static FluidPicker createFluidPicker(const NoiseGeneratorSettings& settings);
    static std::unique_ptr<Aquifer> createDisabled(FluidPicker fluidRule);
    static std::unique_ptr<Aquifer> create(
        PreliminarySurfaceGetter preliminarySurface,
        int chunkMinBlockX,
        int chunkMinBlockZ,
        const NoiseRouter& router,
        std::shared_ptr<PositionalRandomFactory> positionalRandomFactory,
        int minBlockY,
        int yBlockSize,
        FluidPicker fluidRule);
};

} // namespace mc::levelgen

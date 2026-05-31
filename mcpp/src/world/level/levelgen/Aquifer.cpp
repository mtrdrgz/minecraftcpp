#include "Aquifer.h"
#include "../block/Blocks.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace mc::levelgen {

namespace {
    constexpr int WAY_BELOW_MIN_Y = -32512;
    constexpr int INVALID_DISTANCE = std::numeric_limits<int>::max();

    uint32_t state(std::string_view name, uint32_t fallback = 0) {
        return getDefaultBlockStateId(name, fallback);
    }

    int floorDiv(int x, int y) {
        int q = x / y;
        int r = x % y;
        if (r != 0 && ((r < 0) != (y < 0))) {
            --q;
        }
        return q;
    }

    int floorToInt(double value) {
        return static_cast<int>(std::floor(value));
    }

    double clamp(double value, double minValue, double maxValue) {
        return std::max(minValue, std::min(maxValue, value));
    }

    double map(double value, double fromMin, double fromMax, double toMin, double toMax) {
        return toMin + (value - fromMin) * (toMax - toMin) / (fromMax - fromMin);
    }

    double clampedMap(double value, double fromMin, double fromMax, double toMin, double toMax) {
        return clamp(map(value, fromMin, fromMax, toMin, toMax), std::min(toMin, toMax), std::max(toMin, toMax));
    }

    int quantize(double value, int quantizeResolution) {
        return floorToInt(value / quantizeResolution) * quantizeResolution;
    }

    int gridX(int blockCoord) {
        return floorDiv(blockCoord, 16);
    }

    int fromGridX(int gridCoord, int blockOffset) {
        return gridCoord * 16 + blockOffset;
    }

    int gridY(int blockCoord) {
        return floorDiv(blockCoord, 12);
    }

    int fromGridY(int gridCoord, int blockOffset) {
        return gridCoord * 12 + blockOffset;
    }

    int gridZ(int blockCoord) {
        return floorDiv(blockCoord, 16);
    }

    int fromGridZ(int gridCoord, int blockOffset) {
        return gridCoord * 16 + blockOffset;
    }

    double similarity(int distanceSqr1, int distanceSqr2) {
        return 1.0 - (distanceSqr2 - distanceSqr1) / 25.0;
    }

    bool isDeepDarkRegion(const DensityFunctionPtr& erosion, const DensityFunctionPtr& depth, const DensityFunctionContext& context) {
        return erosion->compute(context) < -0.225 && depth->compute(context) > 0.9;
    }

    struct AquiferLocation {
        int x = 0;
        int y = 0;
        int z = 0;
        bool valid = false;
    };

    class DisabledAquifer final : public Aquifer {
    public:
        explicit DisabledAquifer(FluidPicker fluidRule)
            : m_fluidRule(std::move(fluidRule)) {
        }

        std::optional<uint32_t> computeSubstance(const DensityFunctionContext& context, double density) override {
            if (density > 0.0) {
                return std::nullopt;
            }
            return m_fluidRule(context.blockX, context.blockY, context.blockZ).at(context.blockY, m_air);
        }

        bool shouldScheduleFluidUpdate() const override {
            return false;
        }

    private:
        FluidPicker m_fluidRule;
        uint32_t m_air = state("air", 0);
    };

    class NoiseBasedAquifer final : public Aquifer {
    public:
        NoiseBasedAquifer(
            PreliminarySurfaceGetter preliminarySurface,
            int chunkMinBlockX,
            int chunkMinBlockZ,
            const NoiseRouter& router,
            std::shared_ptr<PositionalRandomFactory> positionalRandomFactory,
            int minBlockY,
            int yBlockSize,
            FluidPicker globalFluidPicker
        ) : m_preliminarySurface(std::move(preliminarySurface)),
            m_barrierNoise(router.barrierNoise),
            m_fluidLevelFloodednessNoise(router.fluidLevelFloodednessNoise),
            m_fluidLevelSpreadNoise(router.fluidLevelSpreadNoise),
            m_lavaNoise(router.lavaNoise),
            m_positionalRandomFactory(std::move(positionalRandomFactory)),
            m_globalFluidPicker(std::move(globalFluidPicker)),
            m_erosion(router.erosion),
            m_depth(router.depth),
            m_minGridX(gridX(chunkMinBlockX - 5)),
            m_minGridY(gridY(minBlockY + 1) - 1),
            m_minGridZ(gridZ(chunkMinBlockZ - 5)) {
            const int chunkMaxBlockX = chunkMinBlockX + 15;
            const int chunkMaxBlockZ = chunkMinBlockZ + 15;
            const int maxGridX = gridX(chunkMaxBlockX - 5) + 1;
            const int maxGridY = gridY(minBlockY + yBlockSize + 1) + 1;
            const int maxGridZ = gridZ(chunkMaxBlockZ - 5) + 1;
            m_gridSizeX = maxGridX - m_minGridX + 1;
            const int gridSizeY = maxGridY - m_minGridY + 1;
            m_gridSizeZ = maxGridZ - m_minGridZ + 1;
            const int totalGridSize = m_gridSizeX * gridSizeY * m_gridSizeZ;
            m_aquiferCache.resize(static_cast<size_t>(totalGridSize));
            m_aquiferLocationCache.resize(static_cast<size_t>(totalGridSize));

            int maxPreliminary = maxPreliminarySurfaceLevel(
                fromGridX(m_minGridX, 0),
                fromGridZ(m_minGridZ, 0),
                fromGridX(maxGridX, 9),
                fromGridZ(maxGridZ, 9));
            int skipSamplingAboveGridY = gridY(adjustSurfaceLevel(maxPreliminary) + 12) + 1;
            m_skipSamplingAboveY = fromGridY(skipSamplingAboveGridY, 11) - 1;
        }

        std::optional<uint32_t> computeSubstance(const DensityFunctionContext& context, double density) override {
            if (density > 0.0) {
                m_shouldScheduleFluidUpdate = false;
                return std::nullopt;
            }

            const int posX = context.blockX;
            const int posY = context.blockY;
            const int posZ = context.blockZ;
            FluidStatus globalFluid = m_globalFluidPicker(posX, posY, posZ);
            if (posY > m_skipSamplingAboveY) {
                m_shouldScheduleFluidUpdate = false;
                return globalFluid.at(posY, m_air);
            }

            if (globalFluid.at(posY, m_air) == m_lava) {
                m_shouldScheduleFluidUpdate = false;
                return m_lava;
            }

            const int xAnchor = gridX(posX - 5);
            const int yAnchor = gridY(posY + 1);
            const int zAnchor = gridZ(posZ - 5);
            int distanceSqr1 = INVALID_DISTANCE;
            int distanceSqr2 = INVALID_DISTANCE;
            int distanceSqr3 = INVALID_DISTANCE;
            int distanceSqr4 = INVALID_DISTANCE;
            int closestIndex1 = 0;
            int closestIndex2 = 0;
            int closestIndex3 = 0;
            int closestIndex4 = 0;

            for (int x1 = 0; x1 <= 1; ++x1) {
                for (int y1 = -1; y1 <= 1; ++y1) {
                    for (int z1 = 0; z1 <= 1; ++z1) {
                        const int spacedGridX = xAnchor + x1;
                        const int spacedGridY = yAnchor + y1;
                        const int spacedGridZ = zAnchor + z1;
                        const int index = getIndex(spacedGridX, spacedGridY, spacedGridZ);
                        AquiferLocation& location = m_aquiferLocationCache[static_cast<size_t>(index)];
                        if (!location.valid) {
                            std::shared_ptr<RandomSource> random = m_positionalRandomFactory->at(spacedGridX, spacedGridY, spacedGridZ);
                            location = AquiferLocation{
                                fromGridX(spacedGridX, random->nextInt(10)),
                                fromGridY(spacedGridY, random->nextInt(9)),
                                fromGridZ(spacedGridZ, random->nextInt(10)),
                                true
                            };
                        }

                        const int dx = location.x - posX;
                        const int dy = location.y - posY;
                        const int dz = location.z - posZ;
                        const int newDistance = dx * dx + dy * dy + dz * dz;
                        if (distanceSqr1 >= newDistance) {
                            closestIndex4 = closestIndex3;
                            closestIndex3 = closestIndex2;
                            closestIndex2 = closestIndex1;
                            closestIndex1 = index;
                            distanceSqr4 = distanceSqr3;
                            distanceSqr3 = distanceSqr2;
                            distanceSqr2 = distanceSqr1;
                            distanceSqr1 = newDistance;
                        } else if (distanceSqr2 >= newDistance) {
                            closestIndex4 = closestIndex3;
                            closestIndex3 = closestIndex2;
                            closestIndex2 = index;
                            distanceSqr4 = distanceSqr3;
                            distanceSqr3 = distanceSqr2;
                            distanceSqr2 = newDistance;
                        } else if (distanceSqr3 >= newDistance) {
                            closestIndex4 = closestIndex3;
                            closestIndex3 = index;
                            distanceSqr4 = distanceSqr3;
                            distanceSqr3 = newDistance;
                        } else if (distanceSqr4 >= newDistance) {
                            closestIndex4 = index;
                            distanceSqr4 = newDistance;
                        }
                    }
                }
            }

            FluidStatus closestStatus1 = getAquiferStatus(closestIndex1);
            const double similarity12 = similarity(distanceSqr1, distanceSqr2);
            const uint32_t fluidState = closestStatus1.at(posY, m_air);
            if (similarity12 <= 0.0) {
                if (similarity12 >= m_flowingUpdateSimilarity) {
                    FluidStatus closestStatus2 = getAquiferStatus(closestIndex2);
                    m_shouldScheduleFluidUpdate = !(closestStatus1 == closestStatus2);
                } else {
                    m_shouldScheduleFluidUpdate = false;
                }
                return fluidState;
            }

            if (fluidState == m_water && m_globalFluidPicker(posX, posY - 1, posZ).at(posY - 1, m_air) == m_lava) {
                m_shouldScheduleFluidUpdate = true;
                return fluidState;
            }

            std::optional<double> barrierNoiseValue;
            FluidStatus closestStatus2 = getAquiferStatus(closestIndex2);
            const double barrier12 = similarity12 * calculatePressure(context, barrierNoiseValue, closestStatus1, closestStatus2);
            if (density + barrier12 > 0.0) {
                m_shouldScheduleFluidUpdate = false;
                return std::nullopt;
            }

            FluidStatus closestStatus3 = getAquiferStatus(closestIndex3);
            const double similarity13 = similarity(distanceSqr1, distanceSqr3);
            if (similarity13 > 0.0) {
                const double barrier13 = similarity12 * similarity13 * calculatePressure(context, barrierNoiseValue, closestStatus1, closestStatus3);
                if (density + barrier13 > 0.0) {
                    m_shouldScheduleFluidUpdate = false;
                    return std::nullopt;
                }
            }

            const double similarity23 = similarity(distanceSqr2, distanceSqr3);
            if (similarity23 > 0.0) {
                const double barrier23 = similarity12 * similarity23 * calculatePressure(context, barrierNoiseValue, closestStatus2, closestStatus3);
                if (density + barrier23 > 0.0) {
                    m_shouldScheduleFluidUpdate = false;
                    return std::nullopt;
                }
            }

            const bool mayFlow12 = !(closestStatus1 == closestStatus2);
            const bool mayFlow23 = similarity23 >= m_flowingUpdateSimilarity && !(closestStatus2 == closestStatus3);
            const bool mayFlow13 = similarity13 >= m_flowingUpdateSimilarity && !(closestStatus1 == closestStatus3);
            if (!mayFlow12 && !mayFlow23 && !mayFlow13) {
                m_shouldScheduleFluidUpdate = similarity13 >= m_flowingUpdateSimilarity
                    && similarity(distanceSqr1, distanceSqr4) >= m_flowingUpdateSimilarity
                    && !(closestStatus1 == getAquiferStatus(closestIndex4));
            } else {
                m_shouldScheduleFluidUpdate = true;
            }

            return fluidState;
        }

        bool shouldScheduleFluidUpdate() const override {
            return m_shouldScheduleFluidUpdate;
        }

    private:
        PreliminarySurfaceGetter m_preliminarySurface;
        DensityFunctionPtr m_barrierNoise;
        DensityFunctionPtr m_fluidLevelFloodednessNoise;
        DensityFunctionPtr m_fluidLevelSpreadNoise;
        DensityFunctionPtr m_lavaNoise;
        std::shared_ptr<PositionalRandomFactory> m_positionalRandomFactory;
        std::vector<std::optional<FluidStatus>> m_aquiferCache;
        std::vector<AquiferLocation> m_aquiferLocationCache;
        FluidPicker m_globalFluidPicker;
        DensityFunctionPtr m_erosion;
        DensityFunctionPtr m_depth;
        bool m_shouldScheduleFluidUpdate = false;
        int m_skipSamplingAboveY = 0;
        int m_minGridX = 0;
        int m_minGridY = 0;
        int m_minGridZ = 0;
        int m_gridSizeX = 0;
        int m_gridSizeZ = 0;
        uint32_t m_air = state("air", 0);
        uint32_t m_water = state("water", 0);
        uint32_t m_lava = state("lava", 0);
        double m_flowingUpdateSimilarity = similarity(10 * 10, 12 * 12);
        static constexpr std::array<std::array<int, 2>, 13> m_surfaceSamplingOffsets{{
            {{0, 0}}, {{-2, -1}}, {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-3, 0}}, {{-2, 0}},
            {{-1, 0}}, {{1, 0}}, {{-2, 1}}, {{-1, 1}}, {{0, 1}}, {{1, 1}}
        }};

        int getIndex(int gridCoordX, int gridCoordY, int gridCoordZ) const {
            const int x = gridCoordX - m_minGridX;
            const int y = gridCoordY - m_minGridY;
            const int z = gridCoordZ - m_minGridZ;
            return (y * m_gridSizeZ + z) * m_gridSizeX + x;
        }

        int preliminarySurfaceLevel(int blockX, int blockZ) const {
            return m_preliminarySurface(blockX, blockZ);
        }

        int maxPreliminarySurfaceLevel(int minBlockX, int minBlockZ, int maxBlockX, int maxBlockZ) const {
            int maxY = std::numeric_limits<int>::min();
            for (int blockZ = minBlockZ; blockZ <= maxBlockZ; blockZ += 4) {
                for (int blockX = minBlockX; blockX <= maxBlockX; blockX += 4) {
                    maxY = std::max(maxY, preliminarySurfaceLevel(blockX, blockZ));
                }
            }
            return maxY;
        }

        int adjustSurfaceLevel(int preliminarySurfaceLevel) const {
            return preliminarySurfaceLevel + 8;
        }

        FluidStatus getAquiferStatus(int index) {
            std::optional<FluidStatus>& oldStatus = m_aquiferCache[static_cast<size_t>(index)];
            if (oldStatus) {
                return *oldStatus;
            }

            const AquiferLocation& location = m_aquiferLocationCache[static_cast<size_t>(index)];
            FluidStatus status = computeFluid(location.x, location.y, location.z);
            oldStatus = status;
            return status;
        }

        FluidStatus computeFluid(int x, int y, int z) {
            FluidStatus globalFluid = m_globalFluidPicker(x, y, z);
            int lowestPreliminarySurface = std::numeric_limits<int>::max();
            const int topOfAquiferCell = y + 12;
            const int bottomOfAquiferCell = y - 12;
            bool surfaceAtCenterIsUnderGlobalFluidLevel = false;

            for (const auto& offset : m_surfaceSamplingOffsets) {
                const int sampleX = x + offset[0] * 16;
                const int sampleZ = z + offset[1] * 16;
                const int preliminarySurface = preliminarySurfaceLevel(sampleX, sampleZ);
                const int adjustedSurface = adjustSurfaceLevel(preliminarySurface);
                const bool start = offset[0] == 0 && offset[1] == 0;
                if (start && bottomOfAquiferCell > adjustedSurface) {
                    return globalFluid;
                }

                const bool topPokesAboveSurface = topOfAquiferCell > adjustedSurface;
                if (topPokesAboveSurface || start) {
                    FluidStatus globalFluidAtSurface = m_globalFluidPicker(sampleX, adjustedSurface, sampleZ);
                    if (globalFluidAtSurface.at(adjustedSurface, m_air) != m_air) {
                        if (start) {
                            surfaceAtCenterIsUnderGlobalFluidLevel = true;
                        }
                        if (topPokesAboveSurface) {
                            return globalFluidAtSurface;
                        }
                    }
                }

                lowestPreliminarySurface = std::min(lowestPreliminarySurface, preliminarySurface);
            }

            const int fluidSurfaceLevel = computeSurfaceLevel(x, y, z, globalFluid, lowestPreliminarySurface, surfaceAtCenterIsUnderGlobalFluidLevel);
            return FluidStatus{ fluidSurfaceLevel, computeFluidType(x, y, z, globalFluid, fluidSurfaceLevel) };
        }

        int computeSurfaceLevel(
            int x,
            int y,
            int z,
            const FluidStatus& globalFluid,
            int lowestPreliminarySurface,
            bool surfaceAtCenterIsUnderGlobalFluidLevel
        ) const {
            DensityFunctionContext context{ x, y, z };
            double partiallyFloodedness = 0.0;
            double fullyFloodedness = 0.0;
            if (isDeepDarkRegion(m_erosion, m_depth, context)) {
                partiallyFloodedness = -1.0;
                fullyFloodedness = -1.0;
            } else {
                const int distanceBelowSurface = lowestPreliminarySurface + 8 - y;
                const double floodednessFactor = surfaceAtCenterIsUnderGlobalFluidLevel
                    ? clampedMap(distanceBelowSurface, 0.0, 64.0, 1.0, 0.0)
                    : 0.0;
                const double floodednessNoise = clamp(m_fluidLevelFloodednessNoise->compute(context), -1.0, 1.0);
                const double fullyFloodedThreshold = map(floodednessFactor, 1.0, 0.0, -0.3, 0.8);
                const double partiallyFloodedThreshold = map(floodednessFactor, 1.0, 0.0, -0.8, 0.4);
                partiallyFloodedness = floodednessNoise - partiallyFloodedThreshold;
                fullyFloodedness = floodednessNoise - fullyFloodedThreshold;
            }

            if (fullyFloodedness > 0.0) {
                return globalFluid.fluidLevel;
            }
            if (partiallyFloodedness > 0.0) {
                return computeRandomizedFluidSurfaceLevel(x, y, z, lowestPreliminarySurface);
            }
            return WAY_BELOW_MIN_Y;
        }

        int computeRandomizedFluidSurfaceLevel(int x, int y, int z, int lowestPreliminarySurface) const {
            const int fluidCellX = floorDiv(x, 16);
            const int fluidCellY = floorDiv(y, 40);
            const int fluidCellZ = floorDiv(z, 16);
            const int fluidCellMiddleY = fluidCellY * 40 + 20;
            const double fluidLevelSpread = m_fluidLevelSpreadNoise->compute(DensityFunctionContext{ fluidCellX, fluidCellY, fluidCellZ }) * 10.0;
            const int fluidLevelSpreadQuantized = quantize(fluidLevelSpread, 3);
            const int targetFluidSurfaceLevel = fluidCellMiddleY + fluidLevelSpreadQuantized;
            return std::min(lowestPreliminarySurface, targetFluidSurfaceLevel);
        }

        uint32_t computeFluidType(int x, int y, int z, const FluidStatus& globalFluid, int fluidSurfaceLevel) const {
            uint32_t fluidType = globalFluid.fluidType;
            if (fluidSurfaceLevel <= -10 && fluidSurfaceLevel != WAY_BELOW_MIN_Y && globalFluid.fluidType != m_lava) {
                const int fluidTypeCellX = floorDiv(x, 64);
                const int fluidTypeCellY = floorDiv(y, 40);
                const int fluidTypeCellZ = floorDiv(z, 64);
                const double lavaNoise = m_lavaNoise->compute(DensityFunctionContext{ fluidTypeCellX, fluidTypeCellY, fluidTypeCellZ });
                if (std::abs(lavaNoise) > 0.3) {
                    fluidType = m_lava;
                }
            }
            return fluidType;
        }

        double calculatePressure(
            const DensityFunctionContext& context,
            std::optional<double>& barrierNoiseValue,
            const FluidStatus& statusClosest1,
            const FluidStatus& statusClosest2
        ) const {
            const int posY = context.blockY;
            const uint32_t type1 = statusClosest1.at(posY, m_air);
            const uint32_t type2 = statusClosest2.at(posY, m_air);
            const bool lavaWater = (type1 == m_lava && type2 == m_water) || (type1 == m_water && type2 == m_lava);
            if (lavaWater) {
                return 2.0;
            }

            const int fluidYDiff = std::abs(statusClosest1.fluidLevel - statusClosest2.fluidLevel);
            if (fluidYDiff == 0) {
                return 0.0;
            }

            const double averageFluidY = 0.5 * (statusClosest1.fluidLevel + statusClosest2.fluidLevel);
            const double howFarAboveAverage = posY + 0.5 - averageFluidY;
            const double baseValue = fluidYDiff / 2.0;
            const double distanceFromBarrierEdgeTowardsMiddle = baseValue - std::abs(howFarAboveAverage);
            double gradient = 0.0;
            if (howFarAboveAverage > 0.0) {
                const double centerPoint = distanceFromBarrierEdgeTowardsMiddle;
                gradient = centerPoint > 0.0 ? centerPoint / 1.5 : centerPoint / 2.5;
            } else {
                const double centerPoint = 3.0 + distanceFromBarrierEdgeTowardsMiddle;
                gradient = centerPoint > 0.0 ? centerPoint / 3.0 : centerPoint / 10.0;
            }

            double noiseValue = 0.0;
            if (!(gradient < -2.0) && !(gradient > 2.0)) {
                if (!barrierNoiseValue) {
                    barrierNoiseValue = m_barrierNoise->compute(context);
                }
                noiseValue = *barrierNoiseValue;
            }

            return 2.0 * (noiseValue + gradient);
        }
    };
}

uint32_t Aquifer::FluidStatus::at(int blockY, uint32_t air) const {
    return blockY < fluidLevel ? fluidType : air;
}

Aquifer::FluidPicker Aquifer::createFluidPicker(const NoiseGeneratorSettings& settings) {
    const uint32_t lava = state("lava", 0);
    const FluidStatus lavaStatus{ -54, lava };
    const FluidStatus seaStatus{ settings.seaLevel, settings.defaultFluid };
    return [lavaStatus, seaStatus, seaLevel = settings.seaLevel](int, int y, int) {
        return y < std::min(-54, seaLevel) ? lavaStatus : seaStatus;
    };
}

std::unique_ptr<Aquifer> Aquifer::createDisabled(FluidPicker fluidRule) {
    return std::make_unique<DisabledAquifer>(std::move(fluidRule));
}

std::unique_ptr<Aquifer> Aquifer::create(
    PreliminarySurfaceGetter preliminarySurface,
    int chunkMinBlockX,
    int chunkMinBlockZ,
    const NoiseRouter& router,
    std::shared_ptr<PositionalRandomFactory> positionalRandomFactory,
    int minBlockY,
    int yBlockSize,
    FluidPicker fluidRule) {
    return std::make_unique<NoiseBasedAquifer>(
        std::move(preliminarySurface),
        chunkMinBlockX,
        chunkMinBlockZ,
        router,
        std::move(positionalRandomFactory),
        minBlockY,
        yBlockSize,
        std::move(fluidRule));
}

} // namespace mc::levelgen

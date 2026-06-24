#pragma once

// World-dependent placement modifiers that lift/scatter positions vertically:
//   HeightmapPlacement  - to the surface heightmap at (x,z)
//   HeightRangePlacement - to a HeightProvider-sampled Y
//   SurfaceRelativeThresholdFilter - keep origin if its Y is within
//       [surfaceY+min, surfaceY+max] of a heightmap surface
// Ports of net.minecraft.world.level.levelgen.placement.{HeightmapPlacement,
// HeightRangePlacement, SurfaceRelativeThresholdFilter}.

#include "../heightproviders/HeightProvider.h"
#include "PlacementContext.h"
#include "PlacementModifier.h"

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace mc::levelgen::placement {

using mc::levelgen::heightproviders::HeightProviderPtr;

class HeightmapPlacement final : public PlacementModifier {
public:
    explicit HeightmapPlacement(Heightmap::Types heightmap) : m_heightmap(heightmap) {}
    static std::shared_ptr<HeightmapPlacement> onHeightmap(Heightmap::Types h) {
        return std::make_shared<HeightmapPlacement>(h);
    }

    std::vector<BlockPos> getPositions(PlacementContext* context, RandomSource&, BlockPos origin) const override {
        const int x = origin.x;
        const int z = origin.z;
        const int height = context->getHeight(m_heightmap, x, z);
        if (height > context->getMinY()) {
            return { BlockPos{ x, height, z } };
        }
        return {};
    }

private:
    Heightmap::Types m_heightmap;
};

// SurfaceRelativeThresholdFilter.shouldPlace (SurfaceRelativeThresholdFilter.java:
// 35-40): surfaceY = context.getHeight(heightmap, x, z) (WorldGenRegion semantics:
// stored heightmap + 1); keep origin iff surfaceY+min <= origin.y <= surfaceY+max.
// The comparison is done in long in Java only to avoid overflow with the
// Integer.MIN/MAX_VALUE defaults; values here are small, int64 mirrors it.
class SurfaceRelativeThresholdFilter final : public PlacementModifier {
public:
    SurfaceRelativeThresholdFilter(Heightmap::Types heightmap, std::int64_t minInclusive, std::int64_t maxInclusive)
        : m_heightmap(heightmap), m_min(minInclusive), m_max(maxInclusive) {}

    std::vector<BlockPos> getPositions(PlacementContext* context, RandomSource&, BlockPos origin) const override {
        const std::int64_t surfaceY = context->getHeight(m_heightmap, origin.x, origin.z);
        if (surfaceY + m_min <= origin.y && origin.y <= surfaceY + m_max) {
            return { origin };
        }
        return {};
    }

private:
    Heightmap::Types m_heightmap;
    std::int64_t m_min, m_max;
};

// SurfaceWaterDepthFilter.shouldPlace (SurfaceWaterDepthFilter.java:30-35):
// keep origin iff getHeight(WORLD_SURFACE) - getHeight(OCEAN_FLOOR) <= max_water_depth
// (the water-column depth above the motion-blocking floor at origin's x/z).
class SurfaceWaterDepthFilter final : public PlacementModifier {
public:
    explicit SurfaceWaterDepthFilter(int maxWaterDepth) : m_maxWaterDepth(maxWaterDepth) {}

    std::vector<BlockPos> getPositions(PlacementContext* context, RandomSource&, BlockPos origin) const override {
        const int oceanFloor = context->getHeight(Heightmap::Types::OCEAN_FLOOR, origin.x, origin.z);
        const int worldSurface = context->getHeight(Heightmap::Types::WORLD_SURFACE, origin.x, origin.z);
        if (worldSurface - oceanFloor <= m_maxWaterDepth) {
            return { origin };
        }
        return {};
    }

private:
    int m_maxWaterDepth;
};

class HeightRangePlacement final : public PlacementModifier {
public:
    explicit HeightRangePlacement(HeightProviderPtr height) : m_height(std::move(height)) {}

    std::vector<BlockPos> getPositions(PlacementContext* context, RandomSource& random, BlockPos origin) const override {
        // origin.atY(height.sample(random, context))
        const int y = m_height->sample(random, *context);
        return { BlockPos{ origin.x, y, origin.z } };
    }

private:
    HeightProviderPtr m_height;
};

// EnvironmentScanPlacement.getPositions (EnvironmentScanPlacement.java:48-71):
// walk up/down from origin while allowed_search_condition holds, returning the
// first position passing target_condition; stepping outside the build height
// aborts; after max_steps the FINAL cursor gets one more target test. No RNG.
class EnvironmentScanPlacement final : public PlacementModifier {
public:
    EnvironmentScanPlacement(int directionY,
                             std::function<bool(WorldGenLevel&, BlockPos)> targetCondition,
                             std::function<bool(WorldGenLevel&, BlockPos)> allowedSearchCondition,
                             int maxSteps, int levelMinY, int levelMaxY)
        : m_directionY(directionY), m_target(std::move(targetCondition)),
          m_allowed(std::move(allowedSearchCondition)), m_maxSteps(maxSteps),
          m_levelMinY(levelMinY), m_levelMaxY(levelMaxY) {}

    std::vector<BlockPos> getPositions(PlacementContext* context, RandomSource&, BlockPos origin) const override {
        WorldGenLevel& level = *context->getLevel();
        BlockPos pos = origin;
        if (!m_allowed(level, pos)) {
            return {};
        }
        for (int i = 0; i < m_maxSteps; ++i) {
            if (m_target(level, pos)) {
                return { pos };
            }
            pos = BlockPos{ pos.x, pos.y + m_directionY, pos.z };
            if (pos.y < m_levelMinY || pos.y >= m_levelMaxY) {   // isOutsideBuildHeight
                return {};
            }
            if (!m_allowed(level, pos)) {
                break;
            }
        }
        if (m_target(level, pos)) {
            return { pos };
        }
        return {};
    }

private:
    int m_directionY;
    std::function<bool(WorldGenLevel&, BlockPos)> m_target;
    std::function<bool(WorldGenLevel&, BlockPos)> m_allowed;
    int m_maxSteps;
    int m_levelMinY, m_levelMaxY;
};

// CountOnEveryLayerPlacement (CountOnEveryLayerPlacement.java): for each layer
// from 0 upward, sample count positions; for each, find the Y at that layer by
// walking down from MOTION_BLOCKING height, counting non-empty→empty
// transitions. Repeat until a layer produces no positions.
class CountOnEveryLayerPlacement final : public PlacementModifier {
public:
    explicit CountOnEveryLayerPlacement(IntProviderPtr count) : m_count(std::move(count)) {}

    std::vector<BlockPos> getPositions(PlacementContext* ctx, RandomSource& random, BlockPos origin) const override {
        std::vector<BlockPos> positions;
        int layer = 0;
        bool foundAny;
        do {
            foundAny = false;
            const int n = m_count->sample(random);
            for (int i = 0; i < n; ++i) {
                const int x = random.nextInt(16) + origin.x;
                const int z = random.nextInt(16) + origin.z;
                const int startY = ctx->getHeight(Heightmap::Types::MOTION_BLOCKING, x, z);
                const int y = findOnGroundY(ctx, x, startY, z, layer);
                if (y != std::numeric_limits<int>::max()) {
                    positions.push_back(BlockPos{ x, y, z });
                    foundAny = true;
                }
            }
            ++layer;
        } while (foundAny);
        return positions;
    }

private:
    IntProviderPtr m_count;

    // CountOnEveryLayerPlacement.findOnGroundYPosition (java:65-84): walk down
    // from yStart; a "layer" is a transition where belowBlock is non-empty,
    // currentBlock is empty, and belowBlock is not bedrock. Return the Y above
    // the belowBlock at the requested layer, or INT_MAX.
    static int findOnGroundY(PlacementContext* ctx, int x, int yStart, int z, int layerToPlaceOn) {
        WorldGenLevel& level = *ctx->getLevel();
        std::string currentBlock = level.getBlockState(BlockPos{ x, yStart, z });
        int currentLayer = 0;
        for (int y = yStart; y >= ctx->getMinY() + 1; --y) {
            const std::string belowBlock = level.getBlockState(BlockPos{ x, y - 1, z });
            // isEmpty: isAir() || is(WATER) || is(LAVA)
            const bool belowEmpty = isStateEmpty(belowBlock);
            const bool currentEmpty = isStateEmpty(currentBlock);
            const bool belowIsBedrock = belowBlock == "minecraft:bedrock";
            if (!belowEmpty && currentEmpty && !belowIsBedrock) {
                if (currentLayer == layerToPlaceOn) {
                    return y;  // (y-1) + 1 = y
                }
                ++currentLayer;
            }
            currentBlock = belowBlock;
        }
        return std::numeric_limits<int>::max();
    }

    static bool isStateEmpty(const std::string& state) {
        return state == "minecraft:air" || state == "minecraft:cave_air" ||
               state == "minecraft:void_air" ||
               state == "minecraft:water" || state == "minecraft:lava";
    }
};

} // namespace mc::levelgen::placement

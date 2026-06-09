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

} // namespace mc::levelgen::placement

#pragma once

// World-dependent placement modifiers that lift/scatter positions vertically:
//   HeightmapPlacement  - to the surface heightmap at (x,z)
//   HeightRangePlacement - to a HeightProvider-sampled Y
// Ports of net.minecraft.world.level.levelgen.placement.{HeightmapPlacement,
// HeightRangePlacement}.

#include "../heightproviders/HeightProvider.h"
#include "PlacementContext.h"
#include "PlacementModifier.h"

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

#pragma once

// Port of net.minecraft.world.level.levelgen.placement.PlacementContext and the
// minimal WorldGenLevel surface that placement modifiers / features query.
// PlacementContext IS-A WorldGenerationContext (so HeightProviders resolve
// against its vertical bounds) and also forwards world queries to the level.

#include "../Heightmap.h"
#include "../WorldGenerationContext.h"
#include "../../../../core/Math.h"

namespace mc::levelgen::placement {

using mc::BlockPos;
using mc::levelgen::Heightmap;
using mc::levelgen::WorldGenerationContext;

// The world-access surface used during decoration. Only the methods needed so
// far are declared; getBlockState/setBlock/etc. are added with the features.
class WorldGenLevel {
public:
    virtual ~WorldGenLevel() = default;
    virtual int getHeight(Heightmap::Types type, int x, int z) const = 0;
    virtual int getMinY() const = 0;
};

class PlacementContext : public WorldGenerationContext {
public:
    PlacementContext(WorldGenLevel* level, int minGenY, int genDepth)
        : WorldGenerationContext(minGenY, genDepth), m_level(level) {}

    int getHeight(Heightmap::Types type, int x, int z) const { return m_level->getHeight(type, x, z); }
    int getMinY() const { return m_level->getMinY(); }
    WorldGenLevel* getLevel() const { return m_level; }

private:
    WorldGenLevel* m_level;
};

} // namespace mc::levelgen::placement

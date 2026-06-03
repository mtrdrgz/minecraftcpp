#pragma once

// Port of net.minecraft.world.level.levelgen.Heightmap.Types (worldgen-relevant
// surface heightmaps). Ordinals match Java (used as a stable index).

namespace mc::levelgen {

struct Heightmap {
    enum class Types {
        WORLD_SURFACE_WG,
        WORLD_SURFACE,
        OCEAN_FLOOR_WG,
        OCEAN_FLOOR,
        MOTION_BLOCKING,
        MOTION_BLOCKING_NO_LEAVES,
    };
};

} // namespace mc::levelgen

#pragma once

#include <cstdint>

namespace mc::levelgen {

// Port of net.minecraft.world.level.levelgen.NoiseSettings.
struct NoiseSettings {
    int minY = 0;
    int height = 0;
    int noiseSizeHorizontal = 1;
    int noiseSizeVertical = 1;

    static NoiseSettings create(int minY, int height, int noiseSizeHorizontal, int noiseSizeVertical);

    static NoiseSettings overworld();
    static NoiseSettings nether();
    static NoiseSettings end();
    static NoiseSettings caves();
    static NoiseSettings floatingIslands();

    int getCellHeight() const;
    int getCellWidth() const;
    NoiseSettings clampToHeightAccessor(int accessorMinY, int accessorMaxY) const;
};

} // namespace mc::levelgen

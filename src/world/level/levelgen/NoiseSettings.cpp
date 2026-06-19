#include "NoiseSettings.h"
#include <algorithm>
#include <stdexcept>

namespace mc::levelgen {

NoiseSettings NoiseSettings::create(int minY, int height, int noiseSizeHorizontal, int noiseSizeVertical) {
    if (minY + height > 2032) {
        throw std::invalid_argument("NoiseSettings: min_y + height cannot be higher than 2032");
    }
    if (height % 16 != 0) {
        throw std::invalid_argument("NoiseSettings: height has to be a multiple of 16");
    }
    if (minY % 16 != 0) {
        throw std::invalid_argument("NoiseSettings: min_y has to be a multiple of 16");
    }
    if (noiseSizeHorizontal < 1 || noiseSizeHorizontal > 4 || noiseSizeVertical < 1 || noiseSizeVertical > 4) {
        throw std::invalid_argument("NoiseSettings: noise sizes must be in [1, 4]");
    }
    return {minY, height, noiseSizeHorizontal, noiseSizeVertical};
}

NoiseSettings NoiseSettings::overworld() { return create(-64, 384, 1, 2); }
NoiseSettings NoiseSettings::nether() { return create(0, 128, 1, 2); }
NoiseSettings NoiseSettings::end() { return create(0, 128, 2, 1); }
NoiseSettings NoiseSettings::caves() { return create(-64, 192, 1, 2); }
NoiseSettings NoiseSettings::floatingIslands() { return create(0, 256, 2, 1); }

int NoiseSettings::getCellHeight() const {
    return noiseSizeVertical << 2;
}

int NoiseSettings::getCellWidth() const {
    return noiseSizeHorizontal << 2;
}

NoiseSettings NoiseSettings::clampToHeightAccessor(int accessorMinY, int accessorMaxY) const {
    int newMinY = std::max(minY, accessorMinY);
    int newHeight = std::min(minY + height, accessorMaxY + 1) - newMinY;
    return {newMinY, newHeight, noiseSizeHorizontal, noiseSizeVertical};
}

} // namespace mc::levelgen

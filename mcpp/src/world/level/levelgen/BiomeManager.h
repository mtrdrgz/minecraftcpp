#pragma once

#include "BiomeSource.h"
#include <array>
#include <cstdint>
#include <string>

namespace mc::levelgen {

class BiomeManager {
public:
    BiomeManager(const BiomeSource& noiseBiomeSource, int64_t seed);

    static int64_t obfuscateSeed(int64_t seed);
    static double debugGetFiddle(int64_t value);
    static double debugGetFiddledDistance(int64_t seed, int xRandom, int yRandom, int zRandom,
                                          double distanceX, double distanceY, double distanceZ);
    static std::array<int, 3> debugSelectQuart(int64_t biomeZoomSeed, int blockX, int blockY, int blockZ);

    std::string getBiome(int blockX, int blockY, int blockZ) const;
    std::string getNoiseBiomeAtQuart(int quartX, int quartY, int quartZ) const;

private:
    static double getFiddledDistance(int64_t seed, int xRandom, int yRandom, int zRandom,
                                     double distanceX, double distanceY, double distanceZ);
    static double getFiddle(int64_t value);

    const BiomeSource& m_noiseBiomeSource;
    int64_t m_biomeZoomSeed = 0;
};

} // namespace mc::levelgen

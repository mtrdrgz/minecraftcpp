#include "BiomeManager.h"

#include "../../../core/Sha256.h"

#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace mc::levelgen {
namespace {

constexpr uint64_t LCG_MULTIPLIER = 6364136223846793005ULL;
constexpr uint64_t LCG_INCREMENT = 1442695040888963407ULL;

uint64_t toUnsigned(int64_t value) {
    uint64_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

int64_t toSigned(uint64_t value) {
    int64_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

int64_t lcgNext(int64_t value, int64_t salt) {
    const uint64_t rval = toUnsigned(value);
    const uint64_t c = toUnsigned(salt);
    return toSigned(rval * (rval * LCG_MULTIPLIER + LCG_INCREMENT) + c);
}

int64_t floorMod(int64_t value, int64_t mod) {
    int64_t result = value % mod;
    return result < 0 ? result + mod : result;
}

std::array<unsigned char, 8> littleEndianBytes(int64_t value) {
    uint64_t raw = toUnsigned(value);
    std::array<unsigned char, 8> bytes{};
    for (int i = 0; i < 8; ++i) {
        bytes[i] = static_cast<unsigned char>((raw >> (i * 8)) & 0xffU);
    }
    return bytes;
}

int64_t littleEndianLong(const unsigned char* bytes) {
    uint64_t raw = 0;
    for (int i = 0; i < 8; ++i) {
        raw |= static_cast<uint64_t>(bytes[i]) << (i * 8);
    }
    return toSigned(raw);
}

} // namespace

BiomeManager::BiomeManager(const BiomeSource& noiseBiomeSource, int64_t seed)
    : m_noiseBiomeSource(noiseBiomeSource)
    , m_biomeZoomSeed(seed) {
}

int64_t BiomeManager::obfuscateSeed(int64_t seed) {
    // Java: Hashing.sha256().hashLong(seed) over the little-endian seed bytes,
    // then read the first 8 digest bytes little-endian.
    auto input = littleEndianBytes(seed);
    std::array<uint8_t, 32> digest = mc::core::sha256(input.data(), input.size());
    return littleEndianLong(digest.data());
}

std::string BiomeManager::getBiome(int blockX, int blockY, int blockZ) const {
    const std::array<int, 3> biomeQuart = debugSelectQuart(m_biomeZoomSeed, blockX, blockY, blockZ);
    return m_noiseBiomeSource.getNoiseBiome(biomeQuart[0], biomeQuart[1], biomeQuart[2]);
}

std::array<int, 3> BiomeManager::debugSelectQuart(int64_t biomeZoomSeed, int blockX, int blockY, int blockZ) {
    const int absX = blockX - 2;
    const int absY = blockY - 2;
    const int absZ = blockZ - 2;
    const int parentX = absX >> 2;
    const int parentY = absY >> 2;
    const int parentZ = absZ >> 2;
    const double fractX = (absX & 3) / 4.0;
    const double fractY = (absY & 3) / 4.0;
    const double fractZ = (absZ & 3) / 4.0;

    int minIndex = 0;
    double minDistance = INFINITY;
    for (int i = 0; i < 8; ++i) {
        const bool xEven = (i & 4) == 0;
        const bool yEven = (i & 2) == 0;
        const bool zEven = (i & 1) == 0;
        const int cornerX = xEven ? parentX : parentX + 1;
        const int cornerY = yEven ? parentY : parentY + 1;
        const int cornerZ = zEven ? parentZ : parentZ + 1;
        const double distanceX = xEven ? fractX : fractX - 1.0;
        const double distanceY = yEven ? fractY : fractY - 1.0;
        const double distanceZ = zEven ? fractZ : fractZ - 1.0;
        const double distance = getFiddledDistance(biomeZoomSeed, cornerX, cornerY, cornerZ,
                                                   distanceX, distanceY, distanceZ);
        if (minDistance > distance) {
            minIndex = i;
            minDistance = distance;
        }
    }

    const int biomeX = (minIndex & 4) == 0 ? parentX : parentX + 1;
    const int biomeY = (minIndex & 2) == 0 ? parentY : parentY + 1;
    const int biomeZ = (minIndex & 1) == 0 ? parentZ : parentZ + 1;
    return { biomeX, biomeY, biomeZ };
}

std::string BiomeManager::getNoiseBiomeAtQuart(int quartX, int quartY, int quartZ) const {
    return m_noiseBiomeSource.getNoiseBiome(quartX, quartY, quartZ);
}

double BiomeManager::getFiddledDistance(int64_t seed, int xRandom, int yRandom, int zRandom,
                                        double distanceX, double distanceY, double distanceZ) {
    int64_t value = seed;
    value = lcgNext(value, xRandom);
    value = lcgNext(value, yRandom);
    value = lcgNext(value, zRandom);
    value = lcgNext(value, xRandom);
    value = lcgNext(value, yRandom);
    value = lcgNext(value, zRandom);
    const double fiddleX = getFiddle(value);
    value = lcgNext(value, seed);
    const double fiddleY = getFiddle(value);
    value = lcgNext(value, seed);
    const double fiddleZ = getFiddle(value);
    return (distanceZ + fiddleZ) * (distanceZ + fiddleZ)
        + (distanceY + fiddleY) * (distanceY + fiddleY)
        + (distanceX + fiddleX) * (distanceX + fiddleX);
}

double BiomeManager::getFiddle(int64_t value) {
    const double uniform = floorMod(value >> 24, 1024) / 1024.0;
    return (uniform - 0.5) * 0.9;
}

double BiomeManager::debugGetFiddle(int64_t value) {
    return getFiddle(value);
}

double BiomeManager::debugGetFiddledDistance(int64_t seed, int xRandom, int yRandom, int zRandom,
                                             double distanceX, double distanceY, double distanceZ) {
    return getFiddledDistance(seed, xRandom, yRandom, zRandom, distanceX, distanceY, distanceZ);
}

} // namespace mc::levelgen

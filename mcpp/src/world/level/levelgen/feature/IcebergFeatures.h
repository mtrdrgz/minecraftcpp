#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.IcebergFeature and
// BlueIceFeature (26.1.2 sources). All writes are Feature.setBlock == flags 3
// (Feature.java:169-171) except BlueIceFeature's own flag-2 setBlock calls.
//
// IcebergFeature RNG order (place, :20-77):
//   nextDouble > 0.7 (snowOnTop), nextDouble*2PI (shapeAngle), nextInt(5)
//   (shapeEllipseA = 11 - .), nextInt(3) (shapeEllipseC = 3 + .), nextDouble > 0.7
//   (isEllipse), overWaterHeight = isEllipse ? nextInt(6)+6 : nextInt(15)+3;
//   !isEllipse && nextDouble > 0.9 -> += nextInt(19)+7;
//   underWaterHeight = min(overWaterHeight + nextInt(11), 18);
//   width = min(overWaterHeight + nextInt(7) - nextInt(5), 11)
//   ... then the per-cell draws inside heightDependentRadius*/signedDistanceCircle/
//   generateIcebergBlock/setIcebergBlock exactly as in the Java (each documented
//   at its function below).

#include "../placement/PlacementContext.h"
#include "../placement/PlacedFeature.h"
#include "../RandomSource.h"
#include "../Mth.h"
#include "../../../../core/Math.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

namespace iceberg_detail {

inline bool isIcebergState(const std::string& s) {
    return s == "minecraft:packed_ice" || s == "minecraft:snow_block" || s == "minecraft:blue_ice";
}

// IcebergFeature.signedDistanceCircle (:228-231): ONE nextFloat per call.
inline double signedDistanceCircle(int xo, int zo, BlockPos origin, int radius, RandomSource& random) {
    const float clamped = std::min(std::max(random.nextFloat(), 0.2f), 0.8f);   // Mth.clamp
    const float off = 10.0f * clamped / static_cast<float>(radius);
    return static_cast<double>(off)
        + std::pow(static_cast<double>(xo - origin.x), 2.0)
        + std::pow(static_cast<double>(zo - origin.z), 2.0)
        - std::pow(static_cast<double>(radius), 2.0);
}

// IcebergFeature.signedDistanceEllipse (:233-237): draw-free.
inline double signedDistanceEllipse(int xo, int zo, BlockPos origin, int a, int c, double angle) {
    return std::pow(((xo - origin.x) * std::cos(angle) - (zo - origin.z) * std::sin(angle)) / static_cast<double>(a), 2.0)
         + std::pow(((xo - origin.x) * std::sin(angle) + (zo - origin.z) * std::cos(angle)) / static_cast<double>(c), 2.0)
         - 1.0;
}

// IcebergFeature.heightDependentRadiusRound (:239-249): nextFloat, nextInt(5);
// nextInt(6) only inside the tall-iceberg arm.
inline int heightDependentRadiusRound(RandomSource& random, int yOff, int height, int width) {
    const float k = 3.5f - random.nextFloat();
    float scale = (1.0f - static_cast<float>(std::pow(yOff, 2.0)) / (static_cast<float>(height) * k)) * static_cast<float>(width);
    if (height > 15 + random.nextInt(5)) {
        const int tempYOff = yOff < 3 + random.nextInt(6) ? yOff / 2 : yOff;
        scale = (1.0f - static_cast<float>(tempYOff) / (static_cast<float>(height) * k * 0.4f)) * static_cast<float>(width);
    }
    return mth::ceil(scale / 2.0f);
}

// IcebergFeature.heightDependentRadiusEllipse (:251-255): draw-free.
inline int heightDependentRadiusEllipse(int yOff, int height, int width) {
    const float scale = (1.0f - static_cast<float>(std::pow(yOff, 2.0)) / (static_cast<float>(height) * 1.0f)) * static_cast<float>(width);
    return mth::ceil(scale / 2.0f);
}

// IcebergFeature.heightDependentRadiusSteep (:257-261): ONE nextFloat.
inline int heightDependentRadiusSteep(RandomSource& random, int yOff, int height, int width) {
    const float k = 1.0f + random.nextFloat() / 2.0f;
    const float scale = (1.0f - static_cast<float>(yOff) / (static_cast<float>(height) * k)) * static_cast<float>(width);
    return mth::ceil(scale / 2.0f);
}

// IcebergFeature.getEllipseC (:220-226).
inline int getEllipseC(int yOff, int height, int shapeEllipseC) {
    int c = shapeEllipseC;
    if (yOff > 0 && height - yOff <= 3) c -= 4 - (height - yOff);
    return c;
}

// IcebergFeature.setIcebergBlock (:188-218): the !isEllipse arm of `randomness`
// short-circuits (no draw); the snow-top nextInt fires only when snowOnTop and
// the cell is not water.
inline void setIcebergBlock(WorldGenLevel& level, RandomSource& random, BlockPos pos,
                            int hDiff, int height, bool isEllipse, bool snowOnTop,
                            const std::string& mainBlock) {
    const std::string state = level.getBlockState(pos);
    const bool isAir = state == "minecraft:air" || state == "minecraft:cave_air" || state == "minecraft:void_air";
    if (isAir || state == "minecraft:snow_block" || state == "minecraft:ice" || state == "minecraft:water") {
        const bool randomness = !isEllipse || random.nextDouble() > 0.05;
        const int divisor = isEllipse ? 3 : 2;
        if (snowOnTop && state != "minecraft:water"
            && static_cast<double>(hDiff) <= random.nextInt(std::max(1, height / divisor)) + height * 0.6
            && randomness) {
            level.setBlock(pos, "minecraft:snow_block", 3);
        } else {
            level.setBlock(pos, mainBlock, 3);
        }
    }
}

// IcebergFeature.generateIcebergBlock (:152-186).
inline void generateIcebergBlock(WorldGenLevel& level, RandomSource& random, BlockPos origin,
                                 int height, int xo, int yOff, int zo, int radius, int a,
                                 bool isEllipse, int shapeEllipseC, double shapeAngle,
                                 bool snowOnTop, const std::string& mainBlock) {
    const double signedDist = isEllipse
        ? signedDistanceEllipse(xo, zo, BlockPos{ 0, 0, 0 }, a, getEllipseC(yOff, height, shapeEllipseC), shapeAngle)
        : signedDistanceCircle(xo, zo, BlockPos{ 0, 0, 0 }, radius, random);
    if (signedDist < 0.0) {
        const BlockPos pos{ origin.x + xo, origin.y + yOff, origin.z + zo };
        const double compareVal = isEllipse ? -0.5 : static_cast<double>(-6 - random.nextInt(3));
        if (signedDist > compareVal && random.nextDouble() > 0.9) {
            return;
        }
        setIcebergBlock(level, random, pos, height - yOff, height, isEllipse, snowOnTop, mainBlock);
    }
}

// IcebergFeature.removeFloatingSnowLayer (:146-150).
inline void removeFloatingSnowLayer(WorldGenLevel& level, BlockPos pos) {
    if (level.getBlockState(BlockPos{ pos.x, pos.y + 1, pos.z }) == "minecraft:snow") {
        level.setBlock(BlockPos{ pos.x, pos.y + 1, pos.z }, "minecraft:air", 3);
    }
}

// IcebergFeature.carve (:113-144): draw-free.
inline void carve(int radius, int yOff, BlockPos globalOrigin, WorldGenLevel& level, bool underWater,
                  double angle, BlockPos localOrigin, int shapeEllipseA, int shapeEllipseC) {
    const int a = radius + 1 + shapeEllipseA / 3;
    const int c = std::min(radius - 3, 3) + shapeEllipseC / 2 - 1;
    for (int xo = -a; xo < a; ++xo) {
        for (int zo = -a; zo < a; ++zo) {
            const double signedDist = signedDistanceEllipse(xo, zo, localOrigin, a, c, angle);
            if (signedDist < 0.0) {
                const BlockPos pos{ globalOrigin.x + xo, globalOrigin.y + yOff, globalOrigin.z + zo };
                const std::string state = level.getBlockState(pos);
                if (isIcebergState(state) || state == "minecraft:snow_block") {
                    if (underWater) {
                        level.setBlock(pos, "minecraft:water", 3);
                    } else {
                        level.setBlock(pos, "minecraft:air", 3);
                        removeFloatingSnowLayer(level, pos);
                    }
                }
            }
        }
    }
}

// IcebergFeature.generateCutOut (:79-111).
inline void generateCutOut(RandomSource& random, WorldGenLevel& level, int width, int height,
                           BlockPos globalOrigin, bool isEllipse, int shapeEllipseA,
                           double shapeAngle, int shapeEllipseC) {
    const int randomSignX = random.nextBoolean() ? -1 : 1;
    const int randomSignZ = random.nextBoolean() ? -1 : 1;
    int xOff = random.nextInt(std::max(width / 2 - 2, 1));
    if (random.nextBoolean()) {
        xOff = width / 2 + 1 - random.nextInt(std::max(width - width / 2 - 1, 1));
    }
    int zOff = random.nextInt(std::max(width / 2 - 2, 1));
    if (random.nextBoolean()) {
        zOff = width / 2 + 1 - random.nextInt(std::max(width - width / 2 - 1, 1));
    }
    if (isEllipse) {
        xOff = zOff = random.nextInt(std::max(shapeEllipseA - 5, 1));
    }
    const BlockPos localOrigin{ randomSignX * xOff, 0, randomSignZ * zOff };
    const double angle = isEllipse ? shapeAngle + 1.5707963267948966 : random.nextDouble() * 2.0 * 3.141592653589793;
    for (int yOff = 0; yOff < height - 3; ++yOff) {
        const int radius = heightDependentRadiusRound(random, yOff, height, width);
        carve(radius, yOff, globalOrigin, level, false, angle, localOrigin, shapeEllipseA, shapeEllipseC);
    }
    for (int yOff = -1; yOff > -height + random.nextInt(5); --yOff) {
        const int radius = heightDependentRadiusSteep(random, -yOff, height, width);
        carve(radius, yOff, globalOrigin, level, true, angle, localOrigin, shapeEllipseA, shapeEllipseC);
    }
}

// IcebergFeature.smooth (:267-303): draw-free.
inline void smooth(WorldGenLevel& level, const std::function<bool(const std::string&)>& isAir,
                   BlockPos origin, int width, int height, bool isEllipse, int shapeEllipseA) {
    const int a = isEllipse ? shapeEllipseA : width / 2;
    for (int x = -a; x <= a; ++x) {
        for (int z = -a; z <= a; ++z) {
            for (int yOff = 0; yOff <= height; ++yOff) {
                const BlockPos pos{ origin.x + x, origin.y + yOff, origin.z + z };
                const std::string state = level.getBlockState(pos);
                if (isIcebergState(state) || state == "minecraft:snow") {
                    if (isAir(level.getBlockState(BlockPos{ pos.x, pos.y - 1, pos.z }))) {
                        level.setBlock(pos, "minecraft:air", 3);
                        level.setBlock(BlockPos{ pos.x, pos.y + 1, pos.z }, "minecraft:air", 3);
                    } else if (isIcebergState(state)) {
                        const std::string sides[4] = {
                            level.getBlockState(BlockPos{ pos.x - 1, pos.y, pos.z }),   // west
                            level.getBlockState(BlockPos{ pos.x + 1, pos.y, pos.z }),   // east
                            level.getBlockState(BlockPos{ pos.x, pos.y, pos.z - 1 }),   // north
                            level.getBlockState(BlockPos{ pos.x, pos.y, pos.z + 1 }),   // south
                        };
                        int counter = 0;
                        for (const std::string& side : sides) {
                            if (!isIcebergState(side)) ++counter;
                        }
                        if (counter >= 3) {
                            level.setBlock(pos, "minecraft:air", 3);
                        }
                    }
                }
            }
        }
    }
}

} // namespace iceberg_detail

// IcebergFeature.place (:20-77). seaLevel = chunkGenerator.getSeaLevel() (63).
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeIcebergPlacer(
        std::string mainBlock, int seaLevel, std::function<bool(const std::string&)> isAir) {
    return [mainBlock = std::move(mainBlock), seaLevel, isAir = std::move(isAir)](
               WorldGenLevel& level, RandomSource& random, BlockPos rawOrigin) -> bool {
        using namespace iceberg_detail;
        const BlockPos origin{ rawOrigin.x, seaLevel, rawOrigin.z };
        const bool snowOnTop = random.nextDouble() > 0.7;
        const double shapeAngle = random.nextDouble() * 2.0 * 3.141592653589793;
        const int shapeEllipseA = 11 - random.nextInt(5);
        const int shapeEllipseC = 3 + random.nextInt(3);
        const bool isEllipse = random.nextDouble() > 0.7;
        int overWaterHeight = isEllipse ? random.nextInt(6) + 6 : random.nextInt(15) + 3;
        if (!isEllipse && random.nextDouble() > 0.9) {
            overWaterHeight += random.nextInt(19) + 7;
        }
        const int underWaterHeight = std::min(overWaterHeight + random.nextInt(11), 18);
        const int width = std::min(overWaterHeight + random.nextInt(7) - random.nextInt(5), 11);
        const int a = isEllipse ? shapeEllipseA : 11;
        for (int xo = -a; xo < a; ++xo) {
            for (int zo = -a; zo < a; ++zo) {
                for (int yOff = 0; yOff < overWaterHeight; ++yOff) {
                    const int radius = isEllipse
                        ? heightDependentRadiusEllipse(yOff, overWaterHeight, width)
                        : heightDependentRadiusRound(random, yOff, overWaterHeight, width);
                    if (isEllipse || xo < radius) {
                        generateIcebergBlock(level, random, origin, overWaterHeight, xo, yOff, zo,
                                             radius, a, isEllipse, shapeEllipseC, shapeAngle, snowOnTop, mainBlock);
                    }
                }
            }
        }
        smooth(level, isAir, origin, width, overWaterHeight, isEllipse, shapeEllipseA);
        for (int xo = -a; xo < a; ++xo) {
            for (int zo = -a; zo < a; ++zo) {
                for (int yOff = -1; yOff > -underWaterHeight; --yOff) {
                    const int newA = isEllipse
                        ? mth::ceil(static_cast<float>(a) * (1.0f - static_cast<float>(std::pow(yOff, 2.0)) / (static_cast<float>(underWaterHeight) * 8.0f)))
                        : a;
                    const int radius = heightDependentRadiusSteep(random, -yOff, underWaterHeight, width);
                    if (xo < radius) {
                        generateIcebergBlock(level, random, origin, underWaterHeight, xo, yOff, zo,
                                             radius, newA, isEllipse, shapeEllipseC, shapeAngle, snowOnTop, mainBlock);
                    }
                }
            }
        }
        const bool doCutOut = isEllipse ? random.nextDouble() > 0.1 : random.nextDouble() > 0.7;
        if (doCutOut) {
            generateCutOut(random, level, width, overWaterHeight, origin, isEllipse, shapeEllipseA, shapeAngle, shapeEllipseC);
        }
        return true;
    };
}

// BlueIceFeature.place (BlueIceFeature.java:19-72).
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeBlueIcePlacer(
        int seaLevel, std::function<bool(const std::string&)> isAir) {
    return [seaLevel, isAir = std::move(isAir)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        if (origin.y > seaLevel - 1) return false;
        if (level.getBlockState(origin) != "minecraft:water"
            && level.getBlockState(BlockPos{ origin.x, origin.y - 1, origin.z }) != "minecraft:water") {
            return false;
        }
        bool foundPackedIce = false;
        for (int dir = 1; dir < 6; ++dir) {   // Direction.values() minus DOWN(0)
            static constexpr int DX[6] = { 0, 0, 0, 0, -1, 1 };
            static constexpr int DY[6] = { -1, 1, 0, 0, 0, 0 };
            static constexpr int DZ[6] = { 0, 0, -1, 1, 0, 0 };
            if (level.getBlockState(BlockPos{ origin.x + DX[dir], origin.y + DY[dir], origin.z + DZ[dir] }) == "minecraft:packed_ice") {
                foundPackedIce = true;
                break;
            }
        }
        if (!foundPackedIce) return false;
        level.setBlock(origin, "minecraft:blue_ice", 2);
        for (int i = 0; i < 200; ++i) {
            const int yOff = random.nextInt(5) - random.nextInt(6);
            int xzDiff = 3;
            if (yOff < 2) xzDiff += yOff / 2;
            if (xzDiff >= 1) {
                const BlockPos placePos{ origin.x + random.nextInt(xzDiff) - random.nextInt(xzDiff),
                                         origin.y + yOff,
                                         origin.z + random.nextInt(xzDiff) - random.nextInt(xzDiff) };
                const std::string placeState = level.getBlockState(placePos);
                if (isAir(placeState) || placeState == "minecraft:water"
                    || placeState == "minecraft:packed_ice" || placeState == "minecraft:ice") {
                    for (int dir = 0; dir < 6; ++dir) {   // Direction.values()
                        static constexpr int DX[6] = { 0, 0, 0, 0, -1, 1 };
                        static constexpr int DY[6] = { -1, 1, 0, 0, 0, 0 };
                        static constexpr int DZ[6] = { 0, 0, -1, 1, 0, 0 };
                        const std::string rel = level.getBlockState(
                            BlockPos{ placePos.x + DX[dir], placePos.y + DY[dir], placePos.z + DZ[dir] });
                        if (rel == "minecraft:blue_ice") {
                            level.setBlock(placePos, "minecraft:blue_ice", 2);
                            break;
                        }
                    }
                }
            }
        }
        return true;
    };
}

} // namespace mc::levelgen::feature

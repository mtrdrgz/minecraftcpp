#pragma once

// 1:1 ports of the dripstone worldgen features:
//   PointedDripstoneFeature  ("pointed_dripstone")
//   LargeDripstoneFeature    ("large_dripstone")
//   DripstoneClusterFeature  ("dripstone_cluster")
// plus their shared helpers DripstoneUtils.java and levelgen/Column.java.
//
// Block-id model notes: pointed dripstone TIP_DIRECTION/THICKNESS/WATERLOGGED
// are properties (id-invisible); growPointedDripstone's WATERLOGGED setValue
// (DripstoneUtils.java:81-88) replaces water cells with the pointed_dripstone id.
//
// RNG orders are documented inline per method, each against the Java lines.

#include "TreeFeature.h"      // WorldGenLevel, BlockPos, treeRelative
#include "../FloatProvider.h"
#include "../IntProvider.h"
#include "../Mth.h"
#include "../Heightmap.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>

namespace mc::levelgen::feature {

// ---------------------------------------------------------------------------
// Hooks: the state predicates DripstoneUtils needs (tags + ids).
struct DripstoneHooks {
    std::function<bool(const std::string&)> isAir;                   // BlockStateBase.isAir
    std::function<bool(const std::string&)> dripstoneReplaceable;    // #dripstone_replaceable_blocks
    std::function<bool(const std::string&)> baseStoneOverworld;      // #base_stone_overworld
    std::function<bool(const std::string&)> isWaterFluid;            // getFluidState().is(FluidTags.WATER)
};

namespace dripstone_detail {

// DripstoneUtils.isEmptyOrWater / isEmptyOrWaterOrLava (DripstoneUtils.java:117-127).
inline bool isEmptyOrWater(const DripstoneHooks& h, const std::string& s) {
    return h.isAir(s) || s == "minecraft:water";
}
inline bool isEmptyOrWaterOrLava(const DripstoneHooks& h, const std::string& s) {
    return h.isAir(s) || s == "minecraft:water" || s == "minecraft:lava";
}
// DripstoneUtils.isDripstoneBase (:113-115) / isDripstoneBaseOrLava (:109-111).
inline bool isDripstoneBase(const DripstoneHooks& h, const std::string& s) {
    return s == "minecraft:dripstone_block" || h.dripstoneReplaceable(s);
}
inline bool isDripstoneBaseOrLava(const DripstoneHooks& h, const std::string& s) {
    return isDripstoneBase(h, s) || s == "minecraft:lava";
}

// DripstoneUtils.getDripstoneHeight (DripstoneUtils.java:16-29).
inline double getDripstoneHeight(double xzDistanceFromCenter, double dripstoneRadius, double scale, double bluntness) {
    if (xzDistanceFromCenter < bluntness) xzDistanceFromCenter = bluntness;
    const double r = xzDistanceFromCenter / dripstoneRadius * 0.384;
    const double part1 = 0.75 * std::pow(r, 1.3333333333333333);
    const double part2 = std::pow(r, 0.6666666666666666);
    const double part3 = 0.3333333333333333 * std::log(r);
    double heightRelativeToMaxRadius = scale * (part1 - part2 - part3);
    heightRelativeToMaxRadius = std::max(heightRelativeToMaxRadius, 0.0);
    return heightRelativeToMaxRadius / 0.384 * dripstoneRadius;
}

// DripstoneUtils.isCircleMostlyEmbeddedInStone (:31-48): table-based Mth.cos/sin.
inline bool isCircleMostlyEmbeddedInStone(const DripstoneHooks& h, WorldGenLevel& level, BlockPos center, int xzRadius) {
    if (isEmptyOrWaterOrLava(h, level.getBlockState(center))) return false;
    const float angleIncrement = 6.0f / static_cast<float>(xzRadius);
    // (float)(Math.PI * 2)
    for (float angle = 0.0f; angle < static_cast<float>(3.141592653589793 * 2.0); angle += angleIncrement) {
        const int dx = static_cast<int>(mc::levelgen::mth::cos(angle) * static_cast<float>(xzRadius));
        const int dz = static_cast<int>(mc::levelgen::mth::sin(angle) * static_cast<float>(xzRadius));
        if (isEmptyOrWaterOrLava(h, level.getBlockState(BlockPos{ center.x + dx, center.y, center.z + dz }))) {
            return false;
        }
    }
    return true;
}

// DripstoneUtils.placeDripstoneBlockIfPossible (:92-100).
inline bool placeDripstoneBlockIfPossible(const DripstoneHooks& h, WorldGenLevel& level, BlockPos pos) {
    if (h.dripstoneReplaceable(level.getBlockState(pos))) {
        level.setBlock(pos, "minecraft:dripstone_block", 2);
        return true;
    }
    return false;
}

// DripstoneUtils.growPointedDripstone (:76-90) over buildBaseToTipColumn (:58-74).
// The column emits totalLength pointed_dripstone states (thickness/waterlogged are
// id-invisible), gated on isDripstoneBase behind the start.
inline void growPointedDripstone(const DripstoneHooks& h, WorldGenLevel& level,
                                 BlockPos startPos, int tipDirection /*0=DOWN,1=UP*/,
                                 int height, bool /*mergedTip: property only*/) {
    const int behind = tipDirection == 1 ? 0 : 1;   // tipDirection.getOpposite()
    if (!isDripstoneBase(h, level.getBlockState(treeRelative(startPos, behind)))) return;
    BlockPos pos = startPos;
    auto emit = [&]() {
        level.setBlock(pos, "minecraft:pointed_dripstone", 2);
        pos = treeRelative(pos, tipDirection);
    };
    // buildBaseToTipColumn: BASE, MIDDLE x (len-3), FRUSTUM, TIP/TIP_MERGE.
    if (height >= 3) { emit(); for (int i = 0; i < height - 3; ++i) emit(); }
    if (height >= 2) emit();
    if (height >= 1) emit();
}

// levelgen/Column.scan (Column.java:64-98) — floor/ceiling of the open column.
struct ColumnRange {
    std::optional<int> floor, ceiling;
    std::optional<int> height() const {
        if (floor.has_value() && ceiling.has_value()) return *ceiling - *floor - 1;
        return std::nullopt;
    }
};
inline std::optional<ColumnRange> columnScan(
        WorldGenLevel& level, BlockPos pos, int searchRange,
        const std::function<bool(const std::string&)>& insideColumn,
        const std::function<bool(const std::string&)>& validEdge) {
    if (!insideColumn(level.getBlockState(pos))) return std::nullopt;
    auto scanDirection = [&](int dirY) -> std::optional<int> {
        BlockPos p{ pos.x, pos.y, pos.z };
        for (int i = 1; i < searchRange && insideColumn(level.getBlockState(p)); ++i) {
            p = BlockPos{ p.x, p.y + dirY, p.z };
        }
        if (validEdge(level.getBlockState(p))) return p.y;
        return std::nullopt;
    };
    ColumnRange r;
    r.ceiling = scanDirection(+1);
    r.floor = scanDirection(-1);
    return r;
}

} // namespace dripstone_detail

// ---------------------------------------------------------------------------
// PointedDripstoneFeature.place (PointedDripstoneFeature.java:17-34).
// RNG: getTipDirection draws nextBoolean ONLY when base above AND below (:36-46);
// createPatchOfDripstoneBlocks (:48-67): per horizontal direction one nextFloat;
// passing arms draw nextFloat (radius2) and Direction.getRandom (nextInt(6)),
// then optionally nextFloat (radius3) + nextInt(6); then one nextFloat for the
// taller-dripstone chance (:29) gated && isEmptyOrWater(tipward).
struct PointedDripstoneConfig {
    float chanceOfTallerDripstone = 0.2f;
    float chanceOfDirectionalSpread = 0.7f;
    float chanceOfSpreadRadius2 = 0.5f;
    float chanceOfSpreadRadius3 = 0.5f;
};

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makePointedDripstonePlacer(
        PointedDripstoneConfig config, std::shared_ptr<const DripstoneHooks> hooks) {
    return [config, hooks = std::move(hooks)](WorldGenLevel& level, RandomSource& random, BlockPos pos) -> bool {
        using namespace dripstone_detail;
        const DripstoneHooks& h = *hooks;
        // getTipDirection
        const bool canPlaceAbove = isDripstoneBase(h, level.getBlockState(BlockPos{ pos.x, pos.y + 1, pos.z }));
        const bool canPlaceBelow = isDripstoneBase(h, level.getBlockState(BlockPos{ pos.x, pos.y - 1, pos.z }));
        int tipDirection;   // 0=DOWN, 1=UP
        if (canPlaceAbove && canPlaceBelow) tipDirection = random.nextBoolean() ? 0 : 1;
        else if (canPlaceAbove) tipDirection = 0;
        else if (canPlaceBelow) tipDirection = 1;
        else return false;
        const int rootDir = tipDirection == 1 ? 0 : 1;
        const BlockPos rootPos = treeRelative(pos, rootDir);
        // createPatchOfDripstoneBlocks
        placeDripstoneBlockIfPossible(h, level, rootPos);
        static constexpr int HORIZONTAL[4] = { 2, 5, 3, 4 };   // NORTH, EAST, SOUTH, WEST
        for (int d : HORIZONTAL) {
            if (!(random.nextFloat() > config.chanceOfDirectionalSpread)) {
                const BlockPos pos1 = treeRelative(rootPos, d);
                placeDripstoneBlockIfPossible(h, level, pos1);
                if (!(random.nextFloat() > config.chanceOfSpreadRadius2)) {
                    // Direction.getRandom(random): Util.getRandom(values[6])
                    const BlockPos pos2 = treeRelative(pos1, random.nextInt(6));
                    placeDripstoneBlockIfPossible(h, level, pos2);
                    if (!(random.nextFloat() > config.chanceOfSpreadRadius3)) {
                        const BlockPos pos3 = treeRelative(pos2, random.nextInt(6));
                        placeDripstoneBlockIfPossible(h, level, pos3);
                    }
                }
            }
        }
        const int height = random.nextFloat() < config.chanceOfTallerDripstone
                           && isEmptyOrWater(h, level.getBlockState(treeRelative(pos, tipDirection))) ? 2 : 1;
        growPointedDripstone(h, level, pos, tipDirection, height, false);
        return true;
    };
}

// ---------------------------------------------------------------------------
// LargeDripstoneFeature (LargeDripstoneFeature.java).
// RNG: radius randomBetweenInclusive (:47); stalactite bluntness+heightScale
// samples (:48-50), stalagmite likewise (:51-53); wind (when both suitable):
// windSpeed.sample + randomBetween(0, PI) (:201-205); placeBlocks per coVered
// column-cell with height>0 one nextFloat, on <0.2 one extra randomBetween
// (:166-168).
struct LargeDripstoneConfig {
    int floorToCeilingSearchRange = 30;
    int columnRadiusMin = 3, columnRadiusMax = 19;
    mc::valueproviders::FloatProviderPtr heightScale, windSpeed, stalactiteBluntness, stalagmiteBluntness;
    float maxColumnRadiusToCaveHeightRatio = 0.33f;
    int minRadiusForWind = 4;
    float minBluntnessForWind = 0.6f;
};

namespace dripstone_detail {

struct LargeDripstone {
    BlockPos root;
    bool pointingUp;
    int radius;
    double bluntness;
    double scale;

    int getHeightAtRadius(float checkRadius) const {
        return static_cast<int>(getDripstoneHeight(checkRadius, radius, scale, bluntness));
    }
    int getHeight() const { return getHeightAtRadius(0.0f); }
    bool isSuitableForWind(const LargeDripstoneConfig& config) const {
        return radius >= config.minRadiusForWind && bluntness >= static_cast<double>(config.minBluntnessForWind);
    }
};

struct WindOffsetter {
    int originY = 0;
    bool hasWind = false;
    double windX = 0.0, windZ = 0.0;

    BlockPos offset(BlockPos pos) const {
        if (!hasWind) return pos;
        const int dy = originY - pos.y;
        const double totalX = windX * dy;
        const double totalZ = windZ * dy;
        return BlockPos{ pos.x + mc::levelgen::mth::floor(totalX), pos.y, pos.z + mc::levelgen::mth::floor(totalZ) };
    }
};

// LargeDripstone.moveBackUntilBaseIsInsideStoneAndShrinkRadiusIfNecessary (:131-153).
inline bool moveBackUntilBaseInsideStone(const DripstoneHooks& h, LargeDripstone& d,
                                         WorldGenLevel& level, const WindOffsetter& wind) {
    while (d.radius > 1) {
        BlockPos newRoot = d.root;
        const int maxTries = std::min(10, d.getHeight());
        for (int i = 0; i < maxTries; ++i) {
            if (level.getBlockState(newRoot) == "minecraft:lava") return false;
            if (isCircleMostlyEmbeddedInStone(h, level, wind.offset(newRoot), d.radius)) {
                d.root = newRoot;
                return true;
            }
            newRoot = BlockPos{ newRoot.x, newRoot.y + (d.pointingUp ? -1 : 1), newRoot.z };
        }
        d.radius /= 2;
    }
    return false;
}

// LargeDripstone.placeBlocks (:159-190).
inline void largeDripstonePlaceBlocks(const DripstoneHooks& h, const LargeDripstone& d,
                                      WorldGenLevel& level, RandomSource& random, const WindOffsetter& wind) {
    for (int dx = -d.radius; dx <= d.radius; ++dx) {
        for (int dz = -d.radius; dz <= d.radius; ++dz) {
            // Mth.sqrt(float) = (float)Math.sqrt
            const float currentRadius = static_cast<float>(std::sqrt(static_cast<double>(static_cast<float>(dx * dx + dz * dz))));
            if (currentRadius > static_cast<float>(d.radius)) continue;
            int height = d.getHeightAtRadius(currentRadius);
            if (height <= 0) continue;
            if (random.nextFloat() < 0.2f) {
                // Mth.randomBetween(random, 0.8F, 1.0F)
                height = static_cast<int>(static_cast<float>(height) * (random.nextFloat() * (1.0f - 0.8f) + 0.8f));
            }
            BlockPos pos{ d.root.x + dx, d.root.y, d.root.z + dz };
            bool hasBeenOutOfStone = false;
            const int maxY = d.pointingUp
                ? level.getHeight(Heightmap::Types::WORLD_SURFACE_WG, pos.x, pos.z)
                : std::numeric_limits<int>::max();
            for (int i = 0; i < height && pos.y < maxY; ++i) {
                const BlockPos windAdjustedPos = wind.offset(pos);
                if (isEmptyOrWaterOrLava(h, level.getBlockState(windAdjustedPos))) {
                    hasBeenOutOfStone = true;
                    level.setBlock(windAdjustedPos, "minecraft:dripstone_block", 2);
                } else if (hasBeenOutOfStone && h.baseStoneOverworld(level.getBlockState(windAdjustedPos))) {
                    break;
                }
                pos = BlockPos{ pos.x, pos.y + (d.pointingUp ? 1 : -1), pos.z };
            }
        }
    }
}

} // namespace dripstone_detail

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeLargeDripstonePlacer(
        std::shared_ptr<const LargeDripstoneConfig> config, std::shared_ptr<const DripstoneHooks> hooks) {
    return [config = std::move(config), hooks = std::move(hooks)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        using namespace dripstone_detail;
        const DripstoneHooks& h = *hooks;
        const LargeDripstoneConfig& cfg = *config;
        if (!isEmptyOrWater(h, level.getBlockState(origin))) return false;
        auto inside = [&](const std::string& s) { return isEmptyOrWater(h, s); };
        auto edge = [&](const std::string& s) { return isDripstoneBaseOrLava(h, s); };
        const std::optional<ColumnRange> column = columnScan(level, origin, cfg.floorToCeilingSearchRange, inside, edge);
        // `column.get() instanceof Column.Range` requires BOTH floor and ceiling.
        if (!column.has_value() || !column->floor.has_value() || !column->ceiling.has_value()) return false;
        const int colFloor = *column->floor, colCeiling = *column->ceiling;
        const int colHeight = colCeiling - colFloor - 1;
        if (colHeight < 4) return false;
        const int maxColumnRadiusBasedOnColumnHeight = static_cast<int>(colHeight * cfg.maxColumnRadiusToCaveHeightRatio);
        const int maxColumnRadius = std::min(std::max(maxColumnRadiusBasedOnColumnHeight, cfg.columnRadiusMin), cfg.columnRadiusMax);   // Mth.clamp
        const int radius = random.nextInt(maxColumnRadius - cfg.columnRadiusMin + 1) + cfg.columnRadiusMin;   // Mth.randomBetweenInclusive
        LargeDripstone stalactite{ BlockPos{ origin.x, colCeiling - 1, origin.z }, false, radius,
                                   static_cast<double>(cfg.stalactiteBluntness->sample(random)),
                                   static_cast<double>(cfg.heightScale->sample(random)) };
        LargeDripstone stalagmite{ BlockPos{ origin.x, colFloor + 1, origin.z }, true, radius,
                                   static_cast<double>(cfg.stalagmiteBluntness->sample(random)),
                                   static_cast<double>(cfg.heightScale->sample(random)) };
        WindOffsetter wind;
        if (stalactite.isSuitableForWind(cfg) && stalagmite.isSuitableForWind(cfg)) {
            wind.originY = origin.y;
            wind.hasWind = true;
            const float speed = cfg.windSpeed->sample(random);
            const float direction = random.nextFloat() * (static_cast<float>(3.141592653589793) - 0.0f) + 0.0f;   // Mth.randomBetween(0, (float)Math.PI)
            wind.windX = static_cast<double>(mc::levelgen::mth::cos(direction) * speed);
            wind.windZ = static_cast<double>(mc::levelgen::mth::sin(direction) * speed);
        }
        const bool stalactiteOk = moveBackUntilBaseInsideStone(h, stalactite, level, wind);
        const bool stalagmiteOk = moveBackUntilBaseInsideStone(h, stalagmite, level, wind);
        if (stalactiteOk) largeDripstonePlaceBlocks(h, stalactite, level, random, wind);
        if (stalagmiteOk) largeDripstonePlaceBlocks(h, stalagmite, level, random, wind);
        return true;
    };
}

// ---------------------------------------------------------------------------
// DripstoneClusterFeature (DripstoneClusterFeature.java).
struct DripstoneClusterConfig {
    int floorToCeilingSearchRange = 12;
    mc::valueproviders::IntProviderPtr height, radius, dripstoneBlockLayerThickness;
    mc::valueproviders::FloatProviderPtr wetness, density;
    int maxStalagmiteStalactiteHeightDiff = 1;
    int heightDeviation = 3;
    float chanceOfDripstoneColumnAtMaxDistanceFromCenter = 0.1f;
    int maxDistanceFromEdgeAffectingChanceOfDripstoneColumn = 3;
    int maxDistanceFromCenterAffectingHeightBias = 8;
};

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeDripstoneClusterPlacer(
        std::shared_ptr<const DripstoneClusterConfig> config, std::shared_ptr<const DripstoneHooks> hooks) {
    return [config = std::move(config), hooks = std::move(hooks)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        using namespace dripstone_detail;
        const DripstoneHooks& h = *hooks;
        const DripstoneClusterConfig& cfg = *config;
        if (!isEmptyOrWater(h, level.getBlockState(origin))) return false;
        const int clusterHeight = cfg.height->sample(random);
        const float wetness = cfg.wetness->sample(random);
        const float density = cfg.density->sample(random);
        const int xRadius = cfg.radius->sample(random);
        const int zRadius = cfg.radius->sample(random);

        // getChanceOfStalagmiteOrStalactite (:201-210): FLOAT clampedMap.
        auto chanceOf = [&](int dx, int dz) -> double {
            const int xDistanceFromEdge = xRadius - std::abs(dx);
            const int zDistanceFromEdge = zRadius - std::abs(dz);
            const int distanceFromEdge = std::min(xDistanceFromEdge, zDistanceFromEdge);
            return static_cast<double>(mc::levelgen::mth::clampedMapF(
                static_cast<float>(distanceFromEdge), 0.0f,
                static_cast<float>(cfg.maxDistanceFromEdgeAffectingChanceOfDripstoneColumn),
                cfg.chanceOfDripstoneColumnAtMaxDistanceFromCenter, 1.0f));
        };
        // getDripstoneHeight (:153-163).
        auto dripstoneHeight = [&](int dx, int dz, int maxHeight) -> int {
            if (random.nextFloat() > density) return 0;
            const int distanceFromCenter = std::abs(dx) + std::abs(dz);
            const float heightMean = static_cast<float>(mc::levelgen::mth::clampedMapD(
                static_cast<double>(distanceFromCenter), 0.0,
                static_cast<double>(cfg.maxDistanceFromCenterAffectingHeightBias),
                static_cast<double>(maxHeight) / 2.0, 0.0));
            // ClampedNormalFloat.sample(random, mean, deviation, min, maxExclusive)
            const float v = mc::valueproviders::mthClampF(
                mc::valueproviders::mthNormal(random, heightMean, static_cast<float>(cfg.heightDeviation)),
                0.0f, static_cast<float>(maxHeight));
            return static_cast<int>(v);
        };
        auto canBeAdjacentToWater = [&](BlockPos p) {
            const std::string s = level.getBlockState(p);
            return h.baseStoneOverworld(s) || h.isWaterFluid(s);
        };
        auto canPlacePool = [&](BlockPos p) {
            const std::string s = level.getBlockState(p);
            if (s == "minecraft:water" || s == "minecraft:dripstone_block" || s == "minecraft:pointed_dripstone") return false;
            if (h.isWaterFluid(level.getBlockState(BlockPos{ p.x, p.y + 1, p.z }))) return false;
            static constexpr int HORIZONTAL[4] = { 2, 5, 3, 4 };
            for (int d : HORIZONTAL) {
                if (!canBeAdjacentToWater(treeRelative(p, d))) return false;
            }
            return canBeAdjacentToWater(BlockPos{ p.x, p.y - 1, p.z });
        };
        auto replaceBlocksWithDripstone = [&](BlockPos firstPos, int maxCount, int dirY) {
            BlockPos p = firstPos;
            for (int i = 0; i < maxCount; ++i) {
                if (!placeDripstoneBlockIfPossible(h, level, p)) return;
                p = BlockPos{ p.x, p.y + dirY, p.z };
            }
        };

        auto inside = [&](const std::string& s) { return isEmptyOrWater(h, s); };
        auto edge = [&](const std::string& s) { return !isEmptyOrWater(h, s); };   // isNeitherEmptyNorWater

        for (int dx = -xRadius; dx <= xRadius; ++dx) {
            for (int dz = -zRadius; dz <= zRadius; ++dz) {
                const double chanceOfStalagmiteOrStalactite = chanceOf(dx, dz);
                const BlockPos pos{ origin.x + dx, origin.y, origin.z + dz };
                // ---- placeColumn (:53-147) ----
                const std::optional<ColumnRange> baseColumn =
                    columnScan(level, pos, cfg.floorToCeilingSearchRange, inside, edge);
                if (!baseColumn.has_value()) continue;
                std::optional<int> ceiling = baseColumn->ceiling;
                std::optional<int> floorY = baseColumn->floor;
                if (!ceiling.has_value() && !floorY.has_value()) continue;
                const bool wantPool = random.nextFloat() < wetness;
                std::optional<int> columnFloor = floorY;
                if (wantPool && floorY.has_value() && canPlacePool(BlockPos{ pos.x, *floorY, pos.z })) {
                    const int baseFloorY = *floorY;
                    columnFloor = baseFloorY - 1;   // column.withFloor(floor - 1)
                    level.setBlock(BlockPos{ pos.x, baseFloorY, pos.z }, "minecraft:water", 2);
                }
                const bool wantStalactite = random.nextDouble() < chanceOfStalagmiteOrStalactite;
                int stalactiteHeight = 0;
                if (ceiling.has_value() && wantStalactite
                    && level.getBlockState(BlockPos{ pos.x, *ceiling, pos.z }) != "minecraft:lava") {
                    const int ceilingThickness = cfg.dripstoneBlockLayerThickness->sample(random);
                    replaceBlocksWithDripstone(BlockPos{ pos.x, *ceiling, pos.z }, ceilingThickness, +1);
                    const int maxHeightForThisColumn = columnFloor.has_value()
                        ? std::min(clusterHeight, *ceiling - *columnFloor) : clusterHeight;
                    stalactiteHeight = dripstoneHeight(dx, dz, maxHeightForThisColumn);
                }
                const bool wantStalagmite = random.nextDouble() < chanceOfStalagmiteOrStalactite;
                int stalagmiteHeight = 0;
                if (columnFloor.has_value() && wantStalagmite
                    && level.getBlockState(BlockPos{ pos.x, *columnFloor, pos.z }) != "minecraft:lava") {
                    const int floorThickness = cfg.dripstoneBlockLayerThickness->sample(random);
                    replaceBlocksWithDripstone(BlockPos{ pos.x, *columnFloor, pos.z }, floorThickness, -1);
                    if (ceiling.has_value()) {
                        stalagmiteHeight = std::max(0, stalactiteHeight
                            + (random.nextInt(2 * cfg.maxStalagmiteStalactiteHeightDiff + 1) - cfg.maxStalagmiteStalactiteHeightDiff));   // Mth.randomBetweenInclusive(-d, d)
                    } else {
                        stalagmiteHeight = dripstoneHeight(dx, dz, clusterHeight);
                    }
                }
                int actualStalactiteHeight, actualStalagmiteHeight;
                if (ceiling.has_value() && columnFloor.has_value()
                    && *ceiling - stalactiteHeight <= *columnFloor + stalagmiteHeight) {
                    const int fY = *columnFloor, cY = *ceiling;
                    const int lowestStalactiteBottom = std::max(cY - stalactiteHeight, fY + 1);
                    const int highestStalagmiteTop = std::min(fY + stalagmiteHeight, cY - 1);
                    const int actualStalactiteBottom =
                        random.nextInt(highestStalagmiteTop + 1 - lowestStalactiteBottom + 1) + lowestStalactiteBottom;   // randomBetweenInclusive
                    const int actualStalagmiteTop = actualStalactiteBottom - 1;
                    actualStalactiteHeight = cY - actualStalactiteBottom;
                    actualStalagmiteHeight = actualStalagmiteTop - fY;
                } else {
                    actualStalactiteHeight = stalactiteHeight;
                    actualStalagmiteHeight = stalagmiteHeight;
                }
                // column.getHeight(): present iff BOTH edges (after the pool adjustment).
                const bool heightPresent = ceiling.has_value() && columnFloor.has_value();
                const int columnHeight = heightPresent ? *ceiling - *columnFloor - 1 : 0;
                const bool mergeTips = random.nextBoolean()
                    && actualStalactiteHeight > 0 && actualStalagmiteHeight > 0
                    && heightPresent && actualStalactiteHeight + actualStalagmiteHeight == columnHeight;
                if (ceiling.has_value()) {
                    growPointedDripstone(h, level, BlockPos{ pos.x, *ceiling - 1, pos.z }, 0, actualStalactiteHeight, mergeTips);
                }
                if (columnFloor.has_value()) {
                    growPointedDripstone(h, level, BlockPos{ pos.x, *columnFloor + 1, pos.z }, 1, actualStalagmiteHeight, mergeTips);
                }
            }
        }
        return true;
    };
}

} // namespace mc::levelgen::feature

#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.GeodeFeature
// ("geode"; data: amethyst_geode, rarity 1/24 in UNDERGROUND_STRUCTURES).
//
// RNG order (GeodeFeature.java:36-171):
//   numPoints = distributionPoints.sample          (uniform 3..4: 1 draw)
//   geode NOISE: NormalNoise.create(WorldgenRandom(LegacyRandomSource(level.getSeed())),
//     -4, [1.0]) — a SEPARATE rng; consumes nothing from the feature random (:45-46)
//   crackSize: baseCrackSize + nextDouble()/2 + (numPoints>3 ? adj : 0)   (1 draw, :56)
//   shouldGenerateCrack = nextFloat() < generateCrackChance               (1 draw, :57)
//   per distribution point: outerWallDistance.sample x3 (x,y,z); the invalid-block
//     check may ++numInvalidPoints and return false past the threshold; otherwise
//     pointOffset.sample (1 draw) (:60-73)
//   if shouldGenerateCrack: nextInt(4) picks the crack offset axis (:76)
//   the 33^3 betweenClosed volume (x fastest, then y, then z — BlockPos.java:405-427):
//     per cell the layer arms draw ONLY in the innermost-block arm: nextFloat
//     (alternate layer) ALWAYS, then nextFloat (potential placement) iff
//     (!placementsRequireLayer0Alternate || useAlternateLayer) (:126-138)
//   per potential crystal placement: Util.getRandom(innerPlacements) = nextInt(size);
//     direction scan reads states; FACING/WATERLOGGED setValue draw nothing (:149-168)
//
// safeSetBlock = write gated on !#features_cannot_replace at the target
// (Feature.safeSetBlock; isReplaceable(GeodeFeature) — cannot_replace tag).
// The crack arm schedules fluid ticks on wet neighbours — hard no-op, counted.
// Mth.invSqrt = org.joml.Math.invsqrt = 1.0 / Math.sqrt (Mth.java:422-428).

#include "TreeFeature.h"   // WorldGenLevel, BlockPos
#include "DiskFeature.h"   // DiskStateProvider
#include "../IntProvider.h"
#include "../Noise.h"
#include "../RandomSource.h"
#include "../../material/Fluids.h"

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mc::levelgen::feature {

struct GeodeConfig {
    DiskStateProvider fillingProvider, innerLayerProvider, alternateInnerLayerProvider,
                      middleLayerProvider, outerLayerProvider;
    std::vector<std::string> innerPlacements;                       // crystal states (ids)
    std::function<bool(const std::string&)> cannotReplace;          // #features_cannot_replace
    std::function<bool(const std::string&)> invalidBlocks;          // #geode_invalid_blocks
    double filling = 1.7, innerLayer = 2.2, middleLayer = 3.2, outerLayer = 4.2;   // GeodeLayerSettings
    double baseCrackSize = 2.0;                                     // GeodeCrackSettings
    double generateCrackChance = 0.95;
    int crackPointOffset = 2;
    double noiseMultiplier = 0.05;
    double usePotentialPlacementsChance = 0.35;
    double useAlternateLayer0Chance = 0.083;
    bool placementsRequireLayer0Alternate = true;
    mc::valueproviders::IntProviderPtr outerWallDistance, distributionPoints, pointOffset;
    int outerWallDistanceMax = 6;                                   // outerWallDistance.maxInclusive()
    int minGenOffset = -16, maxGenOffset = 16;
    int invalidBlocksThreshold = 1;
};

struct GeodeHooks {
    std::function<bool(const std::string&)> isAir;
    std::function<std::int64_t()> levelSeed;                        // level.getSeed() (per-case)
    std::function<void()> countSkippedScheduleTick;                 // fluid ticks: hard no-op, counted
};

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeGeodePlacer(
        std::shared_ptr<const GeodeConfig> config, std::shared_ptr<const GeodeHooks> hooks) {
    return [config = std::move(config), hooks = std::move(hooks)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        const GeodeConfig& cfg = *config;
        const int minGenOffset = cfg.minGenOffset, maxGenOffset = cfg.maxGenOffset;
        std::vector<std::pair<BlockPos, int>> points;
        const int numPoints = cfg.distributionPoints->sample(random);
        WorldgenRandom random1(std::make_shared<LegacyRandomSource>(hooks->levelSeed()));
        const NormalNoise noise = NormalNoise::create(random1, NoiseParameters{ -4, { 1.0 } });
        std::vector<BlockPos> crackPoints;
        const double crackSizeAdjustment = static_cast<double>(numPoints) / cfg.outerWallDistanceMax;
        const double innerAir = 1.0 / std::sqrt(cfg.filling);
        const double innermostBlockLayer = 1.0 / std::sqrt(cfg.innerLayer + crackSizeAdjustment);
        const double innerCrust = 1.0 / std::sqrt(cfg.middleLayer + crackSizeAdjustment);
        const double outerCrust = 1.0 / std::sqrt(cfg.outerLayer + crackSizeAdjustment);
        const double crackSize = 1.0 / std::sqrt(cfg.baseCrackSize + random.nextDouble() / 2.0
                                                 + (numPoints > 3 ? crackSizeAdjustment : 0.0));
        const bool shouldGenerateCrack = static_cast<double>(random.nextFloat()) < cfg.generateCrackChance;
        int numInvalidPoints = 0;

        for (int i = 0; i < numPoints; ++i) {
            const int x = cfg.outerWallDistance->sample(random);
            const int y = cfg.outerWallDistance->sample(random);
            const int z = cfg.outerWallDistance->sample(random);
            const BlockPos pos{ origin.x + x, origin.y + y, origin.z + z };
            const std::string state = level.getBlockState(pos);
            if (hooks->isAir(state) || cfg.invalidBlocks(state)) {
                if (++numInvalidPoints > cfg.invalidBlocksThreshold) {
                    return false;
                }
            }
            points.emplace_back(pos, cfg.pointOffset->sample(random));
        }

        if (shouldGenerateCrack) {
            const int offsetIndex = random.nextInt(4);
            const int crackOffset = numPoints * 2 + 1;
            auto add = [&](int ox, int oy, int oz) { crackPoints.push_back(BlockPos{ origin.x + ox, origin.y + oy, origin.z + oz }); };
            if (offsetIndex == 0) { add(crackOffset, 7, 0); add(crackOffset, 5, 0); add(crackOffset, 1, 0); }
            else if (offsetIndex == 1) { add(0, 7, crackOffset); add(0, 5, crackOffset); add(0, 1, crackOffset); }
            else if (offsetIndex == 2) { add(crackOffset, 7, crackOffset); add(crackOffset, 5, crackOffset); add(crackOffset, 1, crackOffset); }
            else { add(0, 7, 0); add(0, 5, 0); add(0, 1, 0); }
        }

        std::vector<BlockPos> potentialCrystalPlacements;
        auto canReplace = [&](const std::string& s) { return !cfg.cannotReplace(s); };
        auto safeSetBlock = [&](BlockPos pos, const std::string& st) {
            if (canReplace(level.getBlockState(pos))) {
                level.setBlock(pos, st, 2);   // Feature.setBlock: flags 2
            }
        };

        // betweenClosed(origin+minGen, origin+maxGen): x fastest, then y, then z.
        for (int dz = minGenOffset; dz <= maxGenOffset; ++dz) {
            for (int dy = minGenOffset; dy <= maxGenOffset; ++dy) {
                for (int dx = minGenOffset; dx <= maxGenOffset; ++dx) {
                    const BlockPos pointInside{ origin.x + dx, origin.y + dy, origin.z + dz };
                    const double noiseOffset = noise.getValue(pointInside.x, pointInside.y, pointInside.z) * cfg.noiseMultiplier;
                    double distSumShell = 0.0;
                    double distSumCrack = 0.0;
                    for (const auto& [point, offsetVal] : points) {
                        const double ddx = pointInside.x - point.x, ddy = pointInside.y - point.y, ddz = pointInside.z - point.z;
                        distSumShell += 1.0 / std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz + static_cast<double>(offsetVal)) + noiseOffset;
                    }
                    for (const BlockPos& point : crackPoints) {
                        const double ddx = pointInside.x - point.x, ddy = pointInside.y - point.y, ddz = pointInside.z - point.z;
                        distSumCrack += 1.0 / std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz + static_cast<double>(cfg.crackPointOffset)) + noiseOffset;
                    }
                    if (distSumShell < outerCrust) continue;
                    if (shouldGenerateCrack && distSumCrack >= crackSize && distSumShell < innerAir) {
                        safeSetBlock(pointInside, "minecraft:air");
                        // Direction.values(): DOWN,UP,NORTH,SOUTH,WEST,EAST — wet
                        // neighbours get a fluid tick (no ServerLevel: counted no-op).
                        for (int dir = 0; dir < 6; ++dir) {
                            const BlockPos adjacent = treeRelative(pointInside, dir);
                            const std::string adjacentState = level.getBlockState(adjacent);
                            if (!mc::material::fluidStateOf(adjacentState).isEmpty()) {
                                hooks->countSkippedScheduleTick();
                            }
                        }
                    } else if (distSumShell >= innerAir) {
                        safeSetBlock(pointInside, cfg.fillingProvider(level, random, pointInside).value());
                    } else if (distSumShell >= innermostBlockLayer) {
                        const bool useAlternateLayer = static_cast<double>(random.nextFloat()) < cfg.useAlternateLayer0Chance;
                        if (useAlternateLayer) {
                            safeSetBlock(pointInside, cfg.alternateInnerLayerProvider(level, random, pointInside).value());
                        } else {
                            safeSetBlock(pointInside, cfg.innerLayerProvider(level, random, pointInside).value());
                        }
                        if ((!cfg.placementsRequireLayer0Alternate || useAlternateLayer)
                            && static_cast<double>(random.nextFloat()) < cfg.usePotentialPlacementsChance) {
                            potentialCrystalPlacements.push_back(pointInside);
                        }
                    } else if (distSumShell >= innerCrust) {
                        safeSetBlock(pointInside, cfg.middleLayerProvider(level, random, pointInside).value());
                    } else {   // distSumShell >= outerCrust
                        safeSetBlock(pointInside, cfg.outerLayerProvider(level, random, pointInside).value());
                    }
                }
            }
        }

        // Crystal placements (GeodeFeature.java:147-168). FACING/WATERLOGGED setValue
        // are id-invisible; the state pick is one nextInt per placement.
        for (const BlockPos& crystalPos : potentialCrystalPlacements) {
            const std::string blockState = cfg.innerPlacements[static_cast<std::size_t>(
                random.nextInt(static_cast<int>(cfg.innerPlacements.size())))];   // Util.getRandom
            for (int dir = 0; dir < 6; ++dir) {
                const BlockPos placePos = treeRelative(crystalPos, dir);
                const std::string placeState = level.getBlockState(placePos);
                // BuddingAmethystBlock.canClusterGrowAtState (BuddingAmethystBlock.java:52-54):
                // isAir || (water block && fluid isFull) — id "minecraft:water" is source/full.
                if (hooks->isAir(placeState) || placeState == "minecraft:water") {
                    safeSetBlock(placePos, blockState);
                    break;
                }
            }
        }
        return true;
    };
}

} // namespace mc::levelgen::feature

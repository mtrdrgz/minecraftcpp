#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.AbstractHugeMushroomFeature
// + HugeBrownMushroomFeature + HugeRedMushroomFeature (26.1.2 sources).
//
// RNG order (AbstractHugeMushroomFeature.place, :88-103):
//   treeHeight = nextInt(3) + 4; nextInt(12) == 0 -> *2          (:46-53)
//   isValidPosition: world reads only (canPlaceOn predicate at origin.below(),
//     then the air-or-#leaves column scan over getTreeRadiusForHeight boxes)
//   makeCap THEN placeTrunk (place order matters for overlap); the cap/stem
//   providers are simple_state_providers (no draws). All writes via
//   Feature.setBlock == level.setBlock(pos, state, 3) (Feature.java:169-171),
//   gated by placeMushroomBlock's isAir || #replaceable_by_mushrooms (:35-40).
// The cap property juggling (WEST/EAST/NORTH/SOUTH/UP) is id-invisible.

#include "../placement/PlacementContext.h"   // WorldGenLevel
#include "../placement/PlacedFeature.h"
#include "../RandomSource.h"
#include "DiskFeature.h"                     // DiskStateProvider
#include "../../../../core/Math.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

struct HugeMushroomConfig {
    DiskStateProvider capProvider;     // cap_provider (getState, no draws here)
    DiskStateProvider stemProvider;    // stem_provider
    std::function<bool(WorldGenLevel&, BlockPos)> canPlaceOn;   // BlockPredicate at origin.below()
    int foliageRadius = 2;
    bool red = false;                  // HugeRed vs HugeBrown geometry
};

struct HugeMushroomHooks {
    std::function<bool(const std::string&)> isAir;
    std::function<bool(const std::string&)> isLeavesTag;            // #minecraft:leaves
    std::function<bool(const std::string&)> replaceableByMushrooms; // #replaceable_by_mushrooms
    int levelMinY = 0;
    int levelMaxY = 0;   // inclusive (level.getMaxY())
};

namespace hugemushroom_detail {

// AbstractHugeMushroomFeature.placeMushroomBlock (:35-40).
inline void placeMushroomBlock(WorldGenLevel& level, const HugeMushroomHooks& hooks,
                               BlockPos pos, const std::string& newState) {
    const std::string current = level.getBlockState(pos);
    if (hooks.isAir(current) || hooks.replaceableByMushrooms(current)) {
        level.setBlock(pos, newState, 3);   // Feature.setBlock (Feature.java:169-171)
    }
}

// HugeBrown/HugeRedMushroomFeature.getTreeRadiusForHeight.
inline int getTreeRadiusForHeight(const HugeMushroomConfig& cfg, int treeHeight, int yo) {
    if (!cfg.red) {
        return yo <= 3 ? 0 : cfg.foliageRadius;     // HugeBrownMushroomFeature.java:59-61
    }
    int radius = 0;                                  // HugeRedMushroomFeature.java:62-71
    if (yo < treeHeight && yo >= treeHeight - 3) radius = cfg.foliageRadius;
    else if (yo == treeHeight) radius = cfg.foliageRadius;
    return radius;
}

// HugeBrownMushroomFeature.makeCap (:24-57).
inline void makeBrownCap(WorldGenLevel& level, const HugeMushroomHooks& hooks,
                         const HugeMushroomConfig& cfg, RandomSource& random,
                         BlockPos origin, int treeHeight) {
    const int radius = cfg.foliageRadius;
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            const bool minX = dx == -radius, maxX = dx == radius;
            const bool minZ = dz == -radius, maxZ = dz == radius;
            const bool xEdge = minX || maxX, zEdge = minZ || maxZ;
            if (!xEdge || !zEdge) {
                const BlockPos pos{ origin.x + dx, origin.y + treeHeight, origin.z + dz };
                const std::optional<std::string> state = cfg.capProvider(level, random, origin);
                placeMushroomBlock(level, hooks, pos, state.value());
            }
        }
    }
}

// HugeRedMushroomFeature.makeCap (:24-59).
inline void makeRedCap(WorldGenLevel& level, const HugeMushroomHooks& hooks,
                       const HugeMushroomConfig& cfg, RandomSource& random,
                       BlockPos origin, int treeHeight) {
    for (int dy = treeHeight - 3; dy <= treeHeight; ++dy) {
        const int radius = dy < treeHeight ? cfg.foliageRadius : cfg.foliageRadius - 1;
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                const bool minX = dx == -radius, maxX = dx == radius;
                const bool minZ = dz == -radius, maxZ = dz == radius;
                const bool xEdge = minX || maxX, zEdge = minZ || maxZ;
                if (dy >= treeHeight || xEdge != zEdge) {
                    const BlockPos pos{ origin.x + dx, origin.y + dy, origin.z + dz };
                    const std::optional<std::string> state = cfg.capProvider(level, random, origin);
                    placeMushroomBlock(level, hooks, pos, state.value());
                }
            }
        }
    }
}

} // namespace hugemushroom_detail

// AbstractHugeMushroomFeature.place (:88-103).
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeHugeMushroomPlacer(
        std::shared_ptr<const HugeMushroomConfig> config, std::shared_ptr<const HugeMushroomHooks> hooks) {
    return [config = std::move(config), hooks = std::move(hooks)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        using namespace hugemushroom_detail;
        // getTreeHeight (:46-53)
        int treeHeight = random.nextInt(3) + 4;
        if (random.nextInt(12) == 0) treeHeight *= 2;
        // isValidPosition (:59-86)
        const int y = origin.y;
        if (!(y >= hooks->levelMinY + 1 && y + treeHeight + 1 <= hooks->levelMaxY)) return false;
        if (!config->canPlaceOn(level, BlockPos{ origin.x, origin.y - 1, origin.z })) return false;
        for (int dy = 0; dy <= treeHeight; ++dy) {
            const int radius = getTreeRadiusForHeight(*config, -1, dy);   // (-1, -1, foliageRadius, dy)
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dz = -radius; dz <= radius; ++dz) {
                    const std::string state = level.getBlockState(BlockPos{ origin.x + dx, origin.y + dy, origin.z + dz });
                    if (!hooks->isAir(state) && !hooks->isLeavesTag(state)) return false;
                }
            }
        }
        // makeCap then placeTrunk (:99-101)
        if (config->red) makeRedCap(level, *hooks, *config, random, origin, treeHeight);
        else makeBrownCap(level, *hooks, *config, random, origin, treeHeight);
        // placeTrunk (:21-33): stem getState passes ORIGIN, not the column pos.
        for (int dy = 0; dy < treeHeight; ++dy) {
            const BlockPos pos{ origin.x, origin.y + dy, origin.z };
            const std::optional<std::string> stem = config->stemProvider(level, random, origin);
            placeMushroomBlock(level, *hooks, pos, stem.value());
        }
        return true;
    };
}

} // namespace mc::levelgen::feature

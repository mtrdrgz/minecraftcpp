#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.LakeFeature (the
// @Deprecated lava/water lake; data: lake_lava_surface rarity 1/200,
// lake_lava_underground rarity 1/9).
//
// RNG order (LakeFeature.java:24-146):
//   early-out origin.y <= minY+4: NO draws
//   spots = nextInt(4) + 4; per spot SIX nextDouble (xr, yr, zr, xp, yp, zp)
//   fluid = config.fluid.getState (simple provider: no draw)
//   validity scan + placement + barrier scan: barrier pass draws nextInt(2) at
//     every yy>=4 shell cell (the (yy < 4 || nextInt(2) != 0) SHORT-CIRCUIT:
//     yy<4 skips the draw); freeze pass: no draws
//
// Write semantics: lake cells yy>=4 -> CAVE_AIR + scheduleTick(AIR; hard no-op,
// counted) + markAboveForPostProcessing; yy<4 -> fluid. Barrier writes solid
// shell cells not in #lava_pool_stone_cannot_replace + markAbove. Water lakes
// additionally freeze the y=4 rim where biome.shouldFreeze (checkNeighbors=false).
// All writes gated by canReplaceBlock = !#features_cannot_replace (:148-150)
// except the barrier (its own isSolid + tag gate, :120-127).

#include "TreeFeature.h"            // WorldGenLevel, BlockPos
#include "SnowAndFreezeFeature.h"   // biomeShouldFreeze + BiomeClimate + SnowFreezeHooks
#include "DiskFeature.h"            // DiskStateProvider

#include <array>
#include <functional>
#include <memory>
#include <string>

namespace mc::levelgen::feature {

struct LakeHooks {
    std::function<bool(const std::string&)> featuresCannotReplace;        // #features_cannot_replace
    std::function<bool(const std::string&)> lavaPoolStoneCannotReplace;   // #lava_pool_stone_cannot_replace
    std::function<bool(const std::string&)> isSolid;                      // BlockStateBase.isSolid (legacySolid)
    std::function<bool(const std::string&)> isLiquid;                     // BlockStateBase.liquid()
    std::function<bool(const std::string&)> isAir;                        // barrier.isAir() gate
    std::function<void(BlockPos)> markAboveForPostProcessing;             // Feature.markAboveForPostProcessing
    std::function<void()> countSkippedScheduleTick;                       // scheduleTick (needs ServerLevel): hard no-op, counted
    // Water-lake freeze: zoomed biome + climate (lava lakes never call these).
    std::shared_ptr<const SnowFreezeHooks> snowFreeze;
};

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeLakePlacer(
        DiskStateProvider fluidProvider, DiskStateProvider barrierProvider,
        std::shared_ptr<const LakeHooks> hooks) {
    return [fluidProvider = std::move(fluidProvider), barrierProvider = std::move(barrierProvider),
            hooks = std::move(hooks)](WorldGenLevel& level, RandomSource& random, BlockPos originIn) -> bool {
        BlockPos origin = originIn;
        if (origin.y <= level.getMinY() + 4) {
            return false;
        }
        origin = BlockPos{ origin.x - 8, origin.y - 4, origin.z - 8 };
        std::array<bool, 2048> grid{};
        const int spots = random.nextInt(4) + 4;
        for (int i = 0; i < spots; ++i) {
            const double xr = random.nextDouble() * 6.0 + 3.0;
            const double yr = random.nextDouble() * 4.0 + 2.0;
            const double zr = random.nextDouble() * 6.0 + 3.0;
            const double xp = random.nextDouble() * (16.0 - xr - 2.0) + 1.0 + xr / 2.0;
            const double yp = random.nextDouble() * (8.0 - yr - 4.0) + 2.0 + yr / 2.0;
            const double zp = random.nextDouble() * (16.0 - zr - 2.0) + 1.0 + zr / 2.0;
            for (int xx = 1; xx < 15; ++xx) {
                for (int zz = 1; zz < 15; ++zz) {
                    for (int yy = 1; yy < 7; ++yy) {
                        const double xd = (xx - xp) / (xr / 2.0);
                        const double yd = (yy - yp) / (yr / 2.0);
                        const double zd = (zz - zp) / (zr / 2.0);
                        const double d = xd * xd + yd * yd + zd * zd;
                        if (d < 1.0) {
                            grid[static_cast<std::size_t>((xx * 16 + zz) * 8 + yy)] = true;
                        }
                    }
                }
            }
        }

        const std::string fluid = fluidProvider(level, random, origin).value();
        auto isShell = [&grid](int xx, int zz, int yy) {
            return !grid[static_cast<std::size_t>((xx * 16 + zz) * 8 + yy)]
                && (xx < 15 && grid[static_cast<std::size_t>(((xx + 1) * 16 + zz) * 8 + yy)]
                    || xx > 0 && grid[static_cast<std::size_t>(((xx - 1) * 16 + zz) * 8 + yy)]
                    || zz < 15 && grid[static_cast<std::size_t>((xx * 16 + zz + 1) * 8 + yy)]
                    || zz > 0 && grid[static_cast<std::size_t>((xx * 16 + (zz - 1)) * 8 + yy)]
                    || yy < 7 && grid[static_cast<std::size_t>((xx * 16 + zz) * 8 + yy + 1)]
                    || yy > 0 && grid[static_cast<std::size_t>((xx * 16 + zz) * 8 + (yy - 1))]);
        };

        // Validity scan (LakeFeature.java:62-86): a liquid in the upper shell or a
        // non-solid non-fluid cell in the lower shell aborts.
        for (int xx = 0; xx < 16; ++xx) {
            for (int zz = 0; zz < 16; ++zz) {
                for (int yy = 0; yy < 8; ++yy) {
                    if (!isShell(xx, zz, yy)) continue;
                    const std::string state = level.getBlockState(BlockPos{ origin.x + xx, origin.y + yy, origin.z + zz });
                    if (yy >= 4 && hooks->isLiquid(state)) {
                        return false;
                    }
                    // `!= fluid` is a BlockState reference compare; at id level the
                    // worldgen fluids ("minecraft:lava"/"minecraft:water", level 0) match
                    // exactly when ids match.
                    if (yy < 4 && !hooks->isSolid(state) && state != fluid) {
                        return false;
                    }
                }
            }
        }

        // Placement (LakeFeature.java:88-104).
        for (int xx = 0; xx < 16; ++xx) {
            for (int zz = 0; zz < 16; ++zz) {
                for (int yy = 0; yy < 8; ++yy) {
                    if (!grid[static_cast<std::size_t>((xx * 16 + zz) * 8 + yy)]) continue;
                    const BlockPos placePos{ origin.x + xx, origin.y + yy, origin.z + zz };
                    if (!hooks->featuresCannotReplace(level.getBlockState(placePos))) {
                        const bool placeAir = yy >= 4;
                        level.setBlock(placePos, placeAir ? "minecraft:cave_air" : fluid, 2);
                        if (placeAir) {
                            hooks->countSkippedScheduleTick();                 // scheduleTick(pos, CAVE_AIR, 0)
                            hooks->markAboveForPostProcessing(placePos);
                        }
                    }
                }
            }
        }

        // Barrier (LakeFeature.java:106-131). barrier.isAir() skips (lava lakes use stone).
        const std::string barrier = barrierProvider(level, random, origin).value();
        if (!hooks->isAir(barrier)) {
            for (int xx = 0; xx < 16; ++xx) {
                for (int zz = 0; zz < 16; ++zz) {
                    for (int yy = 0; yy < 8; ++yy) {
                        // (yy < 4 || random.nextInt(2) != 0): the draw fires ONLY for
                        // shell cells with yy >= 4 (&& short-circuit after isShell).
                        if (isShell(xx, zz, yy) && (yy < 4 || random.nextInt(2) != 0)) {
                            const BlockPos barrierPos{ origin.x + xx, origin.y + yy, origin.z + zz };
                            const std::string state = level.getBlockState(barrierPos);
                            if (hooks->isSolid(state) && !hooks->lavaPoolStoneCannotReplace(state)) {
                                level.setBlock(barrierPos, barrier, 2);
                                hooks->markAboveForPostProcessing(barrierPos);
                            }
                        }
                    }
                }
            }
        }

        // Water-lake rim freeze (LakeFeature.java:133-143); fluid tag WATER == id water.
        if (fluid == "minecraft:water") {
            const SnowFreezeHooks& sf = *hooks->snowFreeze;
            for (int xx = 0; xx < 16; ++xx) {
                for (int zz = 0; zz < 16; ++zz) {
                    const BlockPos offset{ origin.x + xx, origin.y + 4, origin.z + zz };
                    const BiomeClimate& climate = sf.climate(sf.getBiome(offset));
                    if (biomeShouldFreeze(sf, climate, level, offset, false)
                        && !hooks->featuresCannotReplace(level.getBlockState(offset))) {
                        level.setBlock(offset, "minecraft:ice", 2);
                    }
                }
            }
        }
        return true;
    };
}

} // namespace mc::levelgen::feature

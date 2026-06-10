#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.SpikeFeature ("spike";
// data: ice_spike — can_place_on snow_block, can_replace #ice_spike_replaceable,
// state packed_ice).
//
// RNG order (SpikeFeature.java:16-99):
//   descend while isEmptyBlock && y > minY+2 (no draws)
//   canPlaceOn fail -> false (no draws)
//   origin += above(nextInt(4)); height = nextInt(4)+7; width = height/4 + nextInt(2)
//   if width > 1: nextInt(60) (the tall-spike gate; on 0: above(10 + nextInt(30)))
//   cone loop: per ring-edge candidate cell (xo or zo == +/-newWidth) ONE
//     nextFloat BEFORE the skip decision (Java's || short-circuit: the draw is on
//     the RIGHT of `xo != -newWidth && ...`, so it fires only for edge cells)
//   pillar loop: per corner column nextInt(5) (run length); descending runs:
//     on run exhaustion nextInt(5)+1 skip + nextInt(5) new run length
//
// `state != config.state()` (:84) is a reference compare — id equality here
// (packed_ice has no properties).

#include "TreeFeature.h"   // WorldGenLevel, BlockPos, mthFloorF

#include <cmath>
#include <functional>
#include <string>

namespace mc::levelgen::feature {

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeSpikePlacer(
        std::function<bool(WorldGenLevel&, BlockPos)> canPlaceOn,
        std::function<bool(WorldGenLevel&, BlockPos)> canReplace,
        std::string state,
        std::function<bool(const std::string&)> isAir) {
    return [canPlaceOn = std::move(canPlaceOn), canReplace = std::move(canReplace),
            state = std::move(state), isAir = std::move(isAir)](
               WorldGenLevel& level, RandomSource& random, BlockPos originIn) -> bool {
        BlockPos origin = originIn;
        while (level.isEmptyBlock(origin) && origin.y > level.getMinY() + 2) {
            origin = BlockPos{ origin.x, origin.y - 1, origin.z };
        }
        if (!canPlaceOn(level, origin)) {
            return false;
        }
        origin = BlockPos{ origin.x, origin.y + random.nextInt(4), origin.z };
        const int height = random.nextInt(4) + 7;
        const int width = height / 4 + random.nextInt(2);
        if (width > 1 && random.nextInt(60) == 0) {
            origin = BlockPos{ origin.x, origin.y + 10 + random.nextInt(30), origin.z };
        }

        for (int yOff = 0; yOff < height; ++yOff) {
            const float scale = (1.0f - static_cast<float>(yOff) / static_cast<float>(height)) * static_cast<float>(width);
            const int newWidth = static_cast<int>(std::ceil(scale));   // Mth.ceil
            for (int xo = -newWidth; xo <= newWidth; ++xo) {
                const float dx = static_cast<float>(std::abs(xo)) - 0.25f;   // Mth.abs
                for (int zo = -newWidth; zo <= newWidth; ++zo) {
                    const float dz = static_cast<float>(std::abs(zo)) - 0.25f;
                    // (xo == 0 && zo == 0 || !(dx*dx + dz*dz > scale*scale))
                    //   && (xo != -newWidth && xo != newWidth && zo != -newWidth
                    //       && zo != newWidth || !(random.nextFloat() > 0.75F))
                    // — the nextFloat draw fires exactly for edge cells that passed
                    // the first conjunct (Java && short-circuit, SpikeFeature.java:47-48).
                    if (!(xo == 0 && zo == 0 || !(dx * dx + dz * dz > scale * scale))) continue;
                    if (!(xo != -newWidth && xo != newWidth && zo != -newWidth && zo != newWidth
                          || !(random.nextFloat() > 0.75f))) continue;
                    const BlockPos positiveOffset{ origin.x + xo, origin.y + yOff, origin.z + zo };
                    const std::string st = level.getBlockState(positiveOffset);
                    if (isAir(st) || canReplace(level, positiveOffset)) {
                        level.setBlock(positiveOffset, state, 2);   // Feature.setBlock: flags 2
                    }
                    if (yOff != 0 && newWidth > 1) {
                        const BlockPos negativeOffset{ origin.x + xo, origin.y - yOff, origin.z + zo };
                        const std::string st2 = level.getBlockState(negativeOffset);
                        if (isAir(st2) || canReplace(level, negativeOffset)) {
                            level.setBlock(negativeOffset, state, 2);
                        }
                    }
                }
            }
        }

        int pillarWidth = width - 1;
        if (pillarWidth < 0) pillarWidth = 0;
        else if (pillarWidth > 1) pillarWidth = 1;
        for (int xo = -pillarWidth; xo <= pillarWidth; ++xo) {
            for (int zo = -pillarWidth; zo <= pillarWidth; ++zo) {
                BlockPos cursor{ origin.x + xo, origin.y - 1, origin.z + zo };
                int runLength = 50;
                if (std::abs(xo) == 1 && std::abs(zo) == 1) {
                    runLength = random.nextInt(5);
                }
                while (cursor.y > 50) {
                    const std::string st = level.getBlockState(cursor);
                    if (!isAir(st) && !canReplace(level, cursor) && st != state) {
                        break;
                    }
                    level.setBlock(cursor, state, 2);
                    cursor = BlockPos{ cursor.x, cursor.y - 1, cursor.z };
                    if (--runLength <= 0) {
                        cursor = BlockPos{ cursor.x, cursor.y - (random.nextInt(5) + 1), cursor.z };
                        runLength = random.nextInt(5);
                    }
                }
            }
        }
        return true;
    };
}

} // namespace mc::levelgen::feature

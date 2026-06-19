#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.BlockBlobFeature
// ("block_blob"; data: forest_rock — can_place_on #forest_rock_can_place_on,
// state mossy_cobblestone).
//
// RNG order (BlockBlobFeature.java:14-45):
//   descend while y > minY+3 && !canPlaceOn(below) (no draws); y <= minY+3 -> false
//   3 iterations: nextInt(2) x3 (xr, yr, zr); then the blob writes (no draws;
//   setBlock flags 3); then origin += (-1+nextInt(2), -nextInt(2), -1+nextInt(2))
//
// BlockPos.betweenClosed iterates x fastest, then y, then z ascending
// (BlockPos.java:405-427: index decomposes as x + width*(y + height*z));
// irrelevant for writes of one state but kept for trace parity.
// distSqr is the double squared distance (Vec3i.distSqr via getCenter? NO —
// BlockPos.distSqr(Vec3i) compares block corners: dx*dx+dy*dy+dz*dz in double).

#include "TreeFeature.h"   // WorldGenLevel, BlockPos

#include <functional>
#include <string>

namespace mc::levelgen::feature {

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeBlockBlobPlacer(
        std::function<bool(WorldGenLevel&, BlockPos)> canPlaceOn, std::string state) {
    return [canPlaceOn = std::move(canPlaceOn), state = std::move(state)](
               WorldGenLevel& level, RandomSource& random, BlockPos originIn) -> bool {
        BlockPos origin = originIn;
        while (origin.y > level.getMinY() + 3
               && !canPlaceOn(level, BlockPos{ origin.x, origin.y - 1, origin.z })) {
            origin = BlockPos{ origin.x, origin.y - 1, origin.z };
        }
        if (origin.y <= level.getMinY() + 3) {
            return false;
        }
        for (int c = 0; c < 3; ++c) {
            const int xr = random.nextInt(2);
            const int yr = random.nextInt(2);
            const int zr = random.nextInt(2);
            const float tr = static_cast<float>(xr + yr + zr) * 0.333f + 0.5f;
            // betweenClosed(origin-(xr,yr,zr), origin+(xr,yr,zr)): x fastest, then y, z.
            for (int dz = -zr; dz <= zr; ++dz) {
                for (int dy = -yr; dy <= yr; ++dy) {
                    for (int dx = -xr; dx <= xr; ++dx) {
                        const double distSqr = static_cast<double>(dx) * dx
                                             + static_cast<double>(dy) * dy
                                             + static_cast<double>(dz) * dz;
                        if (distSqr <= static_cast<double>(tr * tr)) {
                            level.setBlock(BlockPos{ origin.x + dx, origin.y + dy, origin.z + dz }, state, 3);
                        }
                    }
                }
            }
            origin = BlockPos{ origin.x - 1 + random.nextInt(2),
                               origin.y - random.nextInt(2),
                               origin.z - 1 + random.nextInt(2) };
        }
        return true;
    };
}

} // namespace mc::levelgen::feature

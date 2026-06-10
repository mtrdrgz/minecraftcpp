#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.CoralFeature (+ the
// CoralTree/CoralClaw/CoralMushroom subtypes) and SeaPickleFeature (26.1.2).
//
// Registry.getRandomElementOf(tag, random) == HolderSet.getRandomElement ==
// list.get(random.nextInt(size)) over the tag's value list in TAG-FILE ORDER
// (HolderSet.ListBacked.getRandomElement; the tag binder resolves "#nested"
// refs in place with LinkedHashSet dedup — FullChunkDecorateParity.java
// bindVanillaBlockTags is the certified reference). The orderedTags vectors
// passed in MUST follow that order:
//   #coral_blocks (5), #corals (= #coral_plants(5) + 5 fans), #wall_corals (5).

#include "../placement/PlacementContext.h"
#include "../placement/PlacedFeature.h"
#include "../RandomSource.h"
#include "../Heightmap.h"
#include "TreeFeature.h"   // javaShuffle, treeRelative, TREE_DIR_*
#include "../../../../core/Math.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

struct CoralHooks {
    std::vector<std::string> coralBlocksTag;   // #coral_blocks in tag-file order
    std::vector<std::string> coralsTag;        // #corals in tag-file order
    std::vector<std::string> wallCoralsTag;    // #wall_corals in tag-file order
    std::function<bool(const std::string&)> isCoralsTag;   // membership (#corals)
    // Wall-fan FACING and sea-pickle PICKLES are id-invisible; the harness keeps
    // a facing side map for the wall fans (updateShape revalidation).
    std::function<void(BlockPos, int)> putWallFanFacing;
};

namespace coral_detail {

// CoralFeature.placeCoralBlock (CoralFeature.java:37-71).
inline bool placeCoralBlock(WorldGenLevel& level, const CoralHooks& hooks, RandomSource& random,
                            BlockPos pos, const std::string& state) {
    static constexpr int HORIZONTAL[4] = { 2, 5, 3, 4 };   // N, E, S, W
    const BlockPos above{ pos.x, pos.y + 1, pos.z };
    const std::string target = level.getBlockState(pos);
    if ((target == "minecraft:water" || hooks.isCoralsTag(target))
        && level.getBlockState(above) == "minecraft:water") {
        level.setBlock(pos, state, 3);
        if (random.nextFloat() < 0.25f) {
            // getRandomElementOf(#corals): one nextInt(size) draw, always present.
            const std::string pick = hooks.coralsTag[static_cast<std::size_t>(
                random.nextInt(static_cast<int>(hooks.coralsTag.size())))];
            level.setBlock(above, pick, 2);
        } else if (random.nextFloat() < 0.05f) {
            (void)random.nextInt(4);   // SeaPickleBlock.PICKLES = nextInt(4)+1 (id-invisible)
            level.setBlock(above, "minecraft:sea_pickle", 2);
        }
        for (int direction : HORIZONTAL) {
            if (random.nextFloat() < 0.2f) {
                const BlockPos relativePos = treeRelative(pos, direction);
                if (level.getBlockState(relativePos) == "minecraft:water") {
                    const std::string fan = hooks.wallCoralsTag[static_cast<std::size_t>(
                        random.nextInt(static_cast<int>(hooks.wallCoralsTag.size())))];
                    if (level.setBlockChecked(relativePos, fan, 2)) {
                        hooks.putWallFanFacing(relativePos, direction);   // FACING side map
                    }
                }
            }
        }
        return true;
    }
    return false;
}

} // namespace coral_detail

// CoralFeature.place (CoralFeature.java:26-33) dispatching to the shape fns.
// kind: 0 = tree, 1 = claw, 2 = mushroom.
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeCoralPlacer(
        int kind, std::shared_ptr<const CoralHooks> hooks) {
    return [kind, hooks = std::move(hooks)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        using namespace coral_detail;
        static constexpr int HORIZONTAL[4] = { 2, 5, 3, 4 };       // N, E, S, W
        static constexpr int CLOCKWISE[6] = { 0, 1, 5, 4, 2, 3 };  // N->E, S->W, W->N, E->S
        static constexpr int COUNTERCW[6] = { 0, 1, 4, 5, 3, 2 };  // N->W, S->E, W->S, E->N
        // getRandomElementOf(#coral_blocks): one nextInt(5).
        const std::string state = hooks->coralBlocksTag[static_cast<std::size_t>(
            random.nextInt(static_cast<int>(hooks->coralBlocksTag.size())))];

        if (kind == 0) {
            // CoralTreeFeature.placeFeature (CoralTreeFeature.java:19-50).
            BlockPos mutPos = origin;
            const int trunckHeight = random.nextInt(3) + 1;
            for (int i = 0; i < trunckHeight; ++i) {
                if (!placeCoralBlock(level, *hooks, random, mutPos, state)) return true;
                mutPos = BlockPos{ mutPos.x, mutPos.y + 1, mutPos.z };
            }
            const BlockPos trunckTopPos = mutPos;
            const int nBranches = random.nextInt(3) + 2;
            // Direction.Plane.HORIZONTAL.shuffledCopy(random) = Util.shuffledCopy
            // of {N,E,S,W} (3 draws).
            std::vector<int> directions = { 2, 5, 3, 4 };
            javaShuffle(directions, random);
            for (int b = 0; b < nBranches && b < static_cast<int>(directions.size()); ++b) {
                const int branchDirection = directions[static_cast<std::size_t>(b)];
                mutPos = treeRelative(trunckTopPos, branchDirection);
                const int branchHeight = random.nextInt(5) + 2;
                int segmentLength = 0;
                for (int j = 0; j < branchHeight && placeCoralBlock(level, *hooks, random, mutPos, state); ++j) {
                    ++segmentLength;
                    mutPos = BlockPos{ mutPos.x, mutPos.y + 1, mutPos.z };
                    if (j == 0 || (segmentLength >= 2 && random.nextFloat() < 0.25f)) {
                        mutPos = treeRelative(mutPos, branchDirection);
                        segmentLength = 0;
                    }
                }
            }
            return true;
        }
        if (kind == 1) {
            // CoralClawFeature.placeFeature (CoralClawFeature.java:21-67).
            if (!placeCoralBlock(level, *hooks, random, origin, state)) return false;
            const int clawDirection = HORIZONTAL[random.nextInt(4)];
            const int nBranches = random.nextInt(2) + 2;
            // Util.toShuffledList(Stream.of(claw, clockwise, counterclockwise)): 2 draws.
            std::vector<int> possibleDirections = { clawDirection, CLOCKWISE[clawDirection], COUNTERCW[clawDirection] };
            javaShuffle(possibleDirections, random);
            for (int b = 0; b < nBranches && b < static_cast<int>(possibleDirections.size()); ++b) {
                const int branchDirection = possibleDirections[static_cast<std::size_t>(b)];
                BlockPos mutPos = origin;
                const int sidewayLength = random.nextInt(2) + 1;
                mutPos = treeRelative(mutPos, branchDirection);
                int inwayLenth;
                int segmentDirection;
                if (branchDirection == clawDirection) {
                    segmentDirection = clawDirection;
                    inwayLenth = random.nextInt(3) + 2;
                } else {
                    mutPos = BlockPos{ mutPos.x, mutPos.y + 1, mutPos.z };
                    const int segmentPossibleDirections[2] = { branchDirection, 1 };   // {dir, UP}
                    segmentDirection = segmentPossibleDirections[random.nextInt(2)];   // Util.getRandom
                    inwayLenth = random.nextInt(3) + 3;
                }
                for (int i = 0; i < sidewayLength && placeCoralBlock(level, *hooks, random, mutPos, state); ++i) {
                    mutPos = treeRelative(mutPos, segmentDirection);
                }
                static constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };
                mutPos = treeRelative(mutPos, OPP[segmentDirection]);
                mutPos = BlockPos{ mutPos.x, mutPos.y + 1, mutPos.z };
                for (int i = 0; i < inwayLenth; ++i) {
                    mutPos = treeRelative(mutPos, clawDirection);
                    if (!placeCoralBlock(level, *hooks, random, mutPos, state)) break;
                    if (random.nextFloat() < 0.25f) {
                        mutPos = BlockPos{ mutPos.x, mutPos.y + 1, mutPos.z };
                    }
                }
            }
            return true;
        }
        // CoralMushroomFeature.placeFeature (CoralMushroomFeature.java:17-41).
        const int height = random.nextInt(3) + 3;
        const int width = random.nextInt(3) + 3;
        const int length = random.nextInt(3) + 3;
        const int sinkValue = random.nextInt(3) + 1;
        for (int x = 0; x <= width; ++x) {
            for (int y = 0; y <= height; ++y) {
                for (int z = 0; z <= length; ++z) {
                    const BlockPos mutPos{ x + origin.x, y + origin.y - sinkValue, z + origin.z };
                    if (((x != 0 && x != width) || (y != 0 && y != height))
                        && ((z != 0 && z != length) || (y != 0 && y != height))
                        && ((x != 0 && x != width) || (z != 0 && z != length))
                        && (x == 0 || x == width || y == 0 || y == height || z == 0 || z == length)
                        && !(random.nextFloat() < 0.1f)
                        && !placeCoralBlock(level, *hooks, random, mutPos, state)) {
                        // empty body: short-circuit evaluation only (verbatim)
                    }
                }
            }
        }
        return true;
    };
}

// SeaPickleFeature.place (SeaPickleFeature.java:19-41): per attempt the x/z
// spread (4 draws), the OCEAN_FLOOR height read, then the PICKLES nextInt(4)
// draw UNCONDITIONALLY (the state is built before the gates).
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeSeaPicklePlacer(
        mc::valueproviders::IntProviderPtr count) {
    return [count = std::move(count)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        int placed = 0;
        const int n = count->sample(random);
        for (int i = 0; i < n; ++i) {
            const int x = random.nextInt(8) - random.nextInt(8);
            const int z = random.nextInt(8) - random.nextInt(8);
            const int y = level.getHeight(Heightmap::Types::OCEAN_FLOOR, origin.x + x, origin.z + z);
            const BlockPos picklePos{ origin.x + x, y, origin.z + z };
            (void)random.nextInt(4);   // SeaPickleBlock.PICKLES = nextInt(4)+1 (id-invisible)
            if (level.getBlockState(picklePos) == "minecraft:water"
                && level.canSurvive("minecraft:sea_pickle", picklePos)) {
                level.setBlock(picklePos, "minecraft:sea_pickle", 2);
                ++placed;
            }
        }
        return placed > 0;
    };
}

} // namespace mc::levelgen::feature

#pragma once

// 1:1 ports of the lush-caves vegetal features:
//   VegetationPatchFeature            ("vegetation_patch": moss_patch, moss_patch_ceiling,
//                                      clay_with_dripleaves)
//   WaterloggedVegetationPatchFeature ("waterlogged_vegetation_patch": clay_pool_with_dripleaves)
//   RootSystemFeature                 ("root_system": rooted_azalea_tree)
//   VinesFeature                      ("vines")
//   RandomBooleanSelectorFeature      ("random_boolean_selector": lush_caves_clay)
//
// HashSet-order sensitivity: VegetationPatchFeature collects the ground surface in
// a java.util.HashSet and iterates it in distributeVegetation, drawing one
// nextFloat per element (VegetationPatchFeature.java:87-101) — replicated with
// JavaBlockPosHashSet.javaOrder() (TreeFeature.h). The waterlogged subclass builds
// a SECOND HashSet from that iteration (WaterloggedVegetationPatchFeature.java:33-40)
// whose own iteration order drives the vegetation draws.

#include "TreeFeature.h"   // WorldGenLevel, BlockPos, JavaBlockPosHashSet, treeRelative
#include "DiskFeature.h"   // DiskStateProvider
#include "../IntProvider.h"
#include "../placement/PlacedFeature.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mc::levelgen::feature {

// Level/tag surface shared by the cave features.
struct CaveFeatureHooks {
    std::function<bool(const std::string&)> isAir;
    // BlockStateBase.isFaceSturdy(level, pos, direction) with SupportType.FULL
    // (the support shape — LeavesBlock overrides it to EMPTY).
    std::function<bool(const std::string&, int /*dirIndex*/)> isFaceSturdyFull;
    std::function<bool(const std::string&)> isSolid;          // BlockStateBase.isSolid
    std::function<bool(const std::string&)> isWaterFluid;     // fluid is(FluidTags.WATER)
    std::function<bool(const std::string&)> isLavaFluid;      // fluid is(FluidTags.LAVA)
};

// ---------------------------------------------------------------------------
// VegetationPatchFeature (VegetationPatchFeature.java).
// RNG (place, :22-33): xzRadius.sample x2 (+1 each); placeGroundPatch (:35-85):
// per non-corner cell — edge cells (non-corner) gated by ONE nextFloat when
// extraEdgeColumnChance != 0 (the Java condition draws only for edge cells) —
// then on a valid empty/sturdy surface: depth.sample + (extraBottomBlockChance>0
// ? one nextFloat : none); placeGround draws only via groundState.getState
// (simple providers: none). distributeVegetation (:87-101): per surface pos in
// HashSet order one nextFloat; passes run the nested placed feature.
struct VegetationPatchConfig {
    std::function<bool(const std::string&)> replaceable;      // #replaceable tag test
    DiskStateProvider groundState;
    std::shared_ptr<mc::levelgen::placement::PlacedFeature> vegetationFeature;
    int surfaceDirection = 0;                                 // CaveSurface: floor -> DOWN(0), ceiling -> UP(1)
    mc::valueproviders::IntProviderPtr depth;
    float extraBottomBlockChance = 0.0f;
    int verticalRange = 0;
    float vegetationChance = 0.0f;
    mc::valueproviders::IntProviderPtr xzRadius;
    float extraEdgeColumnChance = 0.0f;
    bool waterlogged = false;                                 // waterlogged_vegetation_patch
    int genMinY = 0, genDepth = 0;                            // nested PlacedFeature::place args
};

namespace cave_detail {

// VegetationPatchFeature.placeGround (:113-135).
inline bool vegetationPatchPlaceGround(const VegetationPatchConfig& cfg, WorldGenLevel& level,
                                       RandomSource& random, BlockPos belowPosIn, int depth) {
    BlockPos belowPos = belowPosIn;
    for (int i = 0; i < depth; ++i) {
        const std::string stateToPlace = cfg.groundState(level, random, belowPos).value();
        const std::string belowState = level.getBlockState(belowPos);
        if (stateToPlace != belowState) {   // !stateToPlace.is(belowState.getBlock()): id compare
            if (!cfg.replaceable(belowState)) {
                return i != 0;
            }
            level.setBlock(belowPos, stateToPlace, 2);
            belowPos = treeRelative(belowPos, cfg.surfaceDirection);
        }
    }
    return true;
}

// VegetationPatchFeature.placeGroundPatch (:35-85). Returns the surface positions
// in INSERTION order inside the JavaBlockPosHashSet.
inline JavaBlockPosHashSet vegetationPatchPlaceGroundPatch(
        const VegetationPatchConfig& cfg, const CaveFeatureHooks& hooks, WorldGenLevel& level,
        RandomSource& random, BlockPos origin, int xRadius, int zRadius) {
    const int inwards = cfg.surfaceDirection;                 // surface.getDirection()
    const int outwards = inwards == 0 ? 1 : 0;                // opposite
    JavaBlockPosHashSet surface;
    for (int dx = -xRadius; dx <= xRadius; ++dx) {
        const bool isXEdge = dx == -xRadius || dx == xRadius;
        for (int dz = -zRadius; dz <= zRadius; ++dz) {
            const bool isZEdge = dz == -zRadius || dz == zRadius;
            const bool isEdge = isXEdge || isZEdge;
            const bool isCorner = isXEdge && isZEdge;
            const bool isEdgeButNotCorner = isEdge && !isCorner;
            // (!isEdgeButNotCorner || extraEdgeColumnChance != 0 && !(nextFloat() > chance))
            if (isCorner) continue;
            if (isEdgeButNotCorner
                && !(cfg.extraEdgeColumnChance != 0.0f && !(random.nextFloat() > cfg.extraEdgeColumnChance))) {
                continue;
            }
            BlockPos pos{ origin.x + dx, origin.y, origin.z + dz };
            for (int offset = 0; hooks.isAir(level.getBlockState(pos)) && offset < cfg.verticalRange; ++offset) {
                pos = treeRelative(pos, inwards);
            }
            for (int offset = 0; !hooks.isAir(level.getBlockState(pos)) && offset < cfg.verticalRange; ++offset) {
                pos = treeRelative(pos, outwards);
            }
            const BlockPos belowPos = treeRelative(pos, inwards);
            const std::string belowState = level.getBlockState(belowPos);
            if (std::getenv("MCPP_VEGPATCH_DEBUG") != nullptr) {
                fprintf(stderr, "VEGCOL\t%d\t%d\torigin=%d,%d,%d\tpos=%d,%d,%d\tbelow=%s\tempty=%d\tsturdy=%d\n",
                        dx, dz, origin.x, origin.y, origin.z, pos.x, pos.y, pos.z, belowState.c_str(),
                        level.isEmptyBlock(pos) ? 1 : 0, hooks.isFaceSturdyFull(belowState, outwards) ? 1 : 0);
            }
            // isFaceSturdy(level, belowPos, surfaceDirection.getOpposite())
            if (level.isEmptyBlock(pos) && hooks.isFaceSturdyFull(belowState, outwards)) {
                const int depth = cfg.depth->sample(random)
                    + (cfg.extraBottomBlockChance > 0.0f && random.nextFloat() < cfg.extraBottomBlockChance ? 1 : 0);
                const BlockPos groundPos = belowPos;
                if (vegetationPatchPlaceGround(cfg, level, random, belowPos, depth)) {
                    surface.add(groundPos);
                    if (std::getenv("MCPP_VEGPATCH_DEBUG") != nullptr)
                        fprintf(stderr, "VEGADD %d,%d,%d\n", groundPos.x, groundPos.y, groundPos.z);
                }
            }
        }
    }
    return surface;
}

// WaterloggedVegetationPatchFeature.isExposed (:49-60): N, E, S, W, DOWN faces.
inline bool waterloggedIsExposed(const CaveFeatureHooks& hooks, WorldGenLevel& level, BlockPos pos) {
    static constexpr int DIRS[5] = { 2, 5, 3, 4, 0 };   // NORTH, EAST, SOUTH, WEST, DOWN
    for (int d : DIRS) {
        const BlockPos testPos = treeRelative(pos, d);
        static constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };
        if (!hooks.isFaceSturdyFull(level.getBlockState(testPos), OPP[d])) {
            return true;
        }
    }
    return false;
}

} // namespace cave_detail

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeVegetationPatchPlacer(
        std::shared_ptr<const VegetationPatchConfig> config, std::shared_ptr<const CaveFeatureHooks> hooks) {
    return [config = std::move(config), hooks = std::move(hooks)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        using namespace cave_detail;
        const VegetationPatchConfig& cfg = *config;
        const int xRadius = cfg.xzRadius->sample(random) + 1;
        const int zRadius = cfg.xzRadius->sample(random) + 1;
        JavaBlockPosHashSet surfaceSet = vegetationPatchPlaceGroundPatch(cfg, *hooks, level, random, origin, xRadius, zRadius);
        std::vector<BlockPos> surface = surfaceSet.javaOrder();

        if (cfg.waterlogged) {
            // WaterloggedVegetationPatchFeature.placeGroundPatch (:22-47): filter the
            // HashSet in ITS iteration order into a new HashSet, write water, return it.
            JavaBlockPosHashSet waterSet;
            for (const BlockPos& p : surface) {
                if (!waterloggedIsExposed(*hooks, level, p)) waterSet.add(p);
            }
            std::vector<BlockPos> waterSurface = waterSet.javaOrder();
            for (const BlockPos& p : waterSurface) {
                level.setBlock(p, "minecraft:water", 2);
            }
            surface = std::move(waterSurface);
            // The empty() result drives the feature's return (surface.isEmpty()).
        }

        // distributeVegetation (:87-101).
        const int outwards = cfg.surfaceDirection == 0 ? 1 : 0;
        const bool vegdbg = std::getenv("MCPP_VEGPATCH_DEBUG") != nullptr;
        if (vegdbg) fprintf(stderr, "VEGSET origin=%d,%d,%d n=%zu\n", origin.x, origin.y, origin.z, surface.size());
        for (const BlockPos& surfacePos : surface) {
            const bool roll = cfg.vegetationChance > 0.0f && random.nextFloat() < cfg.vegetationChance;
            if (vegdbg) fprintf(stderr, "VEGROLL %d,%d,%d %d\n", surfacePos.x, surfacePos.y, surfacePos.z, roll ? 1 : 0);
            if (roll) {
                if (cfg.waterlogged) {
                    // WaterloggedVegetationPatchFeature.placeVegetation (:62-80):
                    // super.placeVegetation at surfacePos.below() -> the nested feature
                    // runs AT surfacePos (below().relative(UP)); on success the placed
                    // state's WATERLOGGED setValue is id-invisible (no extra draws).
                    const BlockPos below{ surfacePos.x, surfacePos.y - 1, surfacePos.z };
                    cfg.vegetationFeature->place(level, random,
                        treeRelative(below, outwards), cfg.genMinY, cfg.genDepth);
                } else {
                    cfg.vegetationFeature->place(level, random,
                        treeRelative(surfacePos, outwards), cfg.genMinY, cfg.genDepth);
                }
            }
        }
        return !surface.empty();
    };
}

// ---------------------------------------------------------------------------
// RootSystemFeature (RootSystemFeature.java).
// RNG (place, :19-36): only when origin is air. placeDirtAndTree (:61-85): per
// upward step, on the allowed+space gate the nested TREE feature runs (its own
// draws); success then placeDirt -> per dirt row placeRootedDirt (:99-121):
// rootPlacementAttempts x (4x nextInt(rootRadius) + provider draw on replaceable);
// then placeRoots (:123-143): hangingRootPlacementAttempts x (6x nextInt + on empty
// target provider.getState draw + canSurvive/sturdy gates).
struct RootSystemConfig {
    std::shared_ptr<mc::levelgen::placement::PlacedFeature> treeFeature;
    int requiredVerticalSpaceForTree = 0;
    int allowedVerticalWaterForTree = 0;
    std::function<bool(WorldGenLevel&, BlockPos)> allowedTreePosition;
    int rootRadius = 0;
    std::function<bool(const std::string&)> rootReplaceable;   // #root_replaceable tag
    DiskStateProvider rootStateProvider;
    int rootPlacementAttempts = 0;
    int rootColumnMaxHeight = 0;
    int hangingRootRadius = 0;
    int hangingRootsVerticalSpan = 0;
    DiskStateProvider hangingRootStateProvider;
    int hangingRootPlacementAttempts = 0;
    int genMinY = 0, genDepth = 0;
};

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeRootSystemPlacer(
        std::shared_ptr<const RootSystemConfig> config, std::shared_ptr<const CaveFeatureHooks> hooks) {
    return [config = std::move(config), hooks = std::move(hooks)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        const RootSystemConfig& cfg = *config;
        const CaveFeatureHooks& h = *hooks;
        if (!h.isAir(level.getBlockState(origin))) {
            return false;
        }
        // spaceForTree (:38-50) + isAllowedTreeSpace (:52-59).
        auto spaceForTree = [&](BlockPos pos) {
            BlockPos columnUp = pos;
            for (int i = 1; i <= cfg.requiredVerticalSpaceForTree; ++i) {
                columnUp = BlockPos{ columnUp.x, columnUp.y + 1, columnUp.z };
                const std::string state = level.getBlockState(columnUp);
                if (h.isAir(state)) continue;
                const int blocksAboveGround = i + 1;
                if (!(blocksAboveGround <= cfg.allowedVerticalWaterForTree && h.isWaterFluid(state))) {
                    return false;
                }
            }
            return true;
        };
        // placeRootedDirt (:99-121).
        auto placeRootedDirt = [&](int originX, int originZ, int y) {
            BlockPos workingPos{ originX, y, originZ };
            for (int i = 0; i < cfg.rootPlacementAttempts; ++i) {
                workingPos = BlockPos{ workingPos.x + random.nextInt(cfg.rootRadius) - random.nextInt(cfg.rootRadius),
                                       workingPos.y,
                                       workingPos.z + random.nextInt(cfg.rootRadius) - random.nextInt(cfg.rootRadius) };
                if (cfg.rootReplaceable(level.getBlockState(workingPos))) {
                    level.setBlock(workingPos, cfg.rootStateProvider(level, random, workingPos).value(), 2);
                }
                workingPos = BlockPos{ originX, workingPos.y, originZ };
            }
        };
        // placeDirtAndTree (:61-85).
        bool treePlaced = false;
        BlockPos workingPos = origin;
        for (int y = 0; y < cfg.rootColumnMaxHeight; ++y) {
            workingPos = BlockPos{ workingPos.x, workingPos.y + 1, workingPos.z };
            if (cfg.allowedTreePosition(level, workingPos) && spaceForTree(workingPos)) {
                const BlockPos belowPos{ workingPos.x, workingPos.y - 1, workingPos.z };
                const std::string below = level.getBlockState(belowPos);
                if (h.isLavaFluid(below) || !h.isSolid(below)) {
                    break;   // return false from placeDirtAndTree
                }
                if (cfg.treeFeature->place(level, random, workingPos, cfg.genMinY, cfg.genDepth)) {
                    // placeDirt (:87-97): rows origin.y .. origin.y + y - 1.
                    const int targetHeight = origin.y + y;
                    for (int rowY = origin.y; rowY < targetHeight; ++rowY) {
                        placeRootedDirt(origin.x, origin.z, rowY);
                    }
                    treePlaced = true;
                    break;
                }
            }
        }
        if (treePlaced) {
            // placeRoots (:123-143).
            for (int i = 0; i < cfg.hangingRootPlacementAttempts; ++i) {
                const BlockPos workingPos2{
                    origin.x + random.nextInt(cfg.hangingRootRadius) - random.nextInt(cfg.hangingRootRadius),
                    origin.y + random.nextInt(cfg.hangingRootsVerticalSpan) - random.nextInt(cfg.hangingRootsVerticalSpan),
                    origin.z + random.nextInt(cfg.hangingRootRadius) - random.nextInt(cfg.hangingRootRadius) };
                if (level.isEmptyBlock(workingPos2)) {
                    const std::string targetState = cfg.hangingRootStateProvider(level, random, workingPos2).value();
                    // targetState.canSurvive && above isFaceSturdy DOWN
                    if (level.canSurvive(targetState, workingPos2)
                        && h.isFaceSturdyFull(level.getBlockState(BlockPos{ workingPos2.x, workingPos2.y + 1, workingPos2.z }), 0)) {
                        level.setBlock(workingPos2, targetState, 2);
                    }
                }
            }
        }
        return true;
    };
}

// ---------------------------------------------------------------------------
// VinesFeature (VinesFeature.java:16-33): on an empty origin, the first
// non-DOWN direction with an acceptable neighbour gets a single-face vine.
// VineBlock.isAcceptableNeighbour = MultifaceBlock.canAttachTo (VineBlock.java:118-120).
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeVinesPlacer(
        std::function<bool(WorldGenLevel&, BlockPos /*neighbour*/, int /*direction*/)> isAcceptableNeighbour,
        std::function<void(BlockPos, int)> putVineFace) {
    return [isAcceptableNeighbour = std::move(isAcceptableNeighbour), putVineFace = std::move(putVineFace)](
               WorldGenLevel& level, RandomSource&, BlockPos origin) -> bool {
        if (!level.isEmptyBlock(origin)) {
            return false;
        }
        for (int direction = 0; direction < 6; ++direction) {   // Direction.values()
            if (direction == 0) continue;                       // != Direction.DOWN
            if (isAcceptableNeighbour(level, treeRelative(origin, direction), direction)) {
                if (level.setBlockChecked(origin, "minecraft:vine", 2)) {
                    putVineFace(origin, direction);
                }
                return true;
            }
        }
        return false;
    };
}

} // namespace mc::levelgen::feature

#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.MultifaceGrowthFeature
// (+ MultifaceGrowthConfiguration, MultifaceBlock placement semantics and
// MultifaceSpreader) for the glow_lichen configured feature.
//
// The engine grid stores block IDS (default states), but the algorithm is
// stateful on the multiface FACE properties and WATERLOGGED of already-placed
// lichen (hasFace gates both placement and spreading). The level therefore
// exposes a side-channel (MultifaceLevelHooks) tracking {faces, waterlogged} per
// lichen position, registered on every successful lichen write and invalidated
// by the level whenever another block overwrites the position.
//
// Direction model (ordinals == Direction.java:33-38):
//   0 DOWN(0,-1,0)  1 UP(0,1,0)  2 NORTH(0,0,-1)  3 SOUTH(0,0,1)
//   4 WEST(-1,0,0)  5 EAST(1,0,0);   opposite pairs 0<->1, 2<->3, 4<->5;
//   axes: Y,Y,Z,Z,X,X.
//
// RNG order (all draws via Util.shuffle, Util.java:1061-1068: for i=size..2
// swap(list[i-1], list[nextInt(i)])):
//   1. config.getShuffledDirections(random): shuffledCopy of validDirections.
//      glow_lichen config (can_place_on_ceiling, !floor, wall): validDirections =
//      [UP, NORTH, EAST, SOUTH, WEST] (MultifaceGrowthConfiguration ctor: ceiling
//      -> UP, floor -> DOWN, wall -> Plane.HORIZONTAL = NORTH,EAST,SOUTH,WEST,
//      Direction.java:576) -> nextInt(5),nextInt(4),nextInt(3),nextInt(2).
//   2. on a successful growth placement: one nextFloat vs chance_of_spreading
//      (MultifaceGrowthFeature.java:75); if it passes, Direction.allShuffled =
//      shuffledCopy(values() = [DOWN,UP,NORTH,SOUTH,WEST,EAST]) -> nextInt(6)..
//      nextInt(2) (MultifaceSpreader.java:56).
//   3. per searchDirection (only reached when step 2's branch did not return):
//      getShuffledDirectionsExcept(random, searchDirection.getOpposite()) ->
//      shuffle of the filtered list (Util.toShuffledList): 4 elements when the
//      opposite is in validDirections (3 draws), else 5 (4 draws).
//
// NOTE MultifaceGrowthFeature.java:40 reads `pos.setWithOffset(origin,
// searchDirection)` INSIDE the i-loop — the probed position is origin+dir for
// every i (it does not walk outward). Replicated verbatim.
//
// canAttachTo (MultifaceBlock.java:256-261): Block.isFaceFull(getBlockSupportShape,
// opposite) || Block.isFaceFull(getCollisionShape, opposite). BOTH disjuncts are
// face-exact ports (isFaceSturdyFull / isCollisionFaceFull): leaves attach via
// the COLLISION cube although their support shape is empty (vines hang from
// leaves); snow/mud/slabs attach on DOWN only; azalea on UP only.

#include "../placement/PlacementContext.h"   // WorldGenLevel
#include "../placement/PlacedFeature.h"      // FeaturePlacer
#include "../RandomSource.h"
#include "../../block/BlockBehaviour.h"      // isFaceSturdyUp (full-cube face test)
#include "../../material/Fluids.h"           // fluidStateOf (sculk spread fluid gate)
#include "../../../../core/Math.h"           // BlockPos

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

// A BlockState as seen by the multiface algorithm.
struct MultifaceBlockState {
    std::string block;            // block id ("minecraft:...")
    bool isPlaceBlock = false;    // block == the configured multiface block
    std::uint8_t faces = 0;       // bit i == Direction ordinal i face present
    bool waterlogged = false;
};

// Level side-channel for the face/waterlogged bits (see header comment).
struct MultifaceLevelHooks {
    std::function<MultifaceBlockState(BlockPos)> getState;
    std::function<void(BlockPos, std::uint8_t, bool)> putState;   // after successful write
    // Sculk-vein spreader config dependencies (SculkVeinSpreaderConfig): only
    // consulted when the configured block is minecraft:sculk_vein.
    std::function<bool(const std::string&)> isFireTag;        // #minecraft:fire
    std::function<bool(const std::string&)> canBeReplaced;    // BlockStateBase.canBeReplaced (#replaceable)
};

// Tracks blocks whose face-full answer had to be defaulted (must stay empty).
inline std::set<std::string>& multifaceAttachDefaulted() {
    static std::set<std::string> s;
    return s;
}

namespace multiface_detail {

constexpr int DIR_X[6] = { 0, 0, 0, 0, -1, 1 };
constexpr int DIR_Y[6] = { -1, 1, 0, 0, 0, 0 };
constexpr int DIR_Z[6] = { 0, 0, -1, 1, 0, 0 };
constexpr int DIR_OPPOSITE[6] = { 1, 0, 3, 2, 5, 4 };
constexpr int DIR_AXIS[6] = { 1, 1, 2, 2, 0, 0 };   // 0=X,1=Y,2=Z

inline BlockPos relative(BlockPos p, int dir) {
    return BlockPos{ p.x + DIR_X[dir], p.y + DIR_Y[dir], p.z + DIR_Z[dir] };
}

// Util.shuffle (Util.java:1061-1068).
inline void shuffle(std::vector<int>& list, RandomSource& random) {
    for (int i = static_cast<int>(list.size()); i > 1; --i) {
        const int swapTo = random.nextInt(i);
        std::swap(list[static_cast<std::size_t>(i - 1)], list[static_cast<std::size_t>(swapTo)]);
    }
}

// MultifaceGrowthFeature.isAirOrWater (:86-88).
inline bool isAirOrWater(const MultifaceBlockState& s) {
    return s.block == "minecraft:air" || s.block == "minecraft:water";
}

inline bool hasFace(const MultifaceBlockState& s, int dir) {
    // MultifaceBlock.hasFace (:245-248): getValueOrElse(property, false) — false
    // for any non-multiface state.
    return s.isPlaceBlock && ((s.faces >> dir) & 1) != 0;
}

// MultifaceBlock.canAttachTo (:256-261): faceDir is the face of the NEIGHBOUR
// pointing back at the attaching block (directionTowardsNeighbour.getOpposite()).
inline bool canAttachTo(WorldGenLevel& level, BlockPos neighbourPos, int faceDir) {
    const std::string neighbour = level.getBlockState(neighbourPos);
    bool defaulted = false;
    const bool full = mc::block::isFaceSturdyFull(neighbour, faceDir, &defaulted)
                   || mc::block::isCollisionFaceFull(neighbour, faceDir, &defaulted);
    if (defaulted) multifaceAttachDefaulted().insert(neighbour);
    return full;
}

struct Config {
    std::string blockId = "minecraft:glow_lichen";   // the MultifaceBlock being placed
    std::set<std::string> canBePlacedOn;
    int searchRange = 10;
    float chanceOfSpreading = 0.5f;
    std::vector<int> validDirections;     // construction order, see header
    MultifaceLevelHooks hooks;
    // MultifaceSpreader.SpreadConfig dispatch: glow_lichen uses the
    // DefaultSpreaderConfig; sculk_vein uses SculkVeinBlock.SculkVeinSpreaderConfig
    // (SculkVeinBlock.java:147-186) whose stateCanBeReplaced is the sculk variant.
    bool sculkVeinConfig = false;
    std::vector<int> spreadTypes = { 0, 1, 2 };   // DEFAULT_SPREAD_ORDER; sameSpace = {0}
    // Sculk-config dependencies (only consulted when sculkVeinConfig):
    std::function<bool(const std::string&)> isFireTag;        // #minecraft:fire
    std::function<bool(const std::string&)> canBeReplaced;    // BlockStateBase.canBeReplaced (#replaceable)
};

// MultifaceBlock.isValidStateForPlacement (:191-198). isFaceSupported is true for
// all six faces on GlowLichenBlock (MultifaceBlock.java:108 default).
inline bool isValidStateForPlacement(WorldGenLevel& level, const MultifaceBlockState& oldState,
                                     BlockPos placementPos, int placementDirection) {
    if (!oldState.isPlaceBlock || !hasFace(oldState, placementDirection)) {
        const BlockPos neighbourPos = relative(placementPos, placementDirection);
        static constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };
        return canAttachTo(level, neighbourPos, OPP[placementDirection]);
    }
    return false;
}

// MultifaceBlock.getStateForPlacement (:200-217). nullopt == null.
inline std::optional<MultifaceBlockState> getStateForPlacement(
        const Config& cfg, const MultifaceBlockState& oldState, WorldGenLevel& level,
        BlockPos placementPos, int placementDirection) {
    if (!isValidStateForPlacement(level, oldState, placementPos, placementDirection)) {
        return std::nullopt;
    }
    MultifaceBlockState newState;
    newState.block = cfg.blockId;
    newState.isPlaceBlock = true;
    if (oldState.isPlaceBlock) {
        newState = oldState;                                    // :208-209
    } else if (oldState.block == "minecraft:water") {
        newState.waterlogged = true;                            // :210-211 (source water)
    }                                                           // :212-213 default
    newState.faces = static_cast<std::uint8_t>(newState.faces | (1u << placementDirection));  // :216
    return newState;
}

// Write a multiface state through the level; register the side-map entry on
// success. Returns the WorldGenRegion.setBlock result.
inline bool writeLichen(const Config& cfg, WorldGenLevel& level, BlockPos pos,
                        const MultifaceBlockState& state, int flags) {
    const bool ok = level.setBlockChecked(pos, cfg.blockId, flags);
    if (ok) cfg.hooks.putState(pos, state.faces, state.waterlogged);
    return ok;
}

// MultifaceSpreader.DefaultSpreaderConfig.stateCanBeReplaced (:131-135).
inline bool stateCanBeReplacedDefault(const MultifaceBlockState& existing) {
    return existing.block == "minecraft:air" || existing.isPlaceBlock
        || existing.block == "minecraft:water";   // worldgen water is always a source
}

// SculkVeinBlock.SculkVeinSpreaderConfig.stateCanBeReplaced (SculkVeinBlock.java:156-178).
inline bool stateCanBeReplacedSculk(const Config& cfg, WorldGenLevel& level, BlockPos sourcePos,
                                    BlockPos placementPos, int placementDirection,
                                    const MultifaceBlockState& existing) {
    const std::string against = level.getBlockState(relative(placementPos, placementDirection));
    if (against == "minecraft:sculk" || against == "minecraft:sculk_catalyst"
        || against == "minecraft:moving_piston") {
        return false;
    }
    const int manhattan = std::abs(sourcePos.x - placementPos.x) + std::abs(sourcePos.y - placementPos.y)
                          + std::abs(sourcePos.z - placementPos.z);
    if (manhattan == 2) {
        const BlockPos neighbourPos = relative(sourcePos, DIR_OPPOSITE[placementDirection]);
        bool defaulted = false;
        const bool sturdy = mc::block::isFaceSturdyFull(
            level.getBlockState(neighbourPos), placementDirection, &defaulted);
        if (defaulted) multifaceAttachDefaulted().insert(level.getBlockState(neighbourPos));
        if (sturdy) return false;
    }
    const mc::material::FluidState fs = mc::material::fluidStateOf(existing.block);
    if (!fs.isEmpty() && fs.fluid != "minecraft:water") return false;
    if (cfg.isFireTag(existing.block)) return false;
    return cfg.canBeReplaced(existing.block) || stateCanBeReplacedDefault(existing);
}

// MultifaceSpreader.SpreadConfig.canSpreadInto dispatch (DefaultSpreaderConfig
// :138-142 / SculkVeinSpreaderConfig).
inline bool canSpreadInto(const Config& cfg, WorldGenLevel& level, BlockPos sourcePos,
                          BlockPos spreadPos, int spreadFace) {
    const MultifaceBlockState existing = cfg.hooks.getState(spreadPos);
    const bool replaceable = cfg.sculkVeinConfig
        ? stateCanBeReplacedSculk(cfg, level, sourcePos, spreadPos, spreadFace, existing)
        : stateCanBeReplacedDefault(existing);
    return replaceable
        && isValidStateForPlacement(level, existing, spreadPos, spreadFace);
}

// SculkVeinSpreaderConfig.isOtherBlockValidAsSource (SculkVeinBlock.java:183-185);
// the default config returns false (MultifaceSpreader.java:160-162).
inline bool isOtherBlockValidAsSource(const Config& cfg, const MultifaceBlockState& state) {
    return cfg.sculkVeinConfig && state.block != "minecraft:sculk_vein";
}

struct SpreadPos {
    BlockPos pos;
    int face;
};

// MultifaceSpreader.SpreadType.getSpreadPos (:190-211), DEFAULT_SPREAD_ORDER
// (:14-16) = SAME_POSITION, SAME_PLANE, WRAP_AROUND.
inline SpreadPos spreadPosFor(int type, BlockPos pos, int spreadDirection, int fromFace) {
    switch (type) {
        case 0: return { pos, spreadDirection };                                       // SAME_POSITION
        case 1: return { relative(pos, spreadDirection), fromFace };                   // SAME_PLANE
        default: return { relative(relative(pos, spreadDirection), fromFace),
                          DIR_OPPOSITE[spreadDirection] };                              // WRAP_AROUND
    }
}

// MultifaceSpreader.getSpreadFromFaceTowardDirection (:86-110).
inline std::optional<SpreadPos> getSpreadFromFaceTowardDirection(
        const Config& cfg, const MultifaceBlockState& state, WorldGenLevel& level,
        BlockPos pos, int startingFace, int spreadDirection) {
    if (DIR_AXIS[spreadDirection] == DIR_AXIS[startingFace]) return std::nullopt;       // :94-96
    if (isOtherBlockValidAsSource(cfg, state)
        || (hasFace(state, startingFace) && !hasFace(state, spreadDirection))) {        // :98
        for (int type : cfg.spreadTypes) {                                              // :99
            const SpreadPos sp = spreadPosFor(type, pos, spreadDirection, startingFace);
            if (canSpreadInto(cfg, level, pos, sp.pos, sp.face)) {                      // :101
                return sp;
            }
        }
    }
    return std::nullopt;
}

// MultifaceSpreader.SpreadConfig.canSpreadFrom (:164-166).
inline bool canSpreadFrom(const Config& cfg, const MultifaceBlockState& state, int face) {
    return isOtherBlockValidAsSource(cfg, state) || hasFace(state, face);
}

// MultifaceSpreader.spreadFromFaceTowardAllDirections (:64-72) + spreadAll (:46-52).
// Declared after spreadToFace below.

// MultifaceSpreader.spreadToFace (:112-115) + SpreadConfig.placeBlock (:168-179).
inline bool spreadToFace(const Config& cfg, WorldGenLevel& level, const SpreadPos& sp, bool postProcess) {
    const MultifaceBlockState oldState = cfg.hooks.getState(sp.pos);
    const std::optional<MultifaceBlockState> spreadState =
        getStateForPlacement(cfg, oldState, level, sp.pos, sp.face);
    if (!spreadState.has_value()) return false;
    if (postProcess) {
        // level.getChunk(pos).markPosForPostprocessing — BEFORE the (gated) write,
        // and independent of the write radius (MultifaceSpreader.java:171-175).
        level.markPosForPostprocessing(sp.pos);
    }
    return writeLichen(cfg, level, sp.pos, *spreadState, 2);
}

// MultifaceSpreader.spreadFromFaceTowardRandomDirection (:53-62): lazy findFirst
// over Direction.allShuffled — side effects per direction until the first success.
inline void spreadFromFaceTowardRandomDirection(const Config& cfg, const MultifaceBlockState& state,
                                                WorldGenLevel& level, BlockPos pos, int startingFace,
                                                RandomSource& random, bool postProcess) {
    std::vector<int> dirs = { 0, 1, 2, 3, 4, 5 };   // Direction.values() order
    shuffle(dirs, random);
    for (int spreadDirection : dirs) {
        const std::optional<SpreadPos> sp =
            getSpreadFromFaceTowardDirection(cfg, state, level, pos, startingFace, spreadDirection);
        if (sp.has_value() && spreadToFace(cfg, level, *sp, postProcess)) {
            return;   // findFirst
        }
    }
}

// MultifaceSpreader.spreadAll (:46-52): Direction.stream() (values() order) over
// canSpreadFrom faces, each spreading toward all 6 directions (:64-72); returns
// the number of successful placements.
inline long spreadAll(const Config& cfg, const MultifaceBlockState& state, WorldGenLevel& level,
                      BlockPos pos, bool postProcess) {
    long total = 0;
    for (int faceDirection = 0; faceDirection < 6; ++faceDirection) {
        if (!canSpreadFrom(cfg, state, faceDirection)) continue;
        for (int spreadDirection = 0; spreadDirection < 6; ++spreadDirection) {
            const std::optional<SpreadPos> sp =
                getSpreadFromFaceTowardDirection(cfg, state, level, pos, faceDirection, spreadDirection);
            if (sp.has_value() && spreadToFace(cfg, level, *sp, postProcess)) {
                ++total;
            }
        }
    }
    return total;
}

// MultifaceGrowthFeature.placeGrowthIfPossible (:55-84).
inline bool placeGrowthIfPossible(const Config& cfg, WorldGenLevel& level, BlockPos pos,
                                  const MultifaceBlockState& oldState, RandomSource& random,
                                  const std::vector<int>& placementDirections) {
    for (int placementDirection : placementDirections) {
        const MultifaceBlockState neighbour = cfg.hooks.getState(relative(pos, placementDirection));
        if (cfg.canBePlacedOn.count(neighbour.block) != 0) {                            // :67
            const std::optional<MultifaceBlockState> newState =
                getStateForPlacement(cfg, oldState, level, pos, placementDirection);    // :68
            if (!newState.has_value()) {
                return false;                                                           // :69-71
            }
            writeLichen(cfg, level, pos, *newState, 3);                                 // :73
            level.markPosForPostprocessing(pos);                                        // :74
            if (random.nextFloat() < cfg.chanceOfSpreading) {                           // :75
                spreadFromFaceTowardRandomDirection(cfg, *newState, level, pos,
                                                    placementDirection, random, true);  // :76
            }
            return true;                                                                // :79
        }
    }
    return false;
}

} // namespace multiface_detail

// blockId: minecraft:glow_lichen or minecraft:sculk_vein (the MultifaceSpreadeable
// blocks reachable in the overworld decoration set). NOTE the sculk_vein FEATURE
// placement (multiface_growth configured type) uses the same MultifaceBlock
// placement/spread semantics as glow_lichen — the vein-specific spreader config
// (SculkVeinSpreaderConfig) only applies to SculkSpreader cursors (SculkFeatures.h).
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeMultifaceGrowthPlacer(
        std::string blockId, std::set<std::string> canBePlacedOn, int searchRange,
        bool canPlaceOnFloor, bool canPlaceOnCeiling, bool canPlaceOnWall,
        float chanceOfSpreading, MultifaceLevelHooks hooks) {
    namespace d = multiface_detail;
    auto cfg = std::make_shared<d::Config>();
    cfg->blockId = std::move(blockId);
    cfg->canBePlacedOn = std::move(canBePlacedOn);
    cfg->searchRange = searchRange;
    cfg->chanceOfSpreading = chanceOfSpreading;
    cfg->hooks = std::move(hooks);
    if (cfg->blockId == "minecraft:sculk_vein") {
        // The sculk_vein placeBlock's getSpreader() is the SculkVeinSpreaderConfig
        // vein spreader (SculkVeinBlock.java:24-25, 39-41).
        cfg->sculkVeinConfig = true;
        cfg->isFireTag = cfg->hooks.isFireTag;
        cfg->canBeReplaced = cfg->hooks.canBeReplaced;
    }
    // MultifaceGrowthConfiguration ctor order: ceiling -> UP, floor -> DOWN,
    // wall -> Plane.HORIZONTAL {NORTH, EAST, SOUTH, WEST}.
    if (canPlaceOnCeiling) cfg->validDirections.push_back(1);
    if (canPlaceOnFloor) cfg->validDirections.push_back(0);
    if (canPlaceOnWall) { cfg->validDirections.push_back(2); cfg->validDirections.push_back(5);
                          cfg->validDirections.push_back(3); cfg->validDirections.push_back(4); }

    return [cfg](WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        const MultifaceBlockState originState = cfg->hooks.getState(origin);
        if (!d::isAirOrWater(originState)) {                                            // :24-26
            return false;
        }
        // getShuffledDirections (:28).
        std::vector<int> searchDirections = cfg->validDirections;
        d::shuffle(searchDirections, random);
        if (d::placeGrowthIfPossible(*cfg, level, origin, originState, random, searchDirections)) {
            return true;                                                                // :29-31
        }
        for (int searchDirection : searchDirections) {                                  // :35
            // getShuffledDirectionsExcept(random, searchDirection.getOpposite()) (:37).
            std::vector<int> placementDirections;
            for (int dir : cfg->validDirections) {
                if (dir != d::DIR_OPPOSITE[searchDirection]) placementDirections.push_back(dir);
            }
            d::shuffle(placementDirections, random);
            for (int i = 0; i < cfg->searchRange; ++i) {                                // :39
                // :40 — origin+dir each iteration (no outward walk), verbatim.
                const BlockPos pos = d::relative(origin, searchDirection);
                const MultifaceBlockState state = cfg->hooks.getState(pos);             // :41
                if (!d::isAirOrWater(state) && !state.isPlaceBlock) {                   // :42-44
                    break;
                }
                if (d::placeGrowthIfPossible(*cfg, level, pos, state, random, placementDirections)) {
                    return true;                                                        // :46-48
                }
            }
        }
        return false;
    };
}

} // namespace mc::levelgen::feature

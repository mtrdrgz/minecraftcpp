#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.SculkPatchFeature over
// net.minecraft.world.level.block.SculkSpreader (worldgen variant), SculkBlock,
// SculkVeinBlock and SculkBehaviour (26.1.2 sources).
//
// Worldgen spreader (SculkSpreader.createWorldGenSpreader, :72-74):
//   replaceable = #sculk_replaceable_world_gen, growthSpawnCost 50,
//   noGrowthRadius 1, chargeDecayRate 5, additionalDecayRate 10.
//
// The SculkBehaviour dispatch (ChargeCursor.getBlockBehaviour, :284-286): only
// SculkBlock (minecraft:sculk) and SculkVeinBlock (minecraft:sculk_vein) are
// `instanceof SculkBehaviour` among worldgen-reachable blocks (SculkCatalystBlock
// extends BaseEntityBlock WITHOUT the interface); everything else uses
// SculkBehaviour.DEFAULT (the anonymous instance, SculkBehaviour.java:13-44).
//
// Cursor movement neighbourhood (ChargeCursor.NON_CORNER_NEIGHBOURS, :196-203):
// BlockPos.betweenClosedStream((-1,-1,-1),(1,1,1)) in Cursor3D order (x fastest,
// then y, then z — Cursor3D.java:30-41) filtered to face/edge cells; the list
// below is that exact order. getValidMovementPos shuffles it (Util.shuffledCopy,
// 17 draws) every call.

#include "../placement/PlacementContext.h"
#include "../placement/PlacedFeature.h"
#include "../RandomSource.h"
#include "../IntProvider.h"
#include "MultifaceGrowthFeature.h"   // multiface_detail (spreadAll, faces, attach)
#include "TreeFeature.h"              // javaShuffle, treeRelative
#include "../../../../core/Math.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

struct SculkPatchConfig {
    int chargeCount = 0, amountPerCharge = 0, spreadAttempts = 0;
    int growthRounds = 0, spreadRounds = 0;
    mc::valueproviders::IntProviderPtr extraRareGrowths;
    float catalystChance = 0.0f;
};

struct SculkHooks {
    std::function<bool(const std::string&)> isAir;
    std::function<bool(const std::string&)> sculkReplaceableWorldGen;  // #sculk_replaceable_world_gen
    std::function<bool(const std::string&)> isCollisionShapeFullBlock; // BlockStateBase.isCollisionShapeFullBlock
    std::function<bool(const std::string&, int)> isFaceSturdy;         // isFaceSturdy(state, dir) FULL
    std::function<bool(const std::string&)> isWaterFluid;              // fluidState.is(WATER)
    // The two vein spreaders (SculkVeinBlock.java:24-28): same-space (SAME_POSITION
    // only) and the full vein spreader (DEFAULT_SPREAD_ORDER), both with the
    // SculkVeinSpreaderConfig replaced-state rules.
    std::shared_ptr<multiface_detail::Config> veinSpreader;
    std::shared_ptr<multiface_detail::Config> sameSpaceSpreader;
};

namespace sculk_detail {

// SculkSpreader.ChargeCursor.NON_CORNER_NEIGHBOURS in Cursor3D order.
inline const std::vector<BlockPos>& nonCornerNeighbours() {
    static const std::vector<BlockPos> offsets = [] {
        std::vector<BlockPos> v;
        for (int z = -1; z <= 1; ++z)
            for (int y = -1; y <= 1; ++y)
                for (int x = -1; x <= 1; ++x)
                    if ((x == 0 || y == 0 || z == 0) && !(x == 0 && y == 0 && z == 0))
                        v.push_back(BlockPos{ x, y, z });
        return v;
    }();
    return offsets;
}

struct ChargeCursor {
    BlockPos pos{};
    int charge = 0;
    int updateDelay = 0;
    int decayDelay = 1;
    std::optional<std::uint8_t> facings;   // null == not yet on a SculkBehaviour block
};

struct Spreader {
    // createWorldGenSpreader constants (SculkSpreader.java:72-74).
    static constexpr int growthSpawnCost = 50;
    static constexpr int noGrowthRadius = 1;
    static constexpr int chargeDecayRate = 5;
    static constexpr int additionalDecayRate = 10;
    std::vector<ChargeCursor> cursors;

    // SculkSpreader.addCursors (:128-138): 32-cursor cap.
    void addCursors(BlockPos startPos, int charge) {
        while (charge > 0) {
            const int currentCharge = std::min(charge, 1000);
            if (static_cast<int>(cursors.size()) < 32) {
                cursors.push_back(ChargeCursor{ startPos, currentCharge, 0, 1, std::nullopt });
            }
            charge -= currentCharge;
        }
    }
    void clear() { cursors.clear(); }
};

// MultifaceBlock.availableFaces (MultifaceBlock.java:70-84) at side-map level.
inline std::uint8_t availableFaces(const MultifaceBlockState& s) {
    return s.isPlaceBlock ? s.faces : 0;
}

// SculkVeinBlock.hasSubstrateAccess (SculkVeinBlock.java:131-145); worldgen
// queries #sculk_replaceable (the LEVEL tag, not the world-gen one) — Java uses
// BlockTags.SCULK_REPLACEABLE there unconditionally.
inline bool hasSubstrateAccess(WorldGenLevel& level, const SculkHooks& hooks,
                               const std::function<bool(const std::string&)>& sculkReplaceableLevelTag,
                               const MultifaceBlockState& state, BlockPos pos) {
    if (state.block != "minecraft:sculk_vein") return false;
    for (int dir = 0; dir < 6; ++dir) {
        if (multiface_detail::hasFace(state, dir)
            && sculkReplaceableLevelTag(level.getBlockState(multiface_detail::relative(pos, dir)))) {
            return true;
        }
    }
    (void)hooks;
    return false;
}

// SculkVeinBlock.regrow (SculkVeinBlock.java:47-69).
inline bool veinRegrow(WorldGenLevel& level, const SculkHooks& hooks, BlockPos pos,
                       const MultifaceBlockState& existing, std::uint8_t faces) {
    bool hasAtLeastOneFace = false;
    std::uint8_t newFaces = 0;
    for (int face = 0; face < 6; ++face) {
        if ((faces & (1u << face)) == 0) continue;
        static constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };
        if (multiface_detail::canAttachTo(level, multiface_detail::relative(pos, face), OPP[face])) {
            newFaces = static_cast<std::uint8_t>(newFaces | (1u << face));
            hasAtLeastOneFace = true;
        }
    }
    if (!hasAtLeastOneFace) return false;
    const bool waterlogged = !mc::material::fluidStateOf(existing.block).isEmpty();
    if (level.setBlockChecked(pos, "minecraft:sculk_vein", 3)) {
        hooks.veinSpreader->hooks.putState(pos, newFaces, waterlogged);
    }
    return true;
}

// SculkVeinBlock.onDischarged (SculkVeinBlock.java:70-87). `state` is the CALLER'S
// BlockState — the live read in attemptPlaceSculk's vein loop (SculkVeinBlock.java:
// 121-127), but the cursor update's START-OF-UPDATE SNAPSHOT in the charge<=0 and
// transfer paths (SculkSpreader.java:265,278,282) — attemptUseCharge's writes to
// this very cell are NOT visible to those onDischarged calls.
inline void veinOnDischarged(WorldGenLevel& level, const SculkHooks& hooks, BlockPos pos,
                             MultifaceBlockState state) {
    if (state.block != "minecraft:sculk_vein") return;
    std::uint8_t faces = state.faces;
    for (int dir = 0; dir < 6; ++dir) {
        if ((faces & (1u << dir)) != 0
            && level.getBlockState(multiface_detail::relative(pos, dir)) == "minecraft:sculk") {
            faces = static_cast<std::uint8_t>(faces & ~(1u << dir));
        }
    }
    if (faces == 0) {
        const std::string replacement = state.waterlogged ? "minecraft:water" : "minecraft:air";
        level.setBlockChecked(pos, replacement, 3);
    } else if (level.setBlockChecked(pos, "minecraft:sculk_vein", 3)) {
        hooks.veinSpreader->hooks.putState(pos, faces, state.waterlogged);
    }
}

// SculkBlock.canPlaceGrowth (SculkBlock.java:81-101).
inline bool canPlaceGrowth(WorldGenLevel& level, const SculkHooks& hooks, BlockPos pos) {
    const std::string above = level.getBlockState(BlockPos{ pos.x, pos.y + 1, pos.z });
    if (hooks.isAir(above) || (above == "minecraft:water" && hooks.isWaterFluid(above))) {
        int growthCount = 0;
        // BlockPos.betweenClosed(pos.offset(-4,0,-4), pos.offset(4,2,4)): count only.
        for (int z = pos.z - 4; z <= pos.z + 4; ++z)
            for (int y = pos.y; y <= pos.y + 2; ++y)
                for (int x = pos.x - 4; x <= pos.x + 4; ++x) {
                    const std::string s = level.getBlockState(BlockPos{ x, y, z });
                    if (s == "minecraft:sculk_sensor" || s == "minecraft:sculk_shrieker") ++growthCount;
                    if (growthCount > 2) return false;
                }
        return true;
    }
    return false;
}

// SculkBlock.getDecayPenalty (SculkBlock.java:60-66), float-exact.
inline int getDecayPenalty(BlockPos pos, BlockPos originPos, int charge) {
    const double dx = pos.x - originPos.x, dy = pos.y - originPos.y, dz = pos.z - originPos.z;
    const float dist = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz)) - Spreader::noGrowthRadius;
    const float outerDistanceSquared = dist * dist;   // Mth.square
    const int maxReachSquared = (24 - Spreader::noGrowthRadius) * (24 - Spreader::noGrowthRadius);
    const float distanceFactor = std::min(1.0f, outerDistanceSquared / static_cast<float>(maxReachSquared));
    return std::max(1, static_cast<int>(static_cast<float>(charge) * distanceFactor * 0.5f));
}

// SculkBlock.attemptUseCharge (SculkBlock.java:28-58).
inline int sculkBlockAttemptUseCharge(ChargeCursor& cursor, WorldGenLevel& level, const SculkHooks& hooks,
                                      BlockPos originPos, RandomSource& random) {
    const int charge = cursor.charge;
    if (charge != 0 && random.nextInt(Spreader::chargeDecayRate) == 0) {
        const BlockPos chargePos = cursor.pos;
        // closerThan(origin, noGrowthRadius): distSqr < r^2 (Vec3i.closerThan).
        const double ddx = chargePos.x - originPos.x, ddy = chargePos.y - originPos.y, ddz = chargePos.z - originPos.z;
        const bool isCloseToCatalyst = ddx * ddx + ddy * ddy + ddz * ddz
                                       < static_cast<double>(Spreader::noGrowthRadius) * Spreader::noGrowthRadius;
        if (!isCloseToCatalyst && canPlaceGrowth(level, hooks, chargePos)) {
            if (random.nextInt(Spreader::growthSpawnCost) < charge) {
                const BlockPos growthPlacement{ chargePos.x, chargePos.y + 1, chargePos.z };
                // getRandomGrowthState (SculkBlock.java:68-79): nextInt(11)==0 ->
                // shrieker[can_summon=true] else sensor; waterlogged is id-invisible.
                const std::string growthState = random.nextInt(11) == 0
                    ? "minecraft:sculk_shrieker" : "minecraft:sculk_sensor";
                level.setBlock(growthPlacement, growthState, 3);
            }
            return std::max(0, charge - Spreader::growthSpawnCost);
        }
        return random.nextInt(Spreader::additionalDecayRate) != 0
            ? charge
            : charge - (isCloseToCatalyst ? 1 : getDecayPenalty(chargePos, originPos, charge));
    }
    return charge;
}

// SculkVeinBlock.attemptPlaceSculk (SculkVeinBlock.java:110-129).
inline bool veinAttemptPlaceSculk(WorldGenLevel& level, const SculkHooks& hooks, BlockPos pos,
                                  RandomSource& random) {
    const MultifaceBlockState state = hooks.veinSpreader->hooks.getState(pos);
    std::vector<int> dirs = { 0, 1, 2, 3, 4, 5 };   // Direction.allShuffled
    javaShuffle(dirs, random);
    for (int support : dirs) {
        if (!multiface_detail::hasFace(state, support)) continue;
        const BlockPos supportPos = multiface_detail::relative(pos, support);
        const std::string supportState = level.getBlockState(supportPos);
        if (!hooks.sculkReplaceableWorldGen(supportState)) continue;
        level.setBlock(supportPos, "minecraft:sculk", 3);
        // Block.pushEntitiesUp: no entities during worldgen (hard no-op).
        multiface_detail::spreadAll(*hooks.veinSpreader,
                                    hooks.veinSpreader->hooks.getState(supportPos),
                                    level, supportPos, /*postProcess=*/true);
        const int skip = multiface_detail::DIR_OPPOSITE[support];
        for (int dir = 0; dir < 6; ++dir) {
            if (dir == skip) continue;
            const BlockPos veinPos = multiface_detail::relative(supportPos, dir);
            // possibleVeinBlock = a FRESH read per neighbour (SculkVeinBlock.java:122-126).
            const MultifaceBlockState possibleVein = hooks.veinSpreader->hooks.getState(veinPos);
            if (possibleVein.block == "minecraft:sculk_vein") {
                veinOnDischarged(level, hooks, veinPos, possibleVein);
            }
        }
        return true;
    }
    return false;
}

// ChargeCursor.isMovementUnobstructed (SculkSpreader.java:316-346).
inline bool isUnobstructed(WorldGenLevel& level, const SculkHooks& hooks, BlockPos from, int direction) {
    const BlockPos testPos = multiface_detail::relative(from, direction);
    static constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };
    return !hooks.isFaceSturdy(level.getBlockState(testPos), OPP[direction]);
}

inline bool isMovementUnobstructed(WorldGenLevel& level, const SculkHooks& hooks, BlockPos from, BlockPos to) {
    const int manhattan = std::abs(from.x - to.x) + std::abs(from.y - to.y) + std::abs(from.z - to.z);
    if (manhattan == 1) return true;
    const BlockPos delta{ to.x - from.x, to.y - from.y, to.z - from.z };
    const int directionX = delta.x < 0 ? 4 : 5;   // WEST : EAST
    const int directionY = delta.y < 0 ? 0 : 1;   // DOWN : UP
    const int directionZ = delta.z < 0 ? 2 : 3;   // NORTH : SOUTH
    if (delta.x == 0) {
        return isUnobstructed(level, hooks, from, directionY) || isUnobstructed(level, hooks, from, directionZ);
    }
    if (delta.y == 0) {
        return isUnobstructed(level, hooks, from, directionX) || isUnobstructed(level, hooks, from, directionZ);
    }
    return isUnobstructed(level, hooks, from, directionX) || isUnobstructed(level, hooks, from, directionY);
}

// ChargeCursor.getValidMovementPos (SculkSpreader.java:296-314).
inline std::optional<BlockPos> getValidMovementPos(WorldGenLevel& level, const SculkHooks& hooks,
                                                   const std::function<bool(const std::string&)>& sculkReplaceableLevelTag,
                                                   BlockPos pos, RandomSource& random) {
    BlockPos sculkPosition = pos;
    std::vector<BlockPos> offsets = nonCornerNeighbours();
    javaShuffle(offsets, random);   // Util.shuffledCopy (17 draws)
    for (const BlockPos& offset : offsets) {
        const BlockPos neighbour{ pos.x + offset.x, pos.y + offset.y, pos.z + offset.z };
        const std::string transferee = level.getBlockState(neighbour);
        // instanceof SculkBehaviour: sculk / sculk_vein.
        if ((transferee == "minecraft:sculk" || transferee == "minecraft:sculk_vein")
            && isMovementUnobstructed(level, hooks, pos, neighbour)) {
            sculkPosition = neighbour;
            if (hasSubstrateAccess(level, hooks, sculkReplaceableLevelTag,
                                   hooks.veinSpreader->hooks.getState(neighbour), neighbour)) {
                break;
            }
        }
    }
    return sculkPosition == pos ? std::nullopt : std::optional<BlockPos>(sculkPosition);
}

// ChargeCursor.update (SculkSpreader.java:236-282), worldgen arm (shouldUpdate
// is unconditionally true with charge > 0).
inline void cursorUpdate(ChargeCursor& cursor, WorldGenLevel& level, const SculkHooks& hooks,
                         const std::function<bool(const std::string&)>& sculkReplaceableLevelTag,
                         BlockPos originPos, RandomSource& random, bool spreadVeins) {
    if (cursor.charge <= 0) return;                 // shouldUpdate (:228-236)
    if (cursor.updateDelay > 0) {
        --cursor.updateDelay;
        return;
    }
    // `currentState` mirrors the Java local of the same name (SculkSpreader.java:265):
    // it is a SNAPSHOT — re-read ONLY after attemptSpreadVein (when the behaviour can
    // change the state, :268-271) and after a cursor transfer (:289), NOT after
    // attemptUseCharge (whose attemptPlaceSculk spreads can rewrite this very cell).
    // The final `facings = MultifaceBlock.availableFaces(currentState)` (:292-294)
    // therefore reads the (possibly stale) snapshot's faces, so the snapshot must
    // carry the full multiface state, not just the block id.
    MultifaceBlockState currentMf = hooks.veinSpreader->hooks.getState(cursor.pos);
    std::string currentState = currentMf.block;
    // getBlockBehaviour dispatch: 0 = DEFAULT, 1 = sculk, 2 = sculk_vein.
    auto behaviourOf = [](const std::string& s) {
        return s == "minecraft:sculk" ? 1 : s == "minecraft:sculk_vein" ? 2 : 0;
    };
    int behaviour = behaviourOf(currentState);

    // ---- attemptSpreadVein (SculkBehaviour.java:13-26 DEFAULT / :56-58 default) ----
    if (spreadVeins) {
        bool spread = false;
        if (behaviour == 0) {
            // SculkBehaviour.DEFAULT.attemptSpreadVein.
            if (!cursor.facings.has_value()) {
                spread = multiface_detail::spreadAll(*hooks.sameSpaceSpreader,
                                                     hooks.sameSpaceSpreader->hooks.getState(cursor.pos),
                                                     level, cursor.pos, /*postProcess=*/true) > 0;
            } else if (*cursor.facings != 0) {
                // `state` param = the update()'s currentState snapshot (SculkBehaviour
                // .java:21; SculkSpreader.java:267) — currentMf here (no writes since).
                spread = (hooks.isAir(currentMf.block) || hooks.isWaterFluid(currentMf.block))
                         && veinRegrow(level, hooks, cursor.pos, currentMf, *cursor.facings);
            } else {
                spread = multiface_detail::spreadAll(*hooks.veinSpreader, currentMf,
                                                     level, cursor.pos, /*postProcess=*/true) > 0;
            }
        } else {
            // SculkBlock / SculkVeinBlock inherit the interface default (:56-58),
            // spreading from the snapshot state param.
            spread = multiface_detail::spreadAll(*hooks.veinSpreader, currentMf,
                                                 level, cursor.pos, /*postProcess=*/true) > 0;
        }
        if (spread) {
            // canChangeBlockStateOnSpread: SculkBlock false (SculkBlock.java:103-106),
            // DEFAULT/vein true -> re-resolve the behaviour from the (possibly new) state.
            if (behaviour != 1) {
                currentMf = hooks.veinSpreader->hooks.getState(cursor.pos);
                currentState = currentMf.block;
                behaviour = behaviourOf(currentState);
            }
        }
    }

    // ---- attemptUseCharge ----
    if (behaviour == 1) {
        cursor.charge = sculkBlockAttemptUseCharge(cursor, level, hooks, originPos, random);
    } else if (behaviour == 2) {
        if (spreadVeins && veinAttemptPlaceSculk(level, hooks, cursor.pos, random)) {
            cursor.charge = cursor.charge - 1;
        } else {
            cursor.charge = random.nextInt(Spreader::chargeDecayRate) == 0
                ? static_cast<int>(std::floor(static_cast<float>(cursor.charge) * 0.5f))   // Mth.floor
                : cursor.charge;
        }
    } else {
        // DEFAULT.attemptUseCharge (SculkBehaviour.java:28-39).
        cursor.charge = cursor.decayDelay > 0 ? cursor.charge : 0;
    }

    if (cursor.charge <= 0) {
        // onDischarged(level, currentState, pos, random) — the START-OF-UPDATE
        // SNAPSHOT, not a live re-read (SculkSpreader.java:277-279).
        if (behaviour == 2) veinOnDischarged(level, hooks, cursor.pos, currentMf);
        return;
    }
    const std::optional<BlockPos> transferPos =
        getValidMovementPos(level, hooks, sculkReplaceableLevelTag, cursor.pos, random);
    if (transferPos.has_value()) {
        // onDischarged with the same snapshot (SculkSpreader.java:281-283).
        if (behaviour == 2) veinOnDischarged(level, hooks, cursor.pos, currentMf);
        cursor.pos = *transferPos;
        // worldgen distance clamp (SculkSpreader.java:265-269): closerThan over
        // (origin.x, pos.y, origin.z), i.e. horizontal distance only.
        const double dx = cursor.pos.x - originPos.x, dz = cursor.pos.z - originPos.z;
        if (!(dx * dx + dz * dz < 15.0 * 15.0)) {
            cursor.charge = 0;
            return;
        }
        currentMf = hooks.veinSpreader->hooks.getState(cursor.pos);
        currentState = currentMf.block;
    }
    if (currentState == "minecraft:sculk" || currentState == "minecraft:sculk_vein") {
        // MultifaceBlock.availableFaces(currentState) (SculkSpreader.java:292-294)
        // over the SNAPSHOT (see above) — sculk (no face properties) -> empty set.
        cursor.facings = currentMf.block == "minecraft:sculk_vein" ? availableFaces(currentMf) : 0;
    }
    // updateDecayDelay: DEFAULT max(age-1,0) only applies to the anon DEFAULT;
    // SculkBlock/SculkVeinBlock use the interface default (:67-69) == 1.
    cursor.decayDelay = behaviour == 0 ? std::max(cursor.decayDelay - 1, 0) : 1;
    cursor.updateDelay = 1;   // getSculkSpreadDelay (:45-47)
}

// SculkSpreader.updateCursors (SculkSpreader.java:140-194), worldgen arm: the
// merge path is disabled (`!this.isWorldGeneration() && ...`), so every cursor
// with charge > 0 survives in iteration order; the chargeMap/levelEvent block
// is particles only (hard no-op).
inline void updateCursors(Spreader& spreader, WorldGenLevel& level, const SculkHooks& hooks,
                          const std::function<bool(const std::string&)>& sculkReplaceableLevelTag,
                          BlockPos originPos, RandomSource& random, bool spreadVeins) {
    if (spreader.cursors.empty()) return;
    std::vector<ChargeCursor> processed;
    for (ChargeCursor& cursor : spreader.cursors) {
        // isPosUnreasonable: distChessboard > 1024 — unreachable in a 3x3 region.
        cursorUpdate(cursor, level, hooks, sculkReplaceableLevelTag, originPos, random, spreadVeins);
        if (cursor.charge > 0) processed.push_back(cursor);
    }
    spreader.cursors = std::move(processed);
}

} // namespace sculk_detail

// SculkPatchFeature.place (SculkPatchFeature.java:22-66).
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeSculkPatchPlacer(
        std::shared_ptr<const SculkPatchConfig> config, std::shared_ptr<const SculkHooks> hooks,
        std::function<bool(const std::string&)> sculkReplaceableLevelTag) {
    return [config = std::move(config), hooks = std::move(hooks),
            sculkReplaceableLevelTag = std::move(sculkReplaceableLevelTag)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        using namespace sculk_detail;
        // canSpreadFrom (:68-77).
        const std::string start = level.getBlockState(origin);
        const bool startIsBehaviour = start == "minecraft:sculk" || start == "minecraft:sculk_vein";
        if (!startIsBehaviour) {
            const bool airOrSourceWater = hooks->isAir(start)
                || (start == "minecraft:water" && hooks->isWaterFluid(start));
            if (!airOrSourceWater) return false;
            bool anyFull = false;
            for (int dir = 0; dir < 6; ++dir) {   // Direction.stream() order
                if (hooks->isCollisionShapeFullBlock(
                        level.getBlockState(treeRelative(origin, dir)))) { anyFull = true; break; }
            }
            if (!anyFull) return false;
        }
        Spreader spreader;
        const int totalRounds = config->spreadRounds + config->growthRounds;
        for (int round = 0; round < totalRounds; ++round) {
            for (int i = 0; i < config->chargeCount; ++i) {
                spreader.addCursors(origin, config->amountPerCharge);
            }
            const bool spreadVeins = round < config->spreadRounds;
            for (int i = 0; i < config->spreadAttempts; ++i) {
                updateCursors(spreader, level, *hooks, sculkReplaceableLevelTag, origin, random, spreadVeins);
            }
            spreader.clear();
        }
        const BlockPos below{ origin.x, origin.y - 1, origin.z };
        if (random.nextFloat() <= config->catalystChance
            && hooks->isCollisionShapeFullBlock(level.getBlockState(below))) {
            level.setBlock(origin, "minecraft:sculk_catalyst", 3);
        }
        const int extraGrowths = config->extraRareGrowths->sample(random);
        for (int i = 0; i < extraGrowths; ++i) {
            const BlockPos candidate{ origin.x + random.nextInt(5) - 2, origin.y, origin.z + random.nextInt(5) - 2 };
            if (hooks->isAir(level.getBlockState(candidate))
                && hooks->isFaceSturdy(level.getBlockState(BlockPos{ candidate.x, candidate.y - 1, candidate.z }), 1)) {
                level.setBlock(candidate, "minecraft:sculk_shrieker", 3);
            }
        }
        return true;
    };
}

} // namespace mc::levelgen::feature

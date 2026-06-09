#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.TreeFeature with
//   trunk placers:   StraightTrunkPlacer, FancyTrunkPlacer
//   foliage placers: BlobFoliagePlacer, FancyFoliagePlacer
//   decorators:      BeehiveDecorator, PlaceOnGroundDecorator
//   minimum size:    TwoLayersFeatureSize
// plus the post-placement leaf DISTANCE update (TreeFeature.updateLeaves) and
// StructureTemplate.updateShapeAtEdge over the resulting DiscreteVoxelShape.
//
// RNG order per attempt (TreeFeature.java:57-94 doPlace):
//   treeHeight  = base + nextInt(randA+1) + nextInt(randB+1)   (TrunkPlacer.java:58-60;
//                 randB=0 still consumes a nextInt(1) draw)
//   foliageHeight = placer.foliageHeight(...)                  (blob/fancy: constant, no draw,
//                 BlobFoliagePlacer.java:50-53)
//   leafRadius  = radius.sample(random)                        (FoliagePlacer.java:65-67;
//                 ConstantInt: no draw)
//   bounds + getMaxFreeTreeHeight: world reads only, no draws
//   placeTrunk:
//     straight (StraightTrunkPlacer.java:27-43): placeBelowTrunkBlock (rule-based
//       provider, no draws) then one placeLog per y (simple provider, no draws)
//     fancy (FancyTrunkPlacer.java:35-91): per relativeY from height-5 down to 0 with
//       treeShape >= 0, clustersPerY(=1) iterations of nextFloat (radius), nextFloat
//       (angle); makeLimb scans are draw-free
//   createFoliage per attachment: offset.sample (ConstantInt: no draw), then rows;
//     blob corner skip draws nextInt(2) at every |dx|==r && |dz|==r cell
//     (BlobFoliagePlacer.java:56-58 — the draw happens before the || y==0
//     short-circuit); fancy skip is pure float math (FancyFoliagePlacer.java:41-44)
//   decorators (TreeFeature.java:155-159, config order):
//     beehive (BeehiveDecorator.java:37-67): one nextFloat ALWAYS (logs non-empty);
//       on pass: optional nextInt(3) (no-leaves arm), Util.shuffle (n-1 nextInt
//       draws), then on a successful placement 2+nextInt(2) bees, nextInt(599) each
//     place_on_ground (PlaceOnGroundDecorator.java:43-85): per try 3x
//       nextIntBetweenInclusive; the provider getState draw happens ONLY when the
//       three gates pass
//   updateLeaves + updateShapeAtEdge: NO draws (level.getRandom() is passed to
//     updateShape but no reachable updateShape consumes it)
//
// HashSet-order sensitivity: trunk/foliage/decoration positions live in Java
// HashSets; TreeDecorator.Context copies them into ObjectArrayLists and sorts by Y
// with fastutil's STABLE mergesort (TreeDecorator.java:45-50, ObjectArrays.sort),
// so the within-Y order is java.util.HashSet iteration order — replicated by
// JavaBlockPosHashSet below.

#include "../placement/PlacementContext.h"   // WorldGenLevel
#include "../placement/PlacedFeature.h"      // FeaturePlacer
#include "../RandomSource.h"
#include "../IntProvider.h"
#include "../Heightmap.h"
#include "DiskFeature.h"                     // DiskStateProvider typedef
#include "../../../../core/Math.h"           // BlockPos

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

// ---------------------------------------------------------------------------
// Direction.values() order (Direction.java): DOWN, UP, NORTH, SOUTH, WEST, EAST.
inline constexpr int TREE_DIR_DX[6] = { 0, 0, 0, 0, -1, 1 };
inline constexpr int TREE_DIR_DY[6] = { -1, 1, 0, 0, 0, 0 };
inline constexpr int TREE_DIR_DZ[6] = { 0, 0, -1, 1, 0, 0 };
inline BlockPos treeRelative(BlockPos p, int dir) {
    return BlockPos{ p.x + TREE_DIR_DX[dir], p.y + TREE_DIR_DY[dir], p.z + TREE_DIR_DZ[dir] };
}

// Mth.floor (Mth.java): (int) cast, minus one when the value is below the cast.
inline int mthFloorD(double v) { int i = static_cast<int>(v); return v < static_cast<double>(i) ? i - 1 : i; }
inline int mthFloorF(float v) { int i = static_cast<int>(v); return v < static_cast<float>(i) ? i - 1 : i; }

// RandomSource.nextIntBetweenInclusive (RandomSource.java:45-47).
inline int nextIntBetweenInclusive(RandomSource& random, int minInclusive, int maxInclusive) {
    return random.nextInt(maxInclusive - minInclusive + 1) + minInclusive;
}

// Util.shuffle (Util.java:1061-1068): Fisher-Yates from the top with
// list.set-swap semantics.
template <typename T>
inline void javaShuffle(std::vector<T>& list, RandomSource& random) {
    for (int i = static_cast<int>(list.size()); i > 1; --i) {
        const int swapTo = random.nextInt(i);
        std::swap(list[static_cast<std::size_t>(i - 1)], list[static_cast<std::size_t>(swapTo)]);
    }
}

// ---------------------------------------------------------------------------
// java.util.HashSet<BlockPos> with Java iteration order.
//   hash:    Vec3i.hashCode = (y + z*31)*31 + x  (Vec3i.java)
//   spread:  java.util.HashMap.hash: h ^ (h >>> 16)
//   bucket:  spread & (capacity-1); buckets iterate ascending, entries within a
//            bucket in insertion order. Resizes (cap 16 -> x2 whenever
//            ++size > 0.75*cap, HashMap.putVal/resize) split each bucket
//            preserving relative order, and bucket bits are capacity-suffix
//            bits — so the final order equals: sort by (spread & (finalCap-1))
//            ascending, ties by insertion order.
class JavaBlockPosHashSet {
public:
    bool add(BlockPos p) {
        for (const BlockPos& q : m_items) {
            if (q == p) return false;   // HashSet.add on a present key keeps the original slot
        }
        m_items.push_back(p);
        return true;
    }
    bool empty() const { return m_items.empty(); }
    std::size_t size() const { return m_items.size(); }
    const std::vector<BlockPos>& insertionOrder() const { return m_items; }

    static std::int32_t vec3iHash(BlockPos p) {
        return (p.y + p.z * 31) * 31 + p.x;
    }
    // Iteration order of the equivalent java.util.HashSet.
    std::vector<BlockPos> javaOrder() const {
        int cap = 16, threshold = 12;
        for (std::size_t i = 1; i <= m_items.size(); ++i) {
            if (static_cast<int>(i) > threshold) { cap <<= 1; threshold = cap / 4 * 3; }
        }
        std::vector<std::pair<std::uint32_t, std::size_t>> keyed;   // (bucket, insertion index)
        keyed.reserve(m_items.size());
        for (std::size_t i = 0; i < m_items.size(); ++i) {
            const std::uint32_t h = static_cast<std::uint32_t>(vec3iHash(m_items[i]));
            const std::uint32_t spread = h ^ (h >> 16);
            keyed.emplace_back(spread & static_cast<std::uint32_t>(cap - 1), i);
        }
        std::stable_sort(keyed.begin(), keyed.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<BlockPos> out;
        out.reserve(m_items.size());
        for (const auto& [bucket, idx] : keyed) out.push_back(m_items[idx]);
        return out;
    }

private:
    std::vector<BlockPos> m_items;   // insertion order, deduped
};

// ---------------------------------------------------------------------------
// BoundingBox (structure/BoundingBox.java) — the pieces TreeFeature uses.
struct TreeBoundingBox {
    int minX, minY, minZ, maxX, maxY, maxZ;
    explicit TreeBoundingBox(BlockPos p) : minX(p.x), minY(p.y), minZ(p.z), maxX(p.x), maxY(p.y), maxZ(p.z) {}
    void encapsulate(BlockPos p) {
        minX = std::min(minX, p.x); minY = std::min(minY, p.y); minZ = std::min(minZ, p.z);
        maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y); maxZ = std::max(maxZ, p.z);
    }
    bool isInside(BlockPos p) const {
        return p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY && p.z >= minZ && p.z <= maxZ;
    }
    int xSpan() const { return maxX - minX + 1; }
    int ySpan() const { return maxY - minY + 1; }
    int zSpan() const { return maxZ - minZ + 1; }
};

// BitSetDiscreteVoxelShape + DiscreteVoxelShape.forAllFaces (forAllAxisFaces in
// AxisCycle order NONE (Z faces), FORWARD (Y faces), BACKWARD (X faces) —
// DiscreteVoxelShape.java:222-252, AxisCycle.java).
class TreeVoxelShape {
public:
    TreeVoxelShape(int sx, int sy, int sz)
        : m_sx(sx), m_sy(sy), m_sz(sz), m_bits(static_cast<std::size_t>(sx) * sy * sz, false) {}
    void fill(int x, int y, int z) { m_bits[index(x, y, z)] = true; }
    bool isFull(int x, int y, int z) const { return m_bits[index(x, y, z)]; }

    // consumer(directionIndex, x, y, z)
    void forAllFaces(const std::function<void(int, int, int, int)>& consumer) const {
        forAllAxisFaces(consumer, 0);   // NONE     -> c axis Z: NORTH(2)/SOUTH(3)
        forAllAxisFaces(consumer, 1);   // FORWARD  -> c axis Y: DOWN(0)/UP(1)
        forAllAxisFaces(consumer, 2);   // BACKWARD -> c axis X: WEST(4)/EAST(5)
    }

private:
    // AxisCycle.cycle(x,y,z,axis): NONE choose(x,y,z); FORWARD choose(z,x,y);
    // BACKWARD choose(y,z,x) (AxisCycle.java). axis: 0=X,1=Y,2=Z.
    static int cycleCoord(int transform, int x, int y, int z, int axis) {
        switch (transform) {
            case 0: return axis == 0 ? x : axis == 1 ? y : z;          // NONE
            case 1: return axis == 0 ? z : axis == 1 ? x : y;          // FORWARD
            default: return axis == 0 ? y : axis == 1 ? z : x;         // BACKWARD
        }
    }
    static int cycleAxis(int transform, int axis) {
        switch (transform) {
            case 0: return axis;
            case 1: return (axis + 1) % 3;    // FORWARD
            default: return (axis + 2) % 3;   // BACKWARD
        }
    }
    static int inverseOf(int transform) { return transform == 0 ? 0 : transform == 1 ? 2 : 1; }
    int sizeOfAxis(int axis) const { return axis == 0 ? m_sx : axis == 1 ? m_sy : m_sz; }
    // Direction.fromAxisAndDirection: X-:WEST(4) X+:EAST(5), Y-:DOWN(0) Y+:UP(1),
    // Z-:NORTH(2) Z+:SOUTH(3).
    static int negDir(int axis) { return axis == 0 ? 4 : axis == 1 ? 0 : 2; }
    static int posDir(int axis) { return axis == 0 ? 5 : axis == 1 ? 1 : 3; }

    void forAllAxisFaces(const std::function<void(int, int, int, int)>& consumer, int transform) const {
        const int inverse = inverseOf(transform);
        const int cAxis = cycleAxis(inverse, 2);                       // inverse.cycle(Z)
        const int aSize = sizeOfAxis(cycleAxis(inverse, 0));           // size(inverse.cycle(X))
        const int bSize = sizeOfAxis(cycleAxis(inverse, 1));           // size(inverse.cycle(Y))
        const int cSize = sizeOfAxis(cAxis);
        const int negative = negDir(cAxis), positive = posDir(cAxis);
        for (int a = 0; a < aSize; ++a) {
            for (int b = 0; b < bSize; ++b) {
                bool lastFull = false;
                for (int c = 0; c <= cSize; ++c) {
                    const bool full = c != cSize
                        && isFull(cycleCoord(inverse, a, b, c, 0), cycleCoord(inverse, a, b, c, 1), cycleCoord(inverse, a, b, c, 2));
                    if (!lastFull && full) {
                        consumer(negative, cycleCoord(inverse, a, b, c, 0), cycleCoord(inverse, a, b, c, 1), cycleCoord(inverse, a, b, c, 2));
                    }
                    if (lastFull && !full) {
                        consumer(positive, cycleCoord(inverse, a, b, c - 1, 0), cycleCoord(inverse, a, b, c - 1, 1), cycleCoord(inverse, a, b, c - 1, 2));
                    }
                    lastFull = full;
                }
            }
        }
    }

    std::size_t index(int x, int y, int z) const {
        return (static_cast<std::size_t>(x) * m_sy + y) * m_sz + z;
    }
    int m_sx, m_sy, m_sz;
    std::vector<bool> m_bits;
};

// ---------------------------------------------------------------------------
// Level/tag surface the tree machinery needs beyond WorldGenLevel; supplied by
// the harness (which owns tags, the leaf DISTANCE side map and updateShape).
struct TreeHooks {
    std::function<bool(const std::string&)> isAir;             // BlockStateBase.isAir
    std::function<bool(const std::string&)> validTreePosState; // isAir || #replaceable_by_trees (TreeFeature.java:53-55)
    std::function<bool(const std::string&)> isLog;             // #minecraft:logs (TrunkPlacer.isFree, TrunkPlacer.java:117-119)
    std::function<bool(const std::string&)> isVine;            // Blocks.VINE (TreeFeature.java:41-43)
    std::function<bool(const std::string&)> isSolidRender;     // BlockStateBase.isSolidRender
    // LeavesBlock.getOptionalDistanceAt (LeavesBlock.java:131-137).
    std::function<std::optional<int>(BlockPos)> optionalDistanceAt;
    // TreeFeature.updateLeaves' setBlockKnownShape(pos, state.setValue(DISTANCE, d))
    // — block id unchanged; the harness updates the side map iff the radius-gated
    // level.setBlock(pos, ..., 19) would land (TreeFeature.java:202-204).
    std::function<void(BlockPos, int)> setLeafDistance;
    // One StructureTemplate.updateShapeAtEdge face visit (StructureTemplate.java:
    // 416-436): update `pos` from (direction, neighbor), write if changed with
    // updateMode & -2; then update `neighbor` from the opposite direction.
    std::function<void(BlockPos, int, BlockPos)> updateShapeFace;
    // TrunkVineDecorator.placeVine sets VINE with exactly one face property
    // (TreeDecorator.java:53-55); the id-only grid keeps the face in a side map.
    std::function<void(BlockPos, int)> putVineFace;
    int levelMinY = 0;   // level.getMinY()
    int levelMaxY = 0;   // level.getMaxY() (inclusive highest buildable y)
};

// ---------------------------------------------------------------------------
// Configuration (TreeConfiguration.java + the placer/decorator codecs).
struct TreeDecoratorConfig {
    enum class Kind { Beehive, PlaceOnGround };
    Kind kind = Kind::Beehive;
    float probability = 0.0f;                 // beehive
    int tries = 128, radius = 2, height = 1;  // place_on_ground codec defaults (PlaceOnGroundDecorator.java:19-21)
    DiskStateProvider provider;               // place_on_ground block_state_provider
};

struct TreeConfig {
    DiskStateProvider trunkProvider;       // getState (never null)
    DiskStateProvider foliageProvider;     // getState (never null)
    DiskStateProvider belowTrunkProvider;  // getOptionalState (nullopt == none)
    enum class Trunk { Straight, Fancy };
    Trunk trunkKind = Trunk::Straight;
    int baseHeight = 0, heightRandA = 0, heightRandB = 0;
    enum class Foliage { Blob, Fancy };
    Foliage foliageKind = Foliage::Blob;
    mc::valueproviders::IntProviderPtr foliageRadius, foliageOffset;
    int foliageHeightParam = 0;            // blob/fancy "height"
    int sizeLimit = 1, lowerSize = 0, upperSize = 1;   // two_layers_feature_size
    std::optional<int> minClippedHeight;
    bool ignoreVines = false;
    std::vector<TreeDecoratorConfig> decorators;
};

// FoliagePlacer.FoliageAttachment (FoliagePlacer.java:189-211).
struct FoliageAttachment {
    BlockPos pos;
    int radiusOffset = 0;
    bool doubleTrunk = false;
};

// ---------------------------------------------------------------------------
// TreeDecorator.Context (TreeDecorator.java:26-88): logs/leaves/roots copied
// from the placement sets and sorted by Y (stable — fastutil mergesort — over
// the Java HashSet iteration order).
struct TreeDecoratorContext {
    WorldGenLevel* level = nullptr;
    // decorationSetter: decorations.add(pos) FIRST, then level.setBlock(pos, state, 19)
    // (TreeFeature.java:150-153). Returns whether the write landed (drives the
    // block-entity-dependent RNG: bee storage, loot tables).
    std::function<bool(BlockPos, const std::string&)> setBlock;
    RandomSource* random = nullptr;
    std::vector<BlockPos> logs, leaves, roots;
    const TreeHooks* hooks = nullptr;

    bool isAir(BlockPos p) const { return hooks->isAir(level->getBlockState(p)); }
};

inline std::vector<BlockPos> sortedByYJavaOrder(const JavaBlockPosHashSet& set) {
    std::vector<BlockPos> v = set.javaOrder();
    std::stable_sort(v.begin(), v.end(), [](const BlockPos& a, const BlockPos& b) { return a.y < b.y; });
    return v;
}

// BeehiveDecorator.place (BeehiveDecorator.java:36-67).
inline void placeBeehiveDecorator(TreeDecoratorContext& ctx, float probability) {
    const std::vector<BlockPos>& leaves = ctx.leaves;
    const std::vector<BlockPos>& logs = ctx.logs;
    if (logs.empty()) return;
    RandomSource& random = *ctx.random;
    if (random.nextFloat() >= probability) return;
    const int hiveY = !leaves.empty()
        ? std::max(leaves.front().y - 1, logs.front().y + 1)
        : std::min(logs.front().y + 1 + random.nextInt(3), logs.back().y);
    // SPAWN_DIRECTIONS = HORIZONTAL (NORTH,EAST,SOUTH,WEST) minus opposite(SOUTH)=NORTH
    // -> EAST, SOUTH, WEST (BeehiveDecorator.java:20-24).
    static constexpr int spawnDirs[3] = { 5, 3, 4 };
    std::vector<BlockPos> hivePlacements;
    for (const BlockPos& pos : logs) {
        if (pos.y == hiveY) {
            for (int d : spawnDirs) hivePlacements.push_back(treeRelative(pos, d));
        }
    }
    if (hivePlacements.empty()) return;
    javaShuffle(hivePlacements, random);
    // findFirst: isAir(pos) && isAir(pos.relative(SOUTH)).
    const BlockPos* hivePos = nullptr;
    for (const BlockPos& p : hivePlacements) {
        if (ctx.isAir(p) && ctx.isAir(treeRelative(p, 3))) { hivePos = &p; break; }
    }
    if (hivePos == nullptr) return;
    const bool placed = ctx.setBlock(*hivePos, "minecraft:bee_nest");
    // getBlockEntity(...).ifPresent: the beehive entity exists iff the write landed
    // (WorldGenRegion.setBlock records the DUMMY nbt only on success).
    if (placed) {
        const int numBees = 2 + random.nextInt(2);
        for (int i = 0; i < numBees; ++i) {
            (void)random.nextInt(599);   // BeehiveBlockEntity.Occupant.create(random.nextInt(599))
        }
    }
}

// TreeFeature.getLowestTrunkOrRootOfTree (TreeFeature.java:235-249).
inline std::vector<BlockPos> lowestTrunkOrRoot(const TreeDecoratorContext& ctx) {
    std::vector<BlockPos> out;
    if (ctx.roots.empty()) {
        out = ctx.logs;
    } else if (!ctx.logs.empty() && ctx.roots.front().y == ctx.logs.front().y) {
        out = ctx.logs;
        out.insert(out.end(), ctx.roots.begin(), ctx.roots.end());
    } else {
        out = ctx.roots;
    }
    return out;
}

// PlaceOnGroundDecorator.place + attemptToPlaceBlockAbove (PlaceOnGroundDecorator.java:43-85).
inline void placeOnGroundDecorator(TreeDecoratorContext& ctx, const TreeDecoratorConfig& cfg) {
    const std::vector<BlockPos> blockPositions = lowestTrunkOrRoot(ctx);
    if (blockPositions.empty()) return;
    const BlockPos origin = blockPositions.front();
    const int minY = origin.y;
    int minX = origin.x, maxX = origin.x, minZ = origin.z, maxZ = origin.z;
    for (const BlockPos& position : blockPositions) {
        if (position.y == minY) {
            minX = std::min(minX, position.x);
            maxX = std::max(maxX, position.x);
            minZ = std::min(minZ, position.z);
            maxZ = std::max(maxZ, position.z);
        }
    }
    RandomSource& random = *ctx.random;
    // new BoundingBox(minX, minY, minZ, maxX, minY, maxZ).inflatedBy(radius, height, radius)
    const int bbMinX = minX - cfg.radius, bbMinY = minY - cfg.height, bbMinZ = minZ - cfg.radius;
    const int bbMaxX = maxX + cfg.radius, bbMaxY = minY + cfg.height, bbMaxZ = maxZ + cfg.radius;
    for (int i = 0; i < cfg.tries; ++i) {
        const BlockPos pos{ nextIntBetweenInclusive(random, bbMinX, bbMaxX),
                            nextIntBetweenInclusive(random, bbMinY, bbMaxY),
                            nextIntBetweenInclusive(random, bbMinZ, bbMaxZ) };
        // attemptToPlaceBlockAbove
        const BlockPos above{ pos.x, pos.y + 1, pos.z };
        const std::string aboveState = ctx.level->getBlockState(above);
        if ((ctx.hooks->isAir(aboveState) || ctx.hooks->isVine(aboveState))
            && ctx.hooks->isSolidRender(ctx.level->getBlockState(pos))
            // getHeightmapPos(MOTION_BLOCKING_NO_LEAVES, pos).getY() <= abovePos.getY();
            // WorldGenRegion.getHeight == stored heightmap + 1 (WorldGenRegion.java:391-393).
            && ctx.level->getHeight(Heightmap::Types::MOTION_BLOCKING_NO_LEAVES, pos.x, pos.z) <= above.y) {
            const std::optional<std::string> state = cfg.provider(*ctx.level, random, above);
            ctx.setBlock(above, state.value());
        }
    }
}

// ---------------------------------------------------------------------------
// Trunk placer helpers (TrunkPlacer.java).
struct TrunkPlacerOps {
    WorldGenLevel* level;
    const TreeConfig* config;
    const TreeHooks* hooks;
    std::function<void(BlockPos, const std::string&)> trunkSetter;

    bool validTreePos(BlockPos pos) const {
        return hooks->validTreePosState(level->getBlockState(pos));
    }
    // TrunkPlacer.isFree (TrunkPlacer.java:117-119).
    bool isFree(BlockPos pos) const {
        const std::string s = level->getBlockState(pos);
        return hooks->validTreePosState(s) || hooks->isLog(s);
    }
    // TrunkPlacer.placeBelowTrunkBlock (TrunkPlacer.java:62-73).
    void placeBelowTrunkBlock(RandomSource& random, BlockPos pos) const {
        const std::optional<std::string> below = config->belowTrunkProvider(*level, random, pos);
        if (below.has_value()) trunkSetter(pos, *below);
    }
    // TrunkPlacer.placeLog (TrunkPlacer.java:85-99); the axis state modifier only
    // touches the AXIS property — invisible at block-id granularity.
    bool placeLog(RandomSource& random, BlockPos pos) const {
        if (validTreePos(pos)) {
            trunkSetter(pos, config->trunkProvider(*level, random, pos).value());
            return true;
        }
        return false;
    }
};

// StraightTrunkPlacer.placeTrunk (StraightTrunkPlacer.java:27-43).
inline std::vector<FoliageAttachment> placeStraightTrunk(
        const TrunkPlacerOps& ops, RandomSource& random, int treeHeight, BlockPos origin) {
    ops.placeBelowTrunkBlock(random, BlockPos{ origin.x, origin.y - 1, origin.z });
    for (int y = 0; y < treeHeight; ++y) {
        ops.placeLog(random, BlockPos{ origin.x, origin.y + y, origin.z });
    }
    return { FoliageAttachment{ BlockPos{ origin.x, origin.y + treeHeight, origin.z }, 0, false } };
}

// FancyTrunkPlacer (FancyTrunkPlacer.java) — the gnarled big oak.
namespace fancy_detail {

struct FoliageCoords {
    BlockPos attachmentPos;   // FoliageAttachment(pos, 0, false)
    int branchBase;
};

inline int getSteps(BlockPos delta) {
    return std::max(std::abs(delta.x), std::max(std::abs(delta.y), std::abs(delta.z)));
}

// FancyTrunkPlacer.makeLimb (FancyTrunkPlacer.java:93-124).
inline bool makeLimb(const TrunkPlacerOps& ops, RandomSource& random,
                     BlockPos startPos, BlockPos endPos, bool doPlace) {
    if (!doPlace && startPos == endPos) {
        return true;
    }
    const BlockPos delta{ endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };
    const int steps = getSteps(delta);
    // Java divides by `steps` in float; steps==0 only when start==end (early return
    // above for the scan arm; the doPlace arms never pass equal endpoints for these
    // configs). Guard identically to Mth.floor(NaN) == 0: emit startPos once.
    const float dx = steps == 0 ? 0.0f : static_cast<float>(delta.x) / static_cast<float>(steps);
    const float dy = steps == 0 ? 0.0f : static_cast<float>(delta.y) / static_cast<float>(steps);
    const float dz = steps == 0 ? 0.0f : static_cast<float>(delta.z) / static_cast<float>(steps);
    for (int i = 0; i <= steps; ++i) {
        const BlockPos blockPos{ startPos.x + mthFloorF(0.5f + static_cast<float>(i) * dx),
                                 startPos.y + mthFloorF(0.5f + static_cast<float>(i) * dy),
                                 startPos.z + mthFloorF(0.5f + static_cast<float>(i) * dz) };
        if (doPlace) {
            ops.placeLog(random, blockPos);   // axis modifier: id-invisible
        } else if (!ops.isFree(blockPos)) {
            return false;
        }
    }
    return true;
}

// FancyTrunkPlacer.treeShape (FancyTrunkPlacer.java:171-186), float-exact.
inline float treeShape(int height, int y) {
    if (static_cast<float>(y) < static_cast<float>(height) * 0.3f) {
        return -1.0f;
    }
    const float radius = static_cast<float>(height) / 2.0f;
    const float adjacent = radius - static_cast<float>(y);
    float distance = static_cast<float>(std::sqrt(static_cast<double>(radius * radius - adjacent * adjacent)));
    if (adjacent == 0.0f) {
        distance = radius;
    } else if (std::abs(adjacent) >= radius) {
        return 0.0f;
    }
    return distance * 0.5f;
}

inline bool trimBranches(int height, int localY) {
    return static_cast<double>(localY) >= static_cast<double>(height) * 0.2;
}

} // namespace fancy_detail

// FancyTrunkPlacer.placeTrunk (FancyTrunkPlacer.java:35-91).
inline std::vector<FoliageAttachment> placeFancyTrunk(
        const TrunkPlacerOps& ops, RandomSource& random, int treeHeight, BlockPos origin) {
    using namespace fancy_detail;
    const int height = treeHeight + 2;
    const int trunkHeight = mthFloorD(static_cast<double>(height) * 0.618);
    ops.placeBelowTrunkBlock(random, BlockPos{ origin.x, origin.y - 1, origin.z });
    const int clustersPerY = std::min(1, mthFloorD(1.382 + std::pow(1.0 * height / 13.0, 2.0)));
    const int trunkTop = origin.y + trunkHeight;
    int relativeY = height - 5;
    std::vector<FoliageCoords> foliageCoords;
    foliageCoords.push_back(FoliageCoords{ BlockPos{ origin.x, origin.y + relativeY, origin.z }, trunkTop });
    for (; relativeY >= 0; --relativeY) {
        const float ts = treeShape(height, relativeY);
        if (!(ts < 0.0f)) {
            for (int i = 0; i < clustersPerY; ++i) {
                const double radius = 1.0 * static_cast<double>(ts) * (static_cast<double>(random.nextFloat()) + 0.328);
                const double angle = static_cast<double>(random.nextFloat() * 2.0f) * 3.141592653589793;  // (float)*2.0F widened * Math.PI
                const double x = radius * std::sin(angle) + 0.5;
                const double z = radius * std::cos(angle) + 0.5;
                const BlockPos checkStart{ origin.x + mthFloorD(x), origin.y + (relativeY - 1), origin.z + mthFloorD(z) };
                const BlockPos checkEnd{ checkStart.x, checkStart.y + 5, checkStart.z };
                if (makeLimb(ops, random, checkStart, checkEnd, false)) {
                    const int dx = origin.x - checkStart.x;
                    const int dz = origin.z - checkStart.z;
                    const double branchHeight = static_cast<double>(checkStart.y) - std::sqrt(static_cast<double>(dx * dx + dz * dz)) * 0.381;
                    const int branchTop = branchHeight > static_cast<double>(trunkTop) ? trunkTop : static_cast<int>(branchHeight);
                    const BlockPos checkBranchBase{ origin.x, branchTop, origin.z };
                    if (makeLimb(ops, random, checkBranchBase, checkStart, false)) {
                        foliageCoords.push_back(FoliageCoords{ checkStart, checkBranchBase.y });
                    }
                }
            }
        }
    }
    makeLimb(ops, random, origin, BlockPos{ origin.x, origin.y + trunkHeight, origin.z }, true);
    // makeBranches (FancyTrunkPlacer.java:153-169)
    for (const FoliageCoords& endCoord : foliageCoords) {
        const BlockPos baseCoord{ origin.x, endCoord.branchBase, origin.z };
        if (!(baseCoord == endCoord.attachmentPos) && trimBranches(height, endCoord.branchBase - origin.y)) {
            makeLimb(ops, random, baseCoord, endCoord.attachmentPos, true);
        }
    }
    std::vector<FoliageAttachment> attachments;
    for (const FoliageCoords& fc : foliageCoords) {
        if (trimBranches(height, fc.branchBase - origin.y)) {
            attachments.push_back(FoliageAttachment{ fc.attachmentPos, 0, false });
        }
    }
    return attachments;
}

// ---------------------------------------------------------------------------
// Foliage placement (FoliagePlacer.java + Blob/FancyFoliagePlacer).
struct FoliageSetterState {
    JavaBlockPosHashSet* foliage;
    std::function<void(BlockPos, const std::string&)> set;   // foliage.add + level.setBlock(...,19)
};

// FoliagePlacer.tryPlaceLeaf (FoliagePlacer.java:170-187). The PERSISTENT read
// (getValueOrElse(PERSISTENT, false)) is constant false here: no worldgen path
// places persistent leaves and non-leaf states lack the property.
inline bool tryPlaceLeaf(const TreeConfig& config, const TreeHooks& hooks, WorldGenLevel& level,
                         const FoliageSetterState& setter, RandomSource& random, BlockPos pos) {
    const bool isPersistent = false;
    if (!isPersistent && hooks.validTreePosState(level.getBlockState(pos))) {
        const std::string foliageState = config.foliageProvider(level, random, pos).value();
        // WATERLOGGED setValue from isFluidAtPosition: property-only, id unchanged.
        setter.set(pos, foliageState);
        return true;
    }
    return false;
}

// FoliagePlacer.shouldSkipLocationSigned + placeLeavesRow (FoliagePlacer.java:77-114).
inline void placeLeavesRow(const TreeConfig& config, const TreeHooks& hooks, WorldGenLevel& level,
                           const FoliageSetterState& setter, RandomSource& random,
                           BlockPos origin, int currentRadius, int y, bool doubleTrunk) {
    const int offset = doubleTrunk ? 1 : 0;
    for (int dx = -currentRadius; dx <= currentRadius + offset; ++dx) {
        for (int dz = -currentRadius; dz <= currentRadius + offset; ++dz) {
            // shouldSkipLocationSigned -> shouldSkipLocation(minDx, y, minDz, ...)
            int minDx, minDz;
            if (doubleTrunk) {
                minDx = std::min(std::abs(dx), std::abs(dx - 1));
                minDz = std::min(std::abs(dz), std::abs(dz - 1));
            } else {
                minDx = std::abs(dx);
                minDz = std::abs(dz);
            }
            bool skip;
            if (config.foliageKind == TreeConfig::Foliage::Blob) {
                // BlobFoliagePlacer.shouldSkipLocation (BlobFoliagePlacer.java:56-58):
                // the nextInt(2) draw fires at every corner BEFORE the || y == 0.
                skip = minDx == currentRadius && minDz == currentRadius
                       && (random.nextInt(2) == 0 || y == 0);
            } else {
                // FancyFoliagePlacer.shouldSkipLocation (FancyFoliagePlacer.java:41-44).
                const float fx = static_cast<float>(minDx) + 0.5f;
                const float fz = static_cast<float>(minDz) + 0.5f;
                skip = fx * fx + fz * fz > static_cast<float>(currentRadius * currentRadius);
            }
            if (!skip) {
                tryPlaceLeaf(config, hooks, level, setter, random,
                             BlockPos{ origin.x + dx, origin.y + y, origin.z + dz });
            }
        }
    }
}

// Blob/FancyFoliagePlacer.createFoliage (BlobFoliagePlacer.java:32-48,
// FancyFoliagePlacer.java:23-39); the shared offset draw happens in
// FoliagePlacer.createFoliage (FoliagePlacer.java:38-49).
inline void createFoliage(const TreeConfig& config, const TreeHooks& hooks, WorldGenLevel& level,
                          const FoliageSetterState& setter, RandomSource& random,
                          const FoliageAttachment& attachment, int foliageHeight, int leafRadius) {
    const int offset = config.foliageOffset->sample(random);
    if (config.foliageKind == TreeConfig::Foliage::Blob) {
        for (int yo = offset; yo >= offset - foliageHeight; --yo) {
            const int currentRadius = std::max(leafRadius + attachment.radiusOffset - 1 - yo / 2, 0);
            placeLeavesRow(config, hooks, level, setter, random, attachment.pos, currentRadius, yo, attachment.doubleTrunk);
        }
    } else {
        for (int yo = offset; yo >= offset - foliageHeight; --yo) {
            const int currentRadius = leafRadius + (yo != offset && yo != offset - foliageHeight ? 1 : 0);
            placeLeavesRow(config, hooks, level, setter, random, attachment.pos, currentRadius, yo, attachment.doubleTrunk);
        }
    }
}

// ---------------------------------------------------------------------------
// TreeFeature.updateLeaves (TreeFeature.java:171-233). Bucketed label-correcting
// pass; the processed-cell closure (the shape) and the queue contents are
// HashSet-order independent — only the transiently written DISTANCE values can
// differ in pop order, and those are block-id invisible. std::set pops give a
// deterministic order.
struct BlockPosLess {
    bool operator()(const BlockPos& a, const BlockPos& b) const {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
};

inline TreeVoxelShape updateLeaves(const TreeHooks& hooks, const TreeBoundingBox& bounds,
                                   const JavaBlockPosHashSet& logs,
                                   const JavaBlockPosHashSet& decorations,
                                   const JavaBlockPosHashSet& roots) {
    TreeVoxelShape shape(bounds.xSpan(), bounds.ySpan(), bounds.zSpan());
    constexpr int maxDistance = 7;
    std::vector<std::set<BlockPos, BlockPosLess>> toCheck(maxDistance);

    // Sets.union(decorationSet, rootPositions): fill only.
    for (const BlockPos& pos : decorations.insertionOrder()) {
        if (bounds.isInside(pos)) shape.fill(pos.x - bounds.minX, pos.y - bounds.minY, pos.z - bounds.minZ);
    }
    for (const BlockPos& pos : roots.insertionOrder()) {
        if (bounds.isInside(pos)) shape.fill(pos.x - bounds.minX, pos.y - bounds.minY, pos.z - bounds.minZ);
    }
    for (const BlockPos& pos : logs.insertionOrder()) {
        toCheck[0].insert(pos);
    }

    int smallestDistance = 0;
    while (true) {
        while (smallestDistance >= maxDistance || !toCheck[smallestDistance].empty()) {
            if (smallestDistance >= maxDistance) {
                return shape;
            }
            const BlockPos pos = *toCheck[smallestDistance].begin();
            toCheck[smallestDistance].erase(toCheck[smallestDistance].begin());
            if (bounds.isInside(pos)) {
                if (smallestDistance != 0) {
                    // setBlockKnownShape(level, pos, state.setValue(DISTANCE, smallestDistance))
                    hooks.setLeafDistance(pos, smallestDistance);
                }
                shape.fill(pos.x - bounds.minX, pos.y - bounds.minY, pos.z - bounds.minZ);
                for (int dir = 0; dir < 6; ++dir) {   // Direction.values() order
                    const BlockPos neighbor = treeRelative(pos, dir);
                    if (bounds.isInside(neighbor)) {
                        if (!shape.isFull(neighbor.x - bounds.minX, neighbor.y - bounds.minY, neighbor.z - bounds.minZ)) {
                            const std::optional<int> distance = hooks.optionalDistanceAt(neighbor);
                            if (distance.has_value()) {
                                const int newDistance = std::min(*distance, smallestDistance + 1);
                                if (newDistance < maxDistance) {
                                    toCheck[newDistance].insert(neighbor);
                                    smallestDistance = std::min(smallestDistance, newDistance);
                                }
                            }
                        }
                    }
                }
            }
        }
        ++smallestDistance;
    }
}

// ---------------------------------------------------------------------------
// The feature itself (TreeFeature.place, TreeFeature.java:120-169).
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeTreePlacer(
        std::shared_ptr<const TreeConfig> config, std::shared_ptr<const TreeHooks> hooks) {
    return [config = std::move(config), hooks = std::move(hooks)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        JavaBlockPosHashSet rootPositions, trunks, foliage, decorations;
        auto trunkSetter = [&](BlockPos pos, const std::string& state) {
            trunks.add(pos);
            level.setBlock(pos, state, 19);
        };
        FoliageSetterState foliageSetter{ &foliage, [&](BlockPos pos, const std::string& state) {
            foliage.add(pos);
            level.setBlock(pos, state, 19);
        } };

        // ---- doPlace (TreeFeature.java:57-94) ----
        bool result = false;
        {
            const int treeHeight = config->baseHeight + random.nextInt(config->heightRandA + 1)
                                   + random.nextInt(config->heightRandB + 1);   // TrunkPlacer.getTreeHeight
            const int foliageHeight = config->foliageHeightParam;               // blob/fancy foliageHeight: constant
            const int trunkHeight = treeHeight - foliageHeight;
            (void)trunkHeight;   // foliageRadius(random, trunkHeight) ignores it for blob/fancy
            const int leafRadius = config->foliageRadius->sample(random);
            const BlockPos trunkOrigin = origin;                                // no root placer
            const int minY = origin.y;
            const int maxY = origin.y + treeHeight + 1;
            if (minY >= hooks->levelMinY + 1 && maxY <= hooks->levelMaxY + 1) {
                // getMaxFreeTreeHeight (TreeFeature.java:96-113)
                TrunkPlacerOps ops{ &level, config.get(), hooks.get(), trunkSetter };
                int clippedTreeHeight = treeHeight;
                bool clipped = false;
                for (int y = 0; y <= treeHeight + 1 && !clipped; ++y) {
                    const int r = y < config->sizeLimit ? config->lowerSize : config->upperSize;
                    for (int x = -r; x <= r && !clipped; ++x) {
                        for (int z = -r; z <= r; ++z) {
                            const BlockPos p{ trunkOrigin.x + x, trunkOrigin.y + y, trunkOrigin.z + z };
                            if (!ops.isFree(p)
                                || (!config->ignoreVines && hooks->isVine(level.getBlockState(p)))) {
                                clippedTreeHeight = y - 2;
                                clipped = true;
                                break;
                            }
                        }
                    }
                }
                if (clippedTreeHeight >= treeHeight
                    || (config->minClippedHeight.has_value() && clippedTreeHeight >= *config->minClippedHeight)) {
                    std::vector<FoliageAttachment> attachments =
                        config->trunkKind == TreeConfig::Trunk::Straight
                            ? placeStraightTrunk(ops, random, clippedTreeHeight, trunkOrigin)
                            : placeFancyTrunk(ops, random, clippedTreeHeight, trunkOrigin);
                    for (const FoliageAttachment& attachment : attachments) {
                        createFoliage(*config, *hooks, level, foliageSetter, random,
                                      attachment, foliageHeight, leafRadius);
                    }
                    result = true;
                }
            }
        }

        if (result && (!trunks.empty() || !foliage.empty())) {
            if (!config->decorators.empty()) {
                TreeDecoratorContext ctx;
                ctx.level = &level;
                ctx.setBlock = [&](BlockPos pos, const std::string& state) {
                    decorations.add(pos);
                    return level.setBlockChecked(pos, state, 19);
                };
                ctx.random = &random;
                ctx.logs = sortedByYJavaOrder(trunks);
                ctx.leaves = sortedByYJavaOrder(foliage);
                ctx.roots = sortedByYJavaOrder(rootPositions);
                ctx.hooks = hooks.get();
                for (const TreeDecoratorConfig& dec : config->decorators) {
                    if (dec.kind == TreeDecoratorConfig::Kind::Beehive) {
                        placeBeehiveDecorator(ctx, dec.probability);
                    } else {
                        placeOnGroundDecorator(ctx, dec);
                    }
                }
            }
            // BoundingBox.encapsulatingPositions(concat(roots, trunks, foliage, decorations))
            std::optional<TreeBoundingBox> bounds;
            auto addAll = [&](const JavaBlockPosHashSet& s) {
                for (const BlockPos& p : s.insertionOrder()) {
                    if (!bounds.has_value()) bounds.emplace(p);
                    else bounds->encapsulate(p);
                }
            };
            addAll(rootPositions); addAll(trunks); addAll(foliage); addAll(decorations);
            if (!bounds.has_value()) return false;
            const TreeVoxelShape shape = updateLeaves(*hooks, *bounds, trunks, decorations, rootPositions);
            // StructureTemplate.updateShapeAtEdge(level, 3, shape, minX, minY, minZ)
            shape.forAllFaces([&](int dir, int x, int y, int z) {
                const BlockPos pos{ bounds->minX + x, bounds->minY + y, bounds->minZ + z };
                hooks->updateShapeFace(pos, dir, treeRelative(pos, dir));
            });
            return true;
        }
        return false;
    };
}

} // namespace mc::levelgen::feature

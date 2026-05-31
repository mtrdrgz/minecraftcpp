#pragma once

// Port of:
//   net.minecraft.world.level.levelgen.feature.TreeFeature
//   net.minecraft.world.level.levelgen.feature.configurations.TreeConfiguration
//   net.minecraft.world.level.levelgen.feature.trunkplacers.*
//   net.minecraft.world.level.levelgen.feature.foliageplacers.*
//   net.minecraft.world.level.levelgen.feature.featuresize.*
//   net.minecraft.data.worldgen.features.TreeFeatures  (built-in configs)
//
// The implementation deliberately avoids the full feature/placement pipeline
// and exposes a single function: decorateChunk(), which scatters trees
// into a freshly-surfaced chunk using a seeded per-chunk random, exactly
// replicating the per-tree shape algorithms from the Java source.

#include "../../chunk/LevelChunk.h"
#include "../RandomSource.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>
#include <functional>


namespace mc::levelgen::feature {

// ---------------------------------------------------------------------------
// IntProvider — port of net.minecraft.util.valueproviders.IntProvider
// Stored as a plain value type; no virtual dispatch needed.
// ---------------------------------------------------------------------------

struct IntVal {
    enum class Kind { Constant, Uniform } kind = Kind::Constant;
    int minValue = 0;
    int maxValue = 0;

    int sample(RandomSource& rng) const {
        if (kind == Kind::Constant) return minValue;
        return minValue + rng.nextInt(maxValue - minValue + 1);
    }

    static IntVal constant(int v)       { return {Kind::Constant, v, v}; }
    static IntVal uniform(int lo, int hi){ return {Kind::Uniform, lo, hi}; }
};

// ---------------------------------------------------------------------------
// FeatureSize — port of net.minecraft.world.level.levelgen.feature.featuresize
// Used by TreeFeature.getMaxFreeTreeHeight to check clearance radius at each Y.
// ---------------------------------------------------------------------------

struct FeatureSize {
    virtual ~FeatureSize() = default;
    virtual int  getSizeAtHeight(int treeHeight, int yo) const = 0;
    virtual std::optional<int> minClippedHeight() const { return std::nullopt; }
};

// Port of TwoLayersFeatureSize(limit, lowerSize, upperSize)
//   getSizeAtHeight(treeHeight, yo): yo < limit ? lowerSize : upperSize
struct TwoLayersFeatureSize final : FeatureSize {
    int limit;
    int lowerSize;
    int upperSize;
    std::optional<int> minClipped;

    TwoLayersFeatureSize(int limit, int lower, int upper,
                          std::optional<int> minClip = std::nullopt)
        : limit(limit), lowerSize(lower), upperSize(upper), minClipped(minClip) {}

    int getSizeAtHeight(int /*treeHeight*/, int yo) const override {
        return yo < limit ? lowerSize : upperSize;
    }
    std::optional<int> minClippedHeight() const override { return minClipped; }
};

// ---------------------------------------------------------------------------
// FoliageAttachment — port of FoliagePlacer.FoliageAttachment
// ---------------------------------------------------------------------------

struct FoliageAttachment {
    int x, y, z;
    int radiusOffset;
    bool doubleTrunk;
};

// ---------------------------------------------------------------------------
// World accessor passed to tree placers — bounds-checked wrapper over LevelChunk.
// Reads outside the chunk return 0 (air); writes outside are discarded.
// This matches Java behaviour where neighbouring chunks are assumed clear but
// inter-chunk leaf spill is not implemented yet.
// ---------------------------------------------------------------------------

struct TreeWorld {
    LevelChunk& chunk;
    int minX, minZ; // world-space origin of the chunk (chunkX*16, chunkZ*16)

    bool inBounds(int wx, int wz) const noexcept {
        return wx >= minX && wx < minX + 16 && wz >= minZ && wz < minZ + 16;
    }

    uint32_t getBlock(int wx, int wy, int wz) const {
        if (!inBounds(wx, wz) || wy < CHUNK_MIN_Y || wy >= CHUNK_MAX_Y) return 0;
        return chunk.getBlock(wx, wy, wz);
    }

    void setBlock(int wx, int wy, int wz, uint32_t stateId) {
        if (!inBounds(wx, wz) || wy < CHUNK_MIN_Y || wy >= CHUNK_MAX_Y) return;
        chunk.setBlock(wx, wy, wz, stateId);
    }

    // Port of TreeFeature.validTreePos:
    // true if the block is air or a non-opaque, non-fluid block (leaves, short grass, etc.)
    bool validTreePos(int wx, int wy, int wz) const;

    // Port of TrunkPlacer.isFree:
    // validTreePos || is a log block
    bool isFree(int wx, int wy, int wz) const;
};

// ---------------------------------------------------------------------------
// FoliagePlacer — abstract base, port of FoliagePlacer.java
// ---------------------------------------------------------------------------

struct TreeConfig; // forward

class FoliagePlacer {
public:
    IntVal radius;
    IntVal offset;

    FoliagePlacer(IntVal radius, IntVal offset)
        : radius(radius), offset(offset) {}
    virtual ~FoliagePlacer() = default;

    // Called once per FoliageAttachment by TreeFeature
    void createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
                        int treeHeight, const FoliageAttachment& att,
                        int foliageHeight, int leafRadius);

    virtual int  foliageHeight(RandomSource& rng, int treeHeight, const TreeConfig& config) = 0;
    virtual int  foliageRadius(RandomSource& rng, int trunkHeight) { return radius.sample(rng); }

protected:
    // Main per-layer dispatch; called by createFoliage with the sampled offset
    virtual void createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
                                int treeHeight, const FoliageAttachment& att,
                                int foliageHeight, int leafRadius, int offsetSample) = 0;

    // Place a row of leaves centred at (origin + (dx, y, dz)) within radius currentRadius
    void placeLeavesRow(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
                         int ox, int oy, int oz,
                         int currentRadius, int dy, bool doubleTrunk);

    // Returns true if this (dx, y, dz) should be skipped when drawing the row
    virtual bool shouldSkipLocation(RandomSource& rng, int dx, int y, int dz,
                                     int currentRadius, bool doubleTrunk) = 0;
};

// ---------------------------------------------------------------------------
// BlobFoliagePlacer  — oak, birch, jungle shrub
// BlobFoliagePlacer(radius, offset, height)
// ---------------------------------------------------------------------------

class BlobFoliagePlacer : public FoliagePlacer {
public:
    int height;

    BlobFoliagePlacer(IntVal radius, IntVal offset, int height)
        : FoliagePlacer(radius, offset), height(height) {}

    int foliageHeight(RandomSource& /*rng*/, int /*treeHeight*/, const TreeConfig& /*c*/) override {
        return height;
    }

protected:
    void createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
                        int treeHeight, const FoliageAttachment& att,
                        int foliageHeight, int leafRadius, int offsetSample) override;

    bool shouldSkipLocation(RandomSource& rng, int dx, int y, int dz,
                             int currentRadius, bool doubleTrunk) override {
        return dx == currentRadius && dz == currentRadius &&
               (rng.nextInt(2) == 0 || y == 0);
    }
};

// ---------------------------------------------------------------------------
// FancyFoliagePlacer  — fancy (large) oak; extends BlobFoliagePlacer
// Uses circular shouldSkipLocation
// ---------------------------------------------------------------------------

class FancyFoliagePlacer final : public BlobFoliagePlacer {
public:
    using BlobFoliagePlacer::BlobFoliagePlacer;

protected:
    void createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
                        int treeHeight, const FoliageAttachment& att,
                        int foliageHeight, int leafRadius, int offsetSample) override;

    bool shouldSkipLocation(RandomSource& /*rng*/, int dx, int /*y*/, int dz,
                             int currentRadius, bool /*doubleTrunk*/) override {
        float fdx = (float)dx + 0.5f;
        float fdz = (float)dz + 0.5f;
        return fdx * fdx + fdz * fdz > (float)(currentRadius * currentRadius);
    }
};

// ---------------------------------------------------------------------------
// SpruceFoliagePlacer  — spruce
// SpruceFoliagePlacer(radius, offset, trunkHeight)
// ---------------------------------------------------------------------------

class SpruceFoliagePlacer final : public FoliagePlacer {
public:
    IntVal trunkHeight;

    SpruceFoliagePlacer(IntVal radius, IntVal offset, IntVal trunkHeight)
        : FoliagePlacer(radius, offset), trunkHeight(trunkHeight) {}

    int foliageHeight(RandomSource& rng, int treeHeight, const TreeConfig& config) override {
        return std::max(4, treeHeight - trunkHeight.sample(rng));
    }

protected:
    void createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
                        int treeHeight, const FoliageAttachment& att,
                        int foliageHeight, int leafRadius, int offsetSample) override;

    bool shouldSkipLocation(RandomSource& /*rng*/, int dx, int /*y*/, int dz,
                             int currentRadius, bool /*doubleTrunk*/) override {
        return dx == currentRadius && dz == currentRadius && currentRadius > 0;
    }
};

// ---------------------------------------------------------------------------
// PineFoliagePlacer  — pine (tall narrow spruce variant)
// PineFoliagePlacer(radius, offset, height)
// ---------------------------------------------------------------------------

class PineFoliagePlacer final : public FoliagePlacer {
public:
    IntVal height;

    PineFoliagePlacer(IntVal radius, IntVal offset, IntVal height)
        : FoliagePlacer(radius, offset), height(height) {}

    int foliageHeight(RandomSource& rng, int /*treeHeight*/, const TreeConfig& /*c*/) override {
        return height.sample(rng);
    }
    int foliageRadius(RandomSource& rng, int trunkHeight) override {
        return FoliagePlacer::foliageRadius(rng, trunkHeight) +
               rng.nextInt(std::max(trunkHeight + 1, 1));
    }

protected:
    void createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
                        int treeHeight, const FoliageAttachment& att,
                        int foliageHeight, int leafRadius, int offsetSample) override;

    bool shouldSkipLocation(RandomSource& /*rng*/, int dx, int /*y*/, int dz,
                             int currentRadius, bool /*doubleTrunk*/) override {
        return dx == currentRadius && dz == currentRadius && currentRadius > 0;
    }
};

// ---------------------------------------------------------------------------
// AcaciaFoliagePlacer  — flat-topped acacia canopy
// AcaciaFoliagePlacer(radius, offset)
// ---------------------------------------------------------------------------

class AcaciaFoliagePlacer final : public FoliagePlacer {
public:
    AcaciaFoliagePlacer(IntVal radius, IntVal offset)
        : FoliagePlacer(radius, offset) {}

    int foliageHeight(RandomSource& /*rng*/, int /*treeHeight*/, const TreeConfig& /*c*/) override {
        return 0;
    }

protected:
    void createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
                        int treeHeight, const FoliageAttachment& att,
                        int foliageHeight, int leafRadius, int offsetSample) override;

    bool shouldSkipLocation(RandomSource& /*rng*/, int dx, int y, int dz,
                             int currentRadius, bool /*doubleTrunk*/) override {
        if (y == 0) return (dx > 1 || dz > 1) && dx != 0 && dz != 0;
        return dx == currentRadius && dz == currentRadius && currentRadius > 0;
    }
};

// ---------------------------------------------------------------------------
// TrunkPlacer — abstract base
// ---------------------------------------------------------------------------

class TrunkPlacer {
public:
    int baseHeight;
    int heightRandA;
    int heightRandB;

    TrunkPlacer(int baseHeight, int heightRandA, int heightRandB)
        : baseHeight(baseHeight), heightRandA(heightRandA), heightRandB(heightRandB) {}
    virtual ~TrunkPlacer() = default;

    int getTreeHeight(RandomSource& rng) const {
        return baseHeight + rng.nextInt(heightRandA + 1) + rng.nextInt(heightRandB + 1);
    }

    // Returns the list of foliage attachment points after placing the trunk.
    virtual std::vector<FoliageAttachment> placeTrunk(
        TreeWorld& world, RandomSource& rng,
        int treeHeight, int originX, int originY, int originZ,
        const TreeConfig& config) = 0;

    // True if the block at pos does not obstruct the trunk.
    bool isFree(TreeWorld& world, int wx, int wy, int wz) const {
        return world.isFree(wx, wy, wz);
    }

protected:
    // Place a log at (wx,wy,wz). Returns true if the position was valid.
    bool placeLog(TreeWorld& world, RandomSource& rng,
                   int wx, int wy, int wz, const TreeConfig& config,
                   int axisOverride = -1); // -1 = use config's logId directly
};

// ---------------------------------------------------------------------------
// StraightTrunkPlacer  — all simple vertical trunks (oak, birch, spruce, pine, jungle)
// ---------------------------------------------------------------------------

class StraightTrunkPlacer final : public TrunkPlacer {
public:
    using TrunkPlacer::TrunkPlacer;

    std::vector<FoliageAttachment> placeTrunk(
        TreeWorld& world, RandomSource& rng,
        int treeHeight, int ox, int oy, int oz,
        const TreeConfig& config) override;
};

// ---------------------------------------------------------------------------
// ForkingTrunkPlacer  — acacia: leaning trunk with one branch
// ---------------------------------------------------------------------------

class ForkingTrunkPlacer final : public TrunkPlacer {
public:
    using TrunkPlacer::TrunkPlacer;

    std::vector<FoliageAttachment> placeTrunk(
        TreeWorld& world, RandomSource& rng,
        int treeHeight, int ox, int oy, int oz,
        const TreeConfig& config) override;
};

// ---------------------------------------------------------------------------
// FancyTrunkPlacer  — large oak with branching
// ---------------------------------------------------------------------------

class FancyTrunkPlacer final : public TrunkPlacer {
public:
    using TrunkPlacer::TrunkPlacer;

    std::vector<FoliageAttachment> placeTrunk(
        TreeWorld& world, RandomSource& rng,
        int treeHeight, int ox, int oy, int oz,
        const TreeConfig& config) override;

private:
    struct FoliageCoords {
        FoliageAttachment attachment;
        int branchBase;
    };

    // Bresenham-style limb: check clearance (doPlace=false) or place logs (doPlace=true)
    bool makeLimb(TreeWorld& world, RandomSource& rng,
                   int x0, int y0, int z0,
                   int x1, int y1, int z1,
                   bool doPlace,
                   const TreeConfig& config);

    void makeBranches(TreeWorld& world, RandomSource& rng,
                       int height, int ox, int oy, int oz,
                       const std::vector<FoliageCoords>& coords,
                       const TreeConfig& config);

    static float treeShape(int height, int y);
    bool trimBranches(int height, int localY) const { return localY >= height * 0.2; }
};

// ---------------------------------------------------------------------------
// TreeConfig  — holds everything needed to grow one type of tree
// Mirrors TreeConfiguration in Java (simplified: no codec, no decorators)
// ---------------------------------------------------------------------------

struct TreeConfig {
    uint32_t logStateId;      // Y-axis log state (upright trunk)
    uint32_t logXStateId;     // X-axis log state (horizontal branch)
    uint32_t logZStateId;     // Z-axis log state (horizontal branch)
    uint32_t leavesStateId;   // leaf state
    uint32_t dirtStateId;     // placed below trunk (belowTrunkProvider → dirt)

    std::shared_ptr<TrunkPlacer>   trunkPlacer;
    std::shared_ptr<FoliagePlacer> foliagePlacer;
    std::shared_ptr<FeatureSize>   minimumSize;
    bool ignoreVines = true;
};

// ---------------------------------------------------------------------------
// Built-in tree configs (from TreeFeatures.java)
// Must be called AFTER initBlocks().
// ---------------------------------------------------------------------------

TreeConfig makeOakConfig();
TreeConfig makeBirchConfig();
TreeConfig makeSpruceConfig();
TreeConfig makePineConfig();
TreeConfig makeAcaciaConfig();
TreeConfig makeFancyOakConfig();

// ---------------------------------------------------------------------------
// TreeFeature placement — port of TreeFeature.place()
// Returns true if the tree was successfully placed.
// originX/Y/Z is the world-space position of the first log (above ground).
// ---------------------------------------------------------------------------

bool placeTree(TreeWorld& world, RandomSource& rng,
                int originX, int originY, int originZ,
                const TreeConfig& config);

// ---------------------------------------------------------------------------
// Chunk decoration entry point
// Called by NoiseBasedChunkGenerator after buildSurface().
// Scatters trees across the chunk using a per-chunk seeded random.
// ---------------------------------------------------------------------------

void decorateChunk(LevelChunk& chunk, uint64_t worldSeed, const std::function<std::string(int, int, int)>& biomeGetter);

} // namespace mc::levelgen::feature


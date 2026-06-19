#pragma once

// Port of net.minecraft.world.level.levelgen.placement.PlacementContext and the
// minimal WorldGenLevel surface that placement modifiers / features query.
// PlacementContext IS-A WorldGenerationContext (so HeightProviders resolve
// against its vertical bounds) and also forwards world queries to the level.

#include "../Heightmap.h"
#include "../WorldGenerationContext.h"
#include "../../../../core/Math.h"

#include <stdexcept>
#include <string>

namespace mc::levelgen::placement {

using mc::BlockPos;
using mc::levelgen::Heightmap;
using mc::levelgen::WorldGenerationContext;

// The world-access surface used during decoration. Placement modifiers only
// need getHeight/getMinY; features additionally read/write blocks. The
// block-read/write/survival methods have throwing defaults so placement-only
// stubs need not implement them. `canSurvive` is the boundary to the (separate)
// block-behaviour subsystem; a BlockState here is its canonical id string.
class WorldGenLevel {
public:
    virtual ~WorldGenLevel() = default;
    virtual int getHeight(Heightmap::Types type, int x, int z) const = 0;
    virtual int getMinY() const = 0;

    virtual std::string getBlockState(BlockPos pos) const { (void)pos; throw std::logic_error("getBlockState not implemented"); }
    virtual void setBlock(BlockPos pos, const std::string& state, int flags) { (void)pos; (void)state; (void)flags; throw std::logic_error("setBlock not implemented"); }
    // WorldGenRegion.setBlock returns a bool: false when ensureCanWrite rejects the
    // position, true after the write (WorldGenRegion.java:264-301). Features whose
    // control flow consumes that result (MultifaceSpreader.SpreadConfig.placeBlock,
    // BubbleColumnBlock.updateColumn's climb loop) call this; the void setBlock
    // remains for callers that ignore it.
    virtual bool setBlockChecked(BlockPos pos, const std::string& state, int flags) {
        if (!ensureCanWrite(pos)) return false;
        setBlock(pos, state, flags);
        return true;
    }
    virtual bool isEmptyBlock(BlockPos pos) const { (void)pos; throw std::logic_error("isEmptyBlock not implemented"); }
    // ChunkAccess.markPosForPostprocessing(pos) on the chunk CONTAINING pos (features
    // call level.getChunk(pos).markPosForPostprocessing(pos) — no write-radius gate:
    // Feature.markAboveForPostProcessing Feature.java:206-217, MultifaceGrowthFeature
    // .java:74, MultifaceSpreader.java:172). Throwing default: only the multi-chunk
    // decoration level implements the mark store; stubs must not silently drop marks.
    virtual void markPosForPostprocessing(BlockPos pos) { (void)pos; throw std::logic_error("markPosForPostprocessing not implemented"); }
    // BlockState.canSurvive(level, pos): delegates to the block-behaviour subsystem.
    virtual bool canSurvive(const std::string& state, BlockPos pos) const { (void)state; (void)pos; throw std::logic_error("canSurvive not implemented"); }
    // WorldGenRegion.ensureCanWrite: whether a feature may write at pos (radius-1 +
    // build-height gating in the real region; Java's default and the single-chunk
    // Proxy both return true).
    virtual bool ensureCanWrite(BlockPos pos) const { (void)pos; return true; }
};

class PlacementContext : public WorldGenerationContext {
public:
    PlacementContext(WorldGenLevel* level, int minGenY, int genDepth)
        : WorldGenerationContext(minGenY, genDepth), m_level(level) {}

    int getHeight(Heightmap::Types type, int x, int z) const { return m_level->getHeight(type, x, z); }
    int getMinY() const { return m_level->getMinY(); }
    WorldGenLevel* getLevel() const { return m_level; }

private:
    WorldGenLevel* m_level;
};

} // namespace mc::levelgen::placement

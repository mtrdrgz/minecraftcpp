#pragma once

// Port of net.minecraft.world.level.levelgen.SurfaceRules
// Port of net.minecraft.world.level.levelgen.VerticalAnchor
// Port of net.minecraft.world.level.levelgen.placement.CaveSurface

#include "../chunk/LevelChunk.h"
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mc::levelgen {

class SurfaceSystem;
class RandomState;

// ---------- CaveSurface ----------

enum class CaveSurface { CEILING, FLOOR };

// ---------- VerticalAnchor ----------

struct VerticalAnchor {
    enum class Type { Absolute, AboveBottom, BelowTop };
    Type type  = Type::Absolute;
    int  value = 0;

    int resolveY(int minGenY, int genDepth) const noexcept {
        switch (type) {
            case Type::Absolute:    return value;
            case Type::AboveBottom: return minGenY + value;
            case Type::BelowTop:    return genDepth - 1 + minGenY - value;
        }
        return value;
    }

    static VerticalAnchor absolute(int y)    { return {Type::Absolute,    y}; }
    static VerticalAnchor aboveBottom(int o) { return {Type::AboveBottom,  o}; }
    static VerticalAnchor belowTop(int o)    { return {Type::BelowTop,     o}; }
    static VerticalAnchor bottom()           { return aboveBottom(0); }
    static VerticalAnchor top()              { return belowTop(0); }
};

// ---------- WorldGenCtx (minimal port of WorldGenerationContext) ----------

struct WorldGenCtx {
    int minGenY  = -64;
    int genDepth = 384;
};

// ==========================================================================
// SurfaceRules namespace — mirrors the Java SurfaceRules outer class
// ==========================================================================

namespace SurfaceRules {

struct Context;

// Core interfaces
class ISurfaceRule {
public:
    virtual ~ISurfaceRule() = default;
    virtual std::optional<uint32_t> tryApply(int blockX, int blockY, int blockZ) = 0;
};

class ICondition {
public:
    virtual ~ICondition() = default;
    virtual bool test() = 0;
};

class IRuleSource {
public:
    virtual ~IRuleSource() = default;
    virtual std::shared_ptr<ISurfaceRule> apply(Context& ctx) = 0;
};

class IConditionSource {
public:
    virtual ~IConditionSource() = default;
    virtual std::shared_ptr<ICondition> apply(Context& ctx) = 0;
};

using SurfaceRulePtr     = std::shared_ptr<ISurfaceRule>;
using ConditionPtr       = std::shared_ptr<ICondition>;
using RuleSourcePtr      = std::shared_ptr<IRuleSource>;
using ConditionSourcePtr = std::shared_ptr<IConditionSource>;

// ---------- Context (port of SurfaceRules.Context inner class) ----------

struct Context {
    // Raw pointers to objects that outlive this context
    SurfaceSystem* system      = nullptr;
    LevelChunk*    chunk       = nullptr;
    RandomState*   randomState = nullptr;

    // Preliminary surface level sampler (replaces NoiseChunk.preliminarySurfaceLevel)
    std::function<int(int, int)> prelimSurfFn;

    // Biome key getter — returns "minecraft:xxx" or "" if no biome system available
    std::function<std::string(int, int, int)> biomeGetter;

    WorldGenCtx genCtx;

    // Inner conditions, created by initConditions()
    ConditionPtr temperature;
    ConditionPtr steep;
    ConditionPtr hole;
    ConditionPtr abovePreliminarySurface;

    // Lazy-caching counters — initialised far below 0 so first access always computes
    int64_t lastUpdateXZ = std::numeric_limits<int64_t>::min() + 1;
    int64_t lastUpdateY  = std::numeric_limits<int64_t>::min() + 1;

    // XZ state
    int blockX       = 0;
    int blockZ       = 0;
    int surfaceDepth = 0;

    // Y state
    int blockY          = 0;
    int waterHeight     = std::numeric_limits<int>::min();
    int stoneDepthAbove = 0;
    int stoneDepthBelow = 0;

    // Cached surface secondary (lazy, invalidated per XZ)
    int64_t lastSurfaceDepth2Update;
    double  surfaceSecondary = 0.0;

    // Cached min surface level (lazy, invalidated per XZ)
    int64_t lastMinSurfaceLevelUpdate;
    int     minSurfaceLevel  = 0;
    int64_t lastPrelimCellOrigin = std::numeric_limits<int64_t>::max();
    int     prelimSurfaceCache[4] = {};

    // Per-Y biome cache (reset in updateY)
    std::string cachedBiome;
    bool        biomeCached = false;

    Context() noexcept {
        lastSurfaceDepth2Update  = lastUpdateXZ - 1;
        lastMinSurfaceLevelUpdate = lastUpdateXZ - 1;
    }

    // Called once, after all pointer fields are set
    void initConditions();

    void updateXZ(int bx, int bz);
    void updateY(int stoneAbove, int stoneBelow, int waterH, int bx, int by, int bz);

    double getSurfaceSecondary();
    int    getMinSurfaceLevel();
    int    getSeaLevel() const noexcept;

    // Returns the biome key for the current position (lazily fetched per Y update)
    const std::string& getBiome();
};

// ---------- Factory functions (mirror Java static methods) ----------

ConditionSourcePtr stoneDepthCheck(int offset, bool addSurfaceDepth, CaveSurface surface);
ConditionSourcePtr stoneDepthCheck(int offset, bool addSurfaceDepth, int secondaryDepthRange,
                                    CaveSurface surface);
ConditionSourcePtr notCond(ConditionSourcePtr target);
ConditionSourcePtr yBlockCheck(VerticalAnchor anchor, int surfaceDepthMultiplier);
ConditionSourcePtr yStartCheck(VerticalAnchor anchor, int surfaceDepthMultiplier);
ConditionSourcePtr waterBlockCheck(int offset, int surfaceDepthMultiplier);
ConditionSourcePtr waterStartCheck(int offset, int surfaceDepthMultiplier);
ConditionSourcePtr isBiome(std::vector<std::string> biomeKeys);
ConditionSourcePtr noiseCondition(const std::string& noiseKey, double minRange,
                                   double maxRange = std::numeric_limits<double>::max());
ConditionSourcePtr verticalGradient(std::string randomName, VerticalAnchor trueAtAndBelow,
                                     VerticalAnchor falseAtAndAbove);
ConditionSourcePtr steepCond();
ConditionSourcePtr holeCond();
ConditionSourcePtr abovePreliminarySurfaceCond();
ConditionSourcePtr temperatureCond();

RuleSourcePtr ifTrue(ConditionSourcePtr cond, RuleSourcePtr thenRun);
RuleSourcePtr sequence(std::vector<RuleSourcePtr> rules);
RuleSourcePtr blockState(uint32_t stateId);
RuleSourcePtr bandlands();

// Equivalents of the Java public static final ConditionSource fields
inline ConditionSourcePtr ON_FLOOR()              { return stoneDepthCheck(0, false,  CaveSurface::FLOOR);   }
inline ConditionSourcePtr UNDER_FLOOR()           { return stoneDepthCheck(0, true,   CaveSurface::FLOOR);   }
inline ConditionSourcePtr DEEP_UNDER_FLOOR()      { return stoneDepthCheck(0, true, 6, CaveSurface::FLOOR);  }
inline ConditionSourcePtr VERY_DEEP_UNDER_FLOOR() { return stoneDepthCheck(0, true, 30, CaveSurface::FLOOR); }
inline ConditionSourcePtr ON_CEILING()            { return stoneDepthCheck(0, false,  CaveSurface::CEILING); }
inline ConditionSourcePtr UNDER_CEILING()         { return stoneDepthCheck(0, true,   CaveSurface::CEILING); }

} // namespace SurfaceRules
} // namespace mc::levelgen

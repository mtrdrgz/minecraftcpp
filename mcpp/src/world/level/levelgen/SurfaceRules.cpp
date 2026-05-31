#include "SurfaceRules.h"
#include "SurfaceSystem.h"
#include "RandomState.h"
#include "RandomSource.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include <unordered_set>
#include <unordered_map>

namespace mc::levelgen::SurfaceRules {

// ==========================================================================
// Internal helpers
// ==========================================================================

namespace {

static int64_t packXZ(int x, int z) noexcept {
    return (static_cast<int64_t>(static_cast<uint32_t>(x)) << 32) |
           static_cast<uint32_t>(z);
}

static double lerp(double t, double a, double b) noexcept { return a + t * (b - a); }

static double lerp2(double t0, double t1, double x00, double x10, double x01, double x11) noexcept {
    return lerp(t1, lerp(t0, x00, x10), lerp(t0, x01, x11));
}

// ---------- Lazy condition base classes ----------

class LazyCondition : public ICondition {
public:
    LazyCondition(Context* ctx, int64_t initTimestamp) noexcept
        : m_ctx(ctx), m_lastUpdate(initTimestamp - 1) {}

    bool test() override {
        int64_t now = contextLastUpdate();
        if (now != m_lastUpdate) {
            m_lastUpdate = now;
            m_result     = compute();
        }
        return m_result;
    }

protected:
    Context* m_ctx;

    virtual int64_t contextLastUpdate() const noexcept = 0;
    virtual bool    compute()                          = 0;

private:
    int64_t m_lastUpdate;
    bool    m_result = false;
};

class LazyXZCondition : public LazyCondition {
public:
    explicit LazyXZCondition(Context* ctx) noexcept : LazyCondition(ctx, ctx->lastUpdateXZ) {}
protected:
    int64_t contextLastUpdate() const noexcept override { return m_ctx->lastUpdateXZ; }
};

class LazyYCondition : public LazyCondition {
public:
    explicit LazyYCondition(Context* ctx) noexcept : LazyCondition(ctx, ctx->lastUpdateY) {}
protected:
    int64_t contextLastUpdate() const noexcept override { return m_ctx->lastUpdateY; }
};

// ---------- Inner conditions created by Context::initConditions ----------

class HoleCondition final : public LazyXZCondition {
public:
    explicit HoleCondition(Context* ctx) noexcept : LazyXZCondition(ctx) {}
protected:
    bool compute() override { return m_ctx->surfaceDepth <= 0; }
};

class SteepCondition final : public LazyXZCondition {
public:
    explicit SteepCondition(Context* ctx) noexcept : LazyXZCondition(ctx) {}
protected:
    bool compute() override {
        int lx   = m_ctx->blockX & 15;
        int lz   = m_ctx->blockZ & 15;
        int zN   = std::max(lz - 1, 0);
        int zS   = std::min(lz + 1, 15);
        LevelChunk* c = m_ctx->chunk;
        // heightmap() returns world Y of topmost solid block (same as Java getHeight()-1)
        int hN = c->heightmap(lx, zN);
        int hS = c->heightmap(lx, zS);
        if (hS >= hN + 4) return true;
        int xW = std::max(lx - 1, 0);
        int xE = std::min(lx + 1, 15);
        int hW = c->heightmap(xW, lz);
        int hE = c->heightmap(xE, lz);
        return hW >= hE + 4;
    }
};

class TemperatureCondition final : public LazyYCondition {
public:
    explicit TemperatureCondition(Context* ctx) noexcept : LazyYCondition(ctx) {}
protected:
    // Returns true when the biome is cold enough to snow at blockY.
    bool compute() override {
        const std::string& biome = m_ctx->getBiome();
        if (biome.find("frozen") != std::string::npos ||
            biome.find("snowy") != std::string::npos ||
            biome == "minecraft:grove" ||
            biome == "minecraft:jagged_peaks" ||
            biome == "minecraft:ice_spikes") {
            return true;
        }
        return false;
    }
};

class AbovePreliminarySurfaceCondition final : public ICondition {
public:
    explicit AbovePreliminarySurfaceCondition(Context* ctx) noexcept : m_ctx(ctx) {}
    bool test() override { return m_ctx->blockY >= m_ctx->getMinSurfaceLevel(); }
private:
    Context* m_ctx;
};

// ---------- Singleton condition sources (no state) ----------

class TemperatureCS final : public IConditionSource {
public:
    ConditionPtr apply(Context& ctx) override { return ctx.temperature; }
};
class SteepCS final : public IConditionSource {
public:
    ConditionPtr apply(Context& ctx) override { return ctx.steep; }
};
class HoleCS final : public IConditionSource {
public:
    ConditionPtr apply(Context& ctx) override { return ctx.hole; }
};
class AbovePreliminarySurfaceCS final : public IConditionSource {
public:
    ConditionPtr apply(Context& ctx) override { return ctx.abovePreliminarySurface; }
};

// ---------- StoneDepthCheck ----------

class StoneDepthCondition final : public LazyYCondition {
public:
    StoneDepthCondition(Context* ctx, int offset, bool addSurface, int secRange, bool ceiling) noexcept
        : LazyYCondition(ctx)
        , m_offset(offset)
        , m_addSurfaceDepth(addSurface)
        , m_secondaryDepthRange(secRange)
        , m_ceiling(ceiling) {}
protected:
    bool compute() override {
        int stoneDepth = m_ceiling ? m_ctx->stoneDepthBelow : m_ctx->stoneDepthAbove;
        int surfDepth  = m_addSurfaceDepth ? m_ctx->surfaceDepth : 0;
        int secDepth   = 0;
        if (m_secondaryDepthRange != 0) {
            double t = (m_ctx->getSurfaceSecondary() + 1.0) * 0.5; // map [-1,1] -> [0,1]
            secDepth = (int)(t * m_secondaryDepthRange);
        }
        return stoneDepth <= 1 + m_offset + surfDepth + secDepth;
    }
private:
    int  m_offset;
    bool m_addSurfaceDepth;
    int  m_secondaryDepthRange;
    bool m_ceiling;
};

class StoneDepthCS final : public IConditionSource {
public:
    StoneDepthCS(int offset, bool addSurface, int secRange, CaveSurface surface) noexcept
        : m_offset(offset)
        , m_addSurface(addSurface)
        , m_secRange(secRange)
        , m_ceiling(surface == CaveSurface::CEILING) {}

    ConditionPtr apply(Context& ctx) override {
        return std::make_shared<StoneDepthCondition>(&ctx, m_offset, m_addSurface, m_secRange, m_ceiling);
    }
private:
    int  m_offset;
    bool m_addSurface;
    int  m_secRange;
    bool m_ceiling;
};

// ---------- Not condition ----------

class NotCondition final : public ICondition {
public:
    explicit NotCondition(ConditionPtr target) : m_target(std::move(target)) {}
    bool test() override { return !m_target->test(); }
private:
    ConditionPtr m_target;
};

class NotCS final : public IConditionSource {
public:
    explicit NotCS(ConditionSourcePtr target) : m_target(std::move(target)) {}
    ConditionPtr apply(Context& ctx) override {
        return std::make_shared<NotCondition>(m_target->apply(ctx));
    }
private:
    ConditionSourcePtr m_target;
};

// ---------- Y conditions ----------

// Port of YConditionSource — checks blockY (+ optional stoneDepthAbove) vs anchor
class YCondition final : public LazyYCondition {
public:
    YCondition(Context* ctx, int anchorY, int multiplier, bool addStoneDepth) noexcept
        : LazyYCondition(ctx)
        , m_anchorY(anchorY)
        , m_multiplier(multiplier)
        , m_addStoneDepth(addStoneDepth) {}
protected:
    bool compute() override {
        int effective = m_ctx->blockY + (m_addStoneDepth ? m_ctx->stoneDepthAbove : 0);
        return effective >= m_anchorY + m_ctx->surfaceDepth * m_multiplier;
    }
private:
    int  m_anchorY;
    int  m_multiplier;
    bool m_addStoneDepth;
};

// yBlockCheck: addStoneDepth = false  (Java: YConditionSource(anchor, mult, false))
// yStartCheck: addStoneDepth = true   (Java: YConditionSource(anchor, mult, true))
class YConditionCS final : public IConditionSource {
public:
    YConditionCS(VerticalAnchor anchor, int multiplier, bool addStoneDepth) noexcept
        : m_anchor(anchor)
        , m_multiplier(multiplier)
        , m_addStoneDepth(addStoneDepth) {}
    ConditionPtr apply(Context& ctx) override {
        int ay = m_anchor.resolveY(ctx.genCtx.minGenY, ctx.genCtx.genDepth);
        return std::make_shared<YCondition>(&ctx, ay, m_multiplier, m_addStoneDepth);
    }
private:
    VerticalAnchor m_anchor;
    int            m_multiplier;
    bool           m_addStoneDepth;
};

// ---------- Water condition ----------

// Port of WaterConditionSource
class WaterCondition final : public LazyYCondition {
public:
    WaterCondition(Context* ctx, int offset, int multiplier, bool addStoneDepth) noexcept
        : LazyYCondition(ctx)
        , m_offset(offset)
        , m_multiplier(multiplier)
        , m_addStoneDepth(addStoneDepth) {}
protected:
    bool compute() override {
        return m_ctx->waterHeight == std::numeric_limits<int>::min() ||
               m_ctx->blockY + (m_addStoneDepth ? m_ctx->stoneDepthAbove : 0) >=
               m_ctx->waterHeight + m_offset + m_ctx->surfaceDepth * m_multiplier;
    }
private:
    int  m_offset;
    int  m_multiplier;
    bool m_addStoneDepth;
};

class WaterCS final : public IConditionSource {
public:
    WaterCS(int offset, int multiplier, bool addStoneDepth) noexcept
        : m_offset(offset)
        , m_multiplier(multiplier)
        , m_addStoneDepth(addStoneDepth) {}
    ConditionPtr apply(Context& ctx) override {
        return std::make_shared<WaterCondition>(&ctx, m_offset, m_multiplier, m_addStoneDepth);
    }
private:
    int  m_offset;
    int  m_multiplier;
    bool m_addStoneDepth;
};

// ---------- Biome condition ----------

class BiomeCondition final : public LazyYCondition {
public:
    BiomeCondition(Context* ctx, std::unordered_set<std::string> biomes) noexcept
        : LazyYCondition(ctx)
        , m_biomes(std::move(biomes)) {}
protected:
    bool compute() override {
        const std::string& biome = m_ctx->getBiome();
        return !biome.empty() && m_biomes.count(biome) > 0;
    }
private:
    std::unordered_set<std::string> m_biomes;
};

class BiomeCS final : public IConditionSource {
public:
    explicit BiomeCS(std::vector<std::string> biomeKeys) : m_biomes(biomeKeys.begin(), biomeKeys.end()) {}
    ConditionPtr apply(Context& ctx) override {
        return std::make_shared<BiomeCondition>(&ctx, m_biomes);
    }
private:
    std::unordered_set<std::string> m_biomes;
};

// ---------- Noise threshold condition ----------

class NoiseThresholdCondition final : public LazyXZCondition {
public:
    NoiseThresholdCondition(Context* ctx,
                            std::shared_ptr<NormalNoise> noise,
                            double minT, double maxT) noexcept
        : LazyXZCondition(ctx)
        , m_noise(std::move(noise))
        , m_min(minT)
        , m_max(maxT) {}
protected:
    bool compute() override {
        double v = m_noise->getValue(m_ctx->blockX, 0.0, m_ctx->blockZ);
        return v >= m_min && v <= m_max;
    }
private:
    std::shared_ptr<NormalNoise> m_noise;
    double m_min;
    double m_max;
};

class NoiseThresholdCS final : public IConditionSource {
public:
    NoiseThresholdCS(std::string noiseKey, double minR, double maxR)
        : m_key(std::move(noiseKey)), m_min(minR), m_max(maxR) {}
    ConditionPtr apply(Context& ctx) override {
        auto noise = ctx.randomState->getOrCreateNoise(m_key);
        return std::make_shared<NoiseThresholdCondition>(&ctx, std::move(noise), m_min, m_max);
    }
private:
    std::string m_key;
    double m_min;
    double m_max;
};

// ---------- Vertical gradient condition ----------

class VerticalGradientCondition final : public LazyYCondition {
public:
    VerticalGradientCondition(Context* ctx,
                               std::shared_ptr<PositionalRandomFactory> factory,
                               int trueAtAndBelow,
                               int falseAtAndAbove) noexcept
        : LazyYCondition(ctx)
        , m_factory(std::move(factory))
        , m_trueAtAndBelow(trueAtAndBelow)
        , m_falseAtAndAbove(falseAtAndAbove) {}
protected:
    bool compute() override {
        int y = m_ctx->blockY;
        if (y <= m_trueAtAndBelow)  return true;
        if (y >= m_falseAtAndAbove) return false;
        double prob = (double)(m_falseAtAndAbove - y) / (double)(m_falseAtAndAbove - m_trueAtAndBelow);
        auto rng = m_factory->at(m_ctx->blockX, y, m_ctx->blockZ);
        return rng->nextFloat() < (float)prob;
    }
private:
    std::shared_ptr<PositionalRandomFactory> m_factory;
    int m_trueAtAndBelow;
    int m_falseAtAndAbove;
};

class VerticalGradientCS final : public IConditionSource {
public:
    VerticalGradientCS(std::string randomName, VerticalAnchor trueAtAndBelow,
                        VerticalAnchor falseAtAndAbove)
        : m_randomName(std::move(randomName))
        , m_trueAt(trueAtAndBelow)
        , m_falseAt(falseAtAndAbove) {}

    ConditionPtr apply(Context& ctx) override {
        int trueY  = m_trueAt.resolveY(ctx.genCtx.minGenY, ctx.genCtx.genDepth);
        int falseY = m_falseAt.resolveY(ctx.genCtx.minGenY, ctx.genCtx.genDepth);
        auto factory = ctx.randomState->getOrCreateRandomFactory(m_randomName);
        return std::make_shared<VerticalGradientCondition>(&ctx, std::move(factory), trueY, falseY);
    }
private:
    std::string    m_randomName;
    VerticalAnchor m_trueAt;
    VerticalAnchor m_falseAt;
};

// ==========================================================================
// SurfaceRule implementations
// ==========================================================================

// StateRule — always returns the fixed block state
class StateRule final : public ISurfaceRule {
public:
    explicit StateRule(uint32_t stateId) noexcept : m_stateId(stateId) {}
    std::optional<uint32_t> tryApply(int, int, int) override { return m_stateId; }
private:
    uint32_t m_stateId;
};

// SequenceRule — tries each rule in order, returns first non-null result
class SequenceRule final : public ISurfaceRule {
public:
    explicit SequenceRule(std::vector<SurfaceRulePtr> rules) : m_rules(std::move(rules)) {}
    std::optional<uint32_t> tryApply(int bx, int by, int bz) override {
        for (auto& rule : m_rules) {
            auto result = rule->tryApply(bx, by, bz);
            if (result.has_value()) return result;
        }
        return std::nullopt;
    }
private:
    std::vector<SurfaceRulePtr> m_rules;
};

// TestRule — if condition passes, delegate to inner rule
class TestRule final : public ISurfaceRule {
public:
    TestRule(ConditionPtr cond, SurfaceRulePtr inner)
        : m_cond(std::move(cond)), m_inner(std::move(inner)) {}
    std::optional<uint32_t> tryApply(int bx, int by, int bz) override {
        if (!m_cond->test()) return std::nullopt;
        return m_inner->tryApply(bx, by, bz);
    }
private:
    ConditionPtr   m_cond;
    SurfaceRulePtr m_inner;
};

// BandlandsRule — delegates to SurfaceSystem::getBand
class BandlandsRule final : public ISurfaceRule {
public:
    explicit BandlandsRule(Context* ctx) noexcept : m_ctx(ctx) {}
    std::optional<uint32_t> tryApply(int bx, int by, int bz) override;
private:
    Context* m_ctx;
};

// ==========================================================================
// RuleSource implementations
// ==========================================================================

class BlockRuleSource final : public IRuleSource {
public:
    explicit BlockRuleSource(uint32_t stateId) noexcept : m_stateId(stateId) {}
    SurfaceRulePtr apply(Context&) override {
        return std::make_shared<StateRule>(m_stateId);
    }
private:
    uint32_t m_stateId;
};

class SequenceRuleSource final : public IRuleSource {
public:
    explicit SequenceRuleSource(std::vector<RuleSourcePtr> sources)
        : m_sources(std::move(sources)) {}
    SurfaceRulePtr apply(Context& ctx) override {
        if (m_sources.size() == 1) return m_sources[0]->apply(ctx);
        std::vector<SurfaceRulePtr> rules;
        rules.reserve(m_sources.size());
        for (auto& src : m_sources) rules.push_back(src->apply(ctx));
        return std::make_shared<SequenceRule>(std::move(rules));
    }
private:
    std::vector<RuleSourcePtr> m_sources;
};

class TestRuleSource final : public IRuleSource {
public:
    TestRuleSource(ConditionSourcePtr cond, RuleSourcePtr thenRun)
        : m_cond(std::move(cond)), m_then(std::move(thenRun)) {}
    SurfaceRulePtr apply(Context& ctx) override {
        return std::make_shared<TestRule>(m_cond->apply(ctx), m_then->apply(ctx));
    }
private:
    ConditionSourcePtr m_cond;
    RuleSourcePtr      m_then;
};

class BandlandsRuleSource final : public IRuleSource {
public:
    SurfaceRulePtr apply(Context& ctx) override {
        return std::make_shared<BandlandsRule>(&ctx);
    }
};

} // anonymous namespace

// ==========================================================================
// BandlandsRule::tryApply — defined here to break the forward-declaration loop
// (requires SurfaceSystem which is included via SurfaceSystem.h in SurfaceRules.cpp)
// ==========================================================================

} // namespace mc::levelgen::SurfaceRules

#include "SurfaceSystem.h" // needed for getBand

namespace mc::levelgen::SurfaceRules {
namespace {

std::optional<uint32_t> BandlandsRule::tryApply(int bx, int by, int bz) {
    uint32_t band = m_ctx->system->getBand(bx, by, bz);
    return band;
}

} // anonymous namespace

// ==========================================================================
// Context methods
// ==========================================================================

void Context::initConditions() {
    temperature             = std::make_shared<TemperatureCondition>(this);
    steep                   = std::make_shared<SteepCondition>(this);
    hole                    = std::make_shared<HoleCondition>(this);
    abovePreliminarySurface = std::make_shared<AbovePreliminarySurfaceCondition>(this);
}

void Context::updateXZ(int bx, int bz) {
    ++lastUpdateXZ;
    ++lastUpdateY;
    blockX = bx;
    blockZ = bz;
    surfaceDepth = system->getSurfaceDepth(bx, bz);
}

void Context::updateY(int stoneAbove, int stoneBelow, int waterH, int bx, int by, int bz) {
    ++lastUpdateY;
    blockY          = by;
    waterHeight     = waterH;
    stoneDepthAbove = stoneAbove;
    stoneDepthBelow = stoneBelow;
    biomeCached     = false; // reset biome cache for new Y position
    (void)bx; (void)bz;
}

double Context::getSurfaceSecondary() {
    if (lastSurfaceDepth2Update != lastUpdateXZ) {
        lastSurfaceDepth2Update = lastUpdateXZ;
        surfaceSecondary = system->getSurfaceSecondary(blockX, blockZ);
    }
    return surfaceSecondary;
}

int Context::getMinSurfaceLevel() {
    if (lastMinSurfaceLevelUpdate != lastUpdateXZ) {
        lastMinSurfaceLevelUpdate = lastUpdateXZ;
        int cornerCellX = blockX >> 4;
        int cornerCellZ = blockZ >> 4;
        int64_t origin  = packXZ(cornerCellX, cornerCellZ);
        if (lastPrelimCellOrigin != origin) {
            lastPrelimCellOrigin = origin;
            int bx0 = cornerCellX << 4;
            int bz0 = cornerCellZ << 4;
            prelimSurfaceCache[0] = prelimSurfFn(bx0,      bz0);
            prelimSurfaceCache[1] = prelimSurfFn(bx0 + 16, bz0);
            prelimSurfaceCache[2] = prelimSurfFn(bx0,      bz0 + 16);
            prelimSurfaceCache[3] = prelimSurfFn(bx0 + 16, bz0 + 16);
        }
        double q0 = (blockX & 15) / 16.0;
        double q1 = (blockZ & 15) / 16.0;
        int prelim = (int)std::floor(
            lerp2(q0, q1,
                  prelimSurfaceCache[0], prelimSurfaceCache[1],
                  prelimSurfaceCache[2], prelimSurfaceCache[3]));
        minSurfaceLevel = prelim + surfaceDepth - 8;
    }
    return minSurfaceLevel;
}

int Context::getSeaLevel() const noexcept {
    return system->getSeaLevel();
}

const std::string& Context::getBiome() {
    if (!biomeCached) {
        cachedBiome = biomeGetter(blockX, blockY, blockZ);
        biomeCached = true;
    }
    return cachedBiome;
}

// ==========================================================================
// Factory function implementations
// ==========================================================================

ConditionSourcePtr stoneDepthCheck(int offset, bool addSurfaceDepth, CaveSurface surface) {
    return stoneDepthCheck(offset, addSurfaceDepth, 0, surface);
}

ConditionSourcePtr stoneDepthCheck(int offset, bool addSurfaceDepth, int secondaryDepthRange,
                                    CaveSurface surface) {
    return std::make_shared<StoneDepthCS>(offset, addSurfaceDepth, secondaryDepthRange, surface);
}

ConditionSourcePtr notCond(ConditionSourcePtr target) {
    return std::make_shared<NotCS>(std::move(target));
}

ConditionSourcePtr yBlockCheck(VerticalAnchor anchor, int surfaceDepthMultiplier) {
    return std::make_shared<YConditionCS>(anchor, surfaceDepthMultiplier, false);
}

ConditionSourcePtr yStartCheck(VerticalAnchor anchor, int surfaceDepthMultiplier) {
    return std::make_shared<YConditionCS>(anchor, surfaceDepthMultiplier, true);
}

ConditionSourcePtr waterBlockCheck(int offset, int surfaceDepthMultiplier) {
    return std::make_shared<WaterCS>(offset, surfaceDepthMultiplier, false);
}

ConditionSourcePtr waterStartCheck(int offset, int surfaceDepthMultiplier) {
    return std::make_shared<WaterCS>(offset, surfaceDepthMultiplier, true);
}

ConditionSourcePtr isBiome(std::vector<std::string> biomeKeys) {
    return std::make_shared<BiomeCS>(std::move(biomeKeys));
}

ConditionSourcePtr noiseCondition(const std::string& noiseKey, double minRange, double maxRange) {
    return std::make_shared<NoiseThresholdCS>(noiseKey, minRange, maxRange);
}

ConditionSourcePtr verticalGradient(std::string randomName, VerticalAnchor trueAtAndBelow,
                                     VerticalAnchor falseAtAndAbove) {
    return std::make_shared<VerticalGradientCS>(std::move(randomName), trueAtAndBelow, falseAtAndAbove);
}

ConditionSourcePtr steepCond()                  { return std::make_shared<SteepCS>(); }
ConditionSourcePtr holeCond()                   { return std::make_shared<HoleCS>(); }
ConditionSourcePtr abovePreliminarySurfaceCond(){ return std::make_shared<AbovePreliminarySurfaceCS>(); }
ConditionSourcePtr temperatureCond()            { return std::make_shared<TemperatureCS>(); }

RuleSourcePtr ifTrue(ConditionSourcePtr cond, RuleSourcePtr thenRun) {
    return std::make_shared<TestRuleSource>(std::move(cond), std::move(thenRun));
}

RuleSourcePtr sequence(std::vector<RuleSourcePtr> rules) {
    assert(!rules.empty());
    return std::make_shared<SequenceRuleSource>(std::move(rules));
}

RuleSourcePtr blockState(uint32_t stateId) {
    return std::make_shared<BlockRuleSource>(stateId);
}

RuleSourcePtr bandlands() {
    return std::make_shared<BandlandsRuleSource>();
}

} // namespace mc::levelgen::SurfaceRules

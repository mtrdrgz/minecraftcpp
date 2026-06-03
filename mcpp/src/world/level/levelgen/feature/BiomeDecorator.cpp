#include "BiomeDecorator.h"

#include "../RandomSource.h"
#include "../IntProvider.h"
#include "../placement/PlacementModifier.h"
#include "../placement/HeightmapPlacement.h"
#include "../placement/NoiseCountPlacement.h"
#include "../placement/PlacedFeature.h"
#include "Feature.h"
#include "TreeGen.h"
#include "stateproviders/BlockStateProvider.h"
#include "stateproviders/NoiseBasedStateProviders.h"
#include "../../block/BlockBehaviour.h"
#include "../../block/BlockState.h"
#include "../../block/BlockStates.h"
#include "../../block/Blocks.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mc::levelgen::feature {

using namespace mc::levelgen::placement;
using SP = stateproviders::SimpleStateProvider;
using WSP = stateproviders::WeightedStateProvider;
using NTP = stateproviders::NoiseThresholdProvider;
using stateproviders::BlockStateProviderPtr;
using mc::valueproviders::ConstantInt;
using mc::valueproviders::UniformInt;
using mc::valueproviders::BiasedToBottomInt;
using mc::valueproviders::ClampedInt;
using mc::valueproviders::TrapezoidInt;
using mc::valueproviders::IntProviderPtr;
using json = nlohmann::json;

namespace {

// ── WorldGenLevel view over a single LevelChunk ──────────────────────────────
// Reads/writes the chunk as canonical block-state id strings; out-of-chunk
// positions read as air and drop on write (single-chunk decoration, no neighbour
// region yet). Carries the biome getter + the feature currently being placed so
// the minecraft:biome filter can check "does the biome at this pos list me".
struct ChunkWGL final : WorldGenLevel {
    LevelChunk* chunk = nullptr;
    const mc::block::BlockTags* tags = nullptr;
    const BiomeFeatures* biomeFeatures = nullptr;
    const std::function<std::string(int, int, int)>* biomeGetter = nullptr;
    int curStep = 0;
    std::string curFeatureKey;

    bool inChunk(BlockPos p) const {
        const ChunkPos cp = chunk->pos();
        return p.x >= cp.x * 16 && p.x < cp.x * 16 + 16 && p.z >= cp.z * 16 && p.z < cp.z * 16 + 16
               && p.y >= CHUNK_MIN_Y && p.y < CHUNK_MAX_Y;
    }

    int getMinY() const override { return CHUNK_MIN_Y; }

    int getHeight(Heightmap::Types, int x, int z) const override {
        const ChunkPos cp = chunk->pos();
        if (x < cp.x * 16 || x >= cp.x * 16 + 16 || z < cp.z * 16 || z >= cp.z * 16 + 16) return CHUNK_MIN_Y;
        const int h = chunk->heightmap(x & 15, z & 15); // top non-air block
        return h < CHUNK_MIN_Y ? CHUNK_MIN_Y : h + 1;   // first free space above
    }

    std::string nameAt(BlockPos p) const {
        if (!inChunk(p)) return "minecraft:air";
        const mc::BlockState* s = mc::getBlockState(chunk->getBlock(p.x, p.y, p.z));
        return (s && s->block) ? ("minecraft:" + s->block->name) : "minecraft:air";
    }

    std::string getBlockState(BlockPos p) const override { return nameAt(p); }
    bool isEmptyBlock(BlockPos p) const override { return nameAt(p) == "minecraft:air"; }

    bool canSurvive(const std::string& state, BlockPos p) const override {
        return mc::block::canSurvive(state, nameAt(BlockPos{ p.x, p.y - 1, p.z }), *tags);
    }

    void setBlock(BlockPos p, const std::string& state, int) override {
        if (!inChunk(p)) return; // clamp to this chunk
        // canonical state -> bare block name (registry keys carry no namespace).
        std::string name = mc::block::blockName(state);
        if (auto c = name.find(':'); c != std::string::npos) name = name.substr(c + 1);
        const uint32_t id = getDefaultBlockStateId(name, 0);
        if (id != 0) chunk->setBlock(p.x, p.y, p.z, id);
    }
};

std::string stripNs(std::string s) {
    if (auto c = s.find(':'); c != std::string::npos) s = s.substr(c + 1);
    return s;
}

// minecraft:biome filter: keep the position iff the biome there lists the feature
// currently being placed (exactly Java's per-biome gating).
bool biomeAllows(WorldGenLevel& level, BlockPos p) {
    auto& w = static_cast<ChunkWGL&>(level);
    const std::string biome = (*w.biomeGetter)(p.x, p.y, p.z);
    return w.biomeFeatures->biomeHasFeature(biome, w.curStep, w.curFeatureKey);
}

// minecraft:count with a weighted_list IntProvider (e.g. trees: {10:w9},{11:w1}).
class WeightedCountPlacement final : public PlacementModifier {
public:
    explicit WeightedCountPlacement(std::vector<std::pair<int, int>> dist) : m_dist(std::move(dist)) {
        for (auto& d : m_dist) m_total += d.second;
    }
    std::vector<BlockPos> getPositions(PlacementContext*, RandomSource& r, BlockPos o) const override {
        int n = m_dist.empty() ? 0 : m_dist.back().first;
        if (m_total > 0) {
            int x = r.nextInt(m_total), cum = 0;
            for (auto& d : m_dist) { cum += d.second; if (x < cum) { n = d.first; break; } }
        }
        return std::vector<BlockPos>(n < 0 ? 0 : static_cast<std::size_t>(n), o);
    }
private:
    std::vector<std::pair<int, int>> m_dist;
    int m_total = 0;
};

// minecraft:surface_water_depth_filter — keep origin iff water depth <= max
// (0 on dry land with the single-heightmap chunk view).
class SurfaceWaterDepthFilter final : public PlacementModifier {
public:
    explicit SurfaceWaterDepthFilter(int maxDepth) : m_max(maxDepth) {}
    std::vector<BlockPos> getPositions(PlacementContext* ctx, RandomSource&, BlockPos o) const override {
        WorldGenLevel* l = ctx->getLevel();
        const int depth = l->getHeight(Heightmap::Types::WORLD_SURFACE_WG, o.x, o.z)
                          - l->getHeight(Heightmap::Types::OCEAN_FLOOR_WG, o.x, o.z);
        return depth <= m_max ? std::vector<BlockPos>{ o } : std::vector<BlockPos>{};
    }
private:
    int m_max;
};

// ── A SimpleBlock feature placer for a state provider ────────────────────────
PlacedFeature::FeaturePlacer simpleBlockPlacer(BlockStateProviderPtr provider) {
    return [cfg = std::make_shared<SimpleBlockConfiguration>(SimpleBlockConfiguration{ std::move(provider), false })](
               WorldGenLevel& l, RandomSource& r, BlockPos p) {
        SimpleBlockFeature f;
        FeaturePlaceContext<SimpleBlockConfiguration> c{ &l, &r, p, cfg.get() };
        return f.place(c);
    };
}

// A tree feature placer placing one tree of `cfg` at the position (TreeFeature port).
PlacedFeature::FeaturePlacer treePlacer(TreeConfig cfg) {
    return [cfg = std::move(cfg)](WorldGenLevel& l, RandomSource& r, BlockPos p) -> bool {
        auto& w = static_cast<ChunkWGL&>(l);
        const ChunkPos cp = w.chunk->pos();
        TreeWorld tw{ *w.chunk, cp.x * 16, cp.z * 16 };
        return placeTree(tw, r, p.x, p.y, p.z, cfg);
    };
}

// ── JSON parsing (data/minecraft/worldgen) ───────────────────────────────────

std::optional<json> loadJsonFile(const std::string& dir, const char* sub, std::string name) {
    name = stripNs(std::move(name));
    std::ifstream in(dir + "/" + sub + "/" + name + ".json");
    if (!in) return std::nullopt;
    std::stringstream ss; ss << in.rdbuf();
    try { return json::parse(ss.str()); } catch (...) { return std::nullopt; }
}

std::string stateName(const json& j) {
    if (j.is_string()) return j.get<std::string>();
    if (j.is_object()) return j.value("Name", std::string("minecraft:air"));
    return "minecraft:air";
}

std::string typeOf(const json& j) { return stripNs(j.value("type", std::string())); }

// First state name of a BlockStateProvider json (for tree trunk/foliage blocks).
std::string providerState(const json& j) {
    if (!j.is_object()) return "minecraft:air";
    if (j.contains("state")) return stateName(j["state"]);
    if (j.contains("entries") && !j["entries"].empty()) return stateName(j["entries"][0]["data"]);
    return "minecraft:air";
}

BlockStateProviderPtr parseProvider(const json& j) {
    if (!j.is_object()) return SP::of("minecraft:air");
    const std::string t = typeOf(j);
    if (t == "simple_state_provider" || t == "rule_based_state_provider")
        return SP::of(stateName(j.contains("state") ? j["state"] : json("minecraft:dirt")));
    if (t == "weighted_state_provider") {
        std::vector<WSP::Entry> es;
        for (const auto& e : j.value("entries", json::array()))
            es.push_back({ stateName(e["data"]), e.value("weight", 1) });
        return es.empty() ? SP::of("minecraft:air") : std::make_shared<WSP>(std::move(es));
    }
    if (t == "noise_threshold_provider") {
        const json& n = j["noise"];
        std::vector<double> amps = n.value("amplitudes", std::vector<double>{ 1.0 });
        std::vector<std::string> low, high;
        for (const auto& s : j.value("low_states", json::array())) low.push_back(stateName(s));
        for (const auto& s : j.value("high_states", json::array())) high.push_back(stateName(s));
        return std::make_shared<NTP>((int64_t)j.value("seed", 0), NoiseParameters{ n.value("firstOctave", 0), amps },
                                     (float)j.value("scale", 1.0), (float)j.value("threshold", 0.0),
                                     (float)j.value("high_chance", 0.0), stateName(j["default_state"]), low, high);
    }
    if (t == "noise_provider" || t == "dual_noise_provider") {
        // approximate the noise-indexed palette by an equal-weight pick of its states
        std::vector<WSP::Entry> es;
        for (const auto& s : j.value("states", json::array())) es.push_back({ stateName(s), 1 });
        return es.empty() ? SP::of("minecraft:air") : std::make_shared<WSP>(std::move(es));
    }
    if (t == "randomized_int_state_provider" && j.contains("source")) return parseProvider(j["source"]);
    return SP::of("minecraft:air");
}

IntProviderPtr parseIntProvider(const json& j) {
    if (j.is_number_integer()) return ConstantInt::of(j.get<int>());
    if (!j.is_object()) return ConstantInt::of(0);
    const std::string t = typeOf(j);
    if (t == "constant") return ConstantInt::of(j.value("value", 0));
    if (t == "uniform") return UniformInt::of(j.value("min_inclusive", 0), j.value("max_inclusive", 0));
    if (t == "biased_to_bottom") return BiasedToBottomInt::of(j.value("min_inclusive", 0), j.value("max_inclusive", 0));
    if (t == "clamped") return ClampedInt::of(parseIntProvider(j["source"]), j.value("min_inclusive", 0), j.value("max_inclusive", 0));
    if (t == "trapezoid") return TrapezoidInt::of(j.value("min", 0), j.value("max", 0), j.value("plateau", 0));
    return ConstantInt::of(0);
}

IntVal parseIntVal(const json& j) {
    if (j.is_number_integer()) return IntVal::constant(j.get<int>());
    if (j.is_object()) {
        const std::string t = typeOf(j);
        if (t == "uniform") return IntVal::uniform(j.value("min_inclusive", 0), j.value("max_inclusive", 0));
        if (t == "constant") return IntVal::constant(j.value("value", 0));
    }
    return IntVal::constant(0);
}

Heightmap::Types hmType(const std::string& s) {
    if (s == "WORLD_SURFACE_WG") return Heightmap::Types::WORLD_SURFACE_WG;
    if (s == "WORLD_SURFACE") return Heightmap::Types::WORLD_SURFACE;
    if (s == "OCEAN_FLOOR_WG") return Heightmap::Types::OCEAN_FLOOR_WG;
    if (s == "OCEAN_FLOOR") return Heightmap::Types::OCEAN_FLOOR;
    if (s == "MOTION_BLOCKING_NO_LEAVES") return Heightmap::Types::MOTION_BLOCKING_NO_LEAVES;
    return Heightmap::Types::MOTION_BLOCKING;
}

BlockPredicateFilter::Predicate parsePredicate(const json& p) {
    const std::string t = typeOf(p);
    if (t == "matching_block_tag") {
        std::string tag = p.value("tag", std::string("minecraft:air"));
        return [tag](WorldGenLevel& l, BlockPos pos) {
            return static_cast<ChunkWGL&>(l).tags->isInTag(l.getBlockState(pos), tag);
        };
    }
    if (t == "would_survive") {
        std::string st = stateName(p.contains("state") ? p["state"] : json("minecraft:air"));
        return [st](WorldGenLevel& l, BlockPos pos) { return l.canSurvive(st, pos); };
    }
    if (t == "all_of") {
        std::vector<BlockPredicateFilter::Predicate> subs;
        for (const auto& c : p.value("predicates", json::array())) subs.push_back(parsePredicate(c));
        return [subs](WorldGenLevel& l, BlockPos pos) {
            for (auto& s : subs) if (!s(l, pos)) return false;
            return true;
        };
    }
    if (t == "any_of") {
        std::vector<BlockPredicateFilter::Predicate> subs;
        for (const auto& c : p.value("predicates", json::array())) subs.push_back(parsePredicate(c));
        return [subs](WorldGenLevel& l, BlockPos pos) {
            for (auto& s : subs) if (s(l, pos)) return true;
            return subs.empty();
        };
    }
    if (t == "not" && p.contains("predicate")) {
        auto sub = parsePredicate(p["predicate"]);
        return [sub](WorldGenLevel& l, BlockPos pos) { return !sub(l, pos); };
    }
    return [](WorldGenLevel&, BlockPos) { return true; }; // true / matching_blocks / unknown
}

std::shared_ptr<const PlacementModifier> parseModifier(const json& m) {
    const std::string t = typeOf(m);
    if (t == "in_square") return std::make_shared<InSquarePlacement>();
    if (t == "biome") return std::make_shared<BiomeFilter>(biomeAllows);
    if (t == "rarity_filter") return std::make_shared<RarityFilter>(m.value("chance", 1));
    if (t == "heightmap") return std::make_shared<HeightmapPlacement>(hmType(m.value("heightmap", std::string("MOTION_BLOCKING"))));
    if (t == "surface_water_depth_filter") return std::make_shared<SurfaceWaterDepthFilter>(m.value("max_water_depth", 0));
    if (t == "block_predicate_filter") return std::make_shared<BlockPredicateFilter>(parsePredicate(m["predicate"]));
    if (t == "random_offset") return std::make_shared<RandomOffsetPlacement>(parseIntProvider(m["xz_spread"]), parseIntProvider(m["y_spread"]));
    if (t == "noise_threshold_count")
        return std::make_shared<NoiseThresholdCountPlacement>(m.value("noise_level", 0.0), m.value("below_noise", 0), m.value("above_noise", 0));
    if (t == "count") {
        const json& c = m["count"];
        if (c.is_object() && typeOf(c) == "weighted_list") {
            std::vector<std::pair<int, int>> dist;
            for (const auto& e : c.value("distribution", json::array())) dist.emplace_back(e.value("data", 0), e.value("weight", 1));
            return std::make_shared<WeightedCountPlacement>(std::move(dist));
        }
        return std::make_shared<CountPlacement>(parseIntProvider(c));
    }
    return nullptr; // height_range / environment_scan / count_on_every_layer / unknown -> identity
}

std::optional<TreeConfig> parseTreeConfig(const json& c) {
    if (!c.contains("trunk_placer") || !c.contains("foliage_placer")) return std::nullopt;
    const json& tp = c["trunk_placer"];
    const json& fp = c["foliage_placer"];
    const std::string tt = typeOf(tp), ft = typeOf(fp);
    const int bh = tp.value("base_height", 4), ra = tp.value("height_rand_a", 0), rb = tp.value("height_rand_b", 0);

    std::shared_ptr<TrunkPlacer> trunk;
    if (tt == "straight_trunk_placer") trunk = std::make_shared<StraightTrunkPlacer>(bh, ra, rb);
    else if (tt == "forking_trunk_placer") trunk = std::make_shared<ForkingTrunkPlacer>(bh, ra, rb);
    else if (tt == "fancy_trunk_placer") trunk = std::make_shared<FancyTrunkPlacer>(bh, ra, rb);
    else return std::nullopt; // dark_oak / giant / mega / mangrove / cherry / bending / pale — not ported

    const IntVal rad = parseIntVal(fp.value("radius", json(0))), off = parseIntVal(fp.value("offset", json(0)));
    std::shared_ptr<FoliagePlacer> foliage;
    if (ft == "blob_foliage_placer" || ft == "bush_foliage_placer") foliage = std::make_shared<BlobFoliagePlacer>(rad, off, fp.value("height", 3));
    else if (ft == "fancy_foliage_placer") foliage = std::make_shared<FancyFoliagePlacer>(rad, off, fp.value("height", 4));
    else if (ft == "spruce_foliage_placer") foliage = std::make_shared<SpruceFoliagePlacer>(rad, off, parseIntVal(fp["trunk_height"]));
    else if (ft == "pine_foliage_placer") foliage = std::make_shared<PineFoliagePlacer>(rad, off, parseIntVal(fp["height"]));
    else if (ft == "acacia_foliage_placer") foliage = std::make_shared<AcaciaFoliagePlacer>(rad, off);
    else return std::nullopt;

    const json& ms = c.value("minimum_size", json::object());
    const int minClip = ms.value("min_clipped_height", -1);
    auto size = std::make_shared<TwoLayersFeatureSize>(ms.value("limit", 1), ms.value("lower_size", 0), ms.value("upper_size", 1),
                                                       minClip >= 0 ? std::optional<int>(minClip) : std::nullopt);

    const uint32_t logId = getDefaultBlockStateId(stripNs(providerState(c.value("trunk_provider", json::object()))), 0);
    const uint32_t leafId = getDefaultBlockStateId(stripNs(providerState(c.value("foliage_provider", json::object()))), 0);
    const uint32_t dirtId = getDefaultBlockStateId("dirt", 0);
    if (logId == 0 || leafId == 0) return std::nullopt; // block not in registry -> skip
    return TreeConfig{ logId, logId, logId, leafId, dirtId, trunk, foliage, size, true };
}

struct Resolved { PlacedFeature::FeaturePlacer placer; bool ok = false; };
Resolved noop() { return { [](WorldGenLevel&, RandomSource&, BlockPos) { return false; }, false }; }

// Unwrap a feature reference: a string name, or {feature: <ref>, placement:[...]}.
std::string featureRef(const json& j) {
    if (j.is_string()) return j.get<std::string>();
    if (j.is_object() && j.contains("feature")) return featureRef(j["feature"]);
    return std::string();
}

Resolved resolveByName(const std::string& dir, const std::string& name); // fwd

Resolved resolveConfigured(const std::string& dir, const json& cf) {
    const std::string t = typeOf(cf);
    const json& cfg = cf.value("config", json::object());
    if (t == "simple_block") return { simpleBlockPlacer(parseProvider(cfg["to_place"])), true };
    if (t == "tree") {
        auto tc = parseTreeConfig(cfg);
        return tc ? Resolved{ treePlacer(*tc), true } : noop();
    }
    if (t == "random_selector") {
        struct E { float chance; Resolved r; };
        auto es = std::make_shared<std::vector<E>>();
        for (const auto& e : cfg.value("features", json::array()))
            es->push_back({ (float)e.value("chance", 0.0), resolveByName(dir, featureRef(e["feature"])) });
        auto def = std::make_shared<Resolved>(resolveByName(dir, featureRef(cfg["default"])));
        return { [es, def](WorldGenLevel& l, RandomSource& r, BlockPos p) -> bool {
            for (auto& e : *es) if (r.nextFloat() < e.chance) return e.r.ok && e.r.placer(l, r, p);
            return def->ok && def->placer(l, r, p);
        }, true };
    }
    if (t == "simple_random_selector") {
        auto fs = std::make_shared<std::vector<Resolved>>();
        for (const auto& f : cfg.value("features", json::array())) fs->push_back(resolveByName(dir, featureRef(f)));
        return { [fs](WorldGenLevel& l, RandomSource& r, BlockPos p) -> bool {
            if (fs->empty()) return false;
            const Resolved& e = (*fs)[r.nextInt((int)fs->size())];
            return e.ok && e.placer(l, r, p);
        }, true };
    }
    if (t == "random_boolean_selector") {
        auto tt = std::make_shared<Resolved>(resolveByName(dir, featureRef(cfg["feature_true"])));
        auto ff = std::make_shared<Resolved>(resolveByName(dir, featureRef(cfg["feature_false"])));
        return { [tt, ff](WorldGenLevel& l, RandomSource& r, BlockPos p) -> bool {
            const Resolved& e = r.nextBoolean() ? *tt : *ff;
            return e.ok && e.placer(l, r, p);
        }, true };
    }
    return noop(); // block_column / bamboo / huge_fungus / vegetation_patch / coral / ... not yet ported
}

Resolved resolveByName(const std::string& dir, const std::string& name) {
    static std::unordered_map<std::string, Resolved> cache;
    if (name.empty()) return noop();
    if (auto it = cache.find(name); it != cache.end()) return it->second;
    cache[name] = noop(); // placeholder breaks any reference cycle
    Resolved res;
    if (auto cf = loadJsonFile(dir, "configured_feature", name)) res = resolveConfigured(dir, *cf);
    else if (auto pf = loadJsonFile(dir, "placed_feature", name)) res = resolveByName(dir, featureRef((*pf)["feature"]));
    else res = noop();
    cache[name] = res;
    return res;
}

// Build a PlacedFeature for a step-9 key by reading its placed_feature JSON; the
// inner configured feature is resolved by resolveByName. Returns nullptr when the
// feature type isn't ported yet (it still consumes its decoration seed index).
std::shared_ptr<PlacedFeature> buildPlaced(const std::string& dir, const std::string& key) {
    auto pf = loadJsonFile(dir, "placed_feature", key);
    if (!pf) return nullptr;
    Resolved res = resolveByName(dir, featureRef((*pf)["feature"]));
    if (!res.ok) return nullptr;
    std::vector<std::shared_ptr<const PlacementModifier>> mods;
    for (const auto& m : pf->value("placement", json::array()))
        if (auto mm = parseModifier(m)) mods.push_back(std::move(mm));
    return std::make_shared<PlacedFeature>(res.placer, std::move(mods));
}

std::shared_ptr<PlacedFeature> featureFor(const std::string& dir, const std::string& key) {
    static std::unordered_map<std::string, std::shared_ptr<PlacedFeature>> cache;
    if (auto it = cache.find(key); it != cache.end()) return it->second;
    auto pf = buildPlaced(dir, key);
    cache[key] = pf;
    return pf;
}

} // namespace

void applyBiomeDecoration(LevelChunk& chunk, std::int64_t worldSeed,
                          const std::function<std::string(int, int, int)>& biomeGetter,
                          const BiomeFeatures& biomeFeatures,
                          const mc::block::BlockTags& tags,
                          const std::string& worldgenDir) {
    const ChunkPos cp = chunk.pos();
    const int minX = cp.x * 16, minZ = cp.z * 16;

    ChunkWGL level;
    level.chunk = &chunk;
    level.tags = &tags;
    level.biomeFeatures = &biomeFeatures;
    level.biomeGetter = &biomeGetter;

    // Population RNG, seeded exactly like ChunkGenerator.applyBiomeDecoration.
    WorldgenRandom random(std::make_shared<XoroshiroRandomSource>((int64_t)0));
    const long long decorationSeed = random.setDecorationSeed(worldSeed, minX, minZ);

    // Biomes present in this chunk (first-encounter order), used to assemble each
    // step's merged feature list (a stand-in for Java's cross-biome FeatureSorter;
    // exact for single-biome chunks, which is the common case).
    std::vector<std::string> present;
    std::unordered_set<std::string> seen;
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) {
            const int y = chunk.heightmap(x, z);
            if (y < CHUNK_MIN_Y) continue;
            std::string b = biomeGetter(minX + x, y, minZ + z);
            if (seen.insert(b).second && biomeFeatures.hasBiome(b)) present.push_back(std::move(b));
        }

    const BlockPos origin{ minX, 0, minZ };

    for (int step = 0; step < GenerationStep::COUNT; ++step) {
        // Merge the per-biome ordered feature lists for this step (dedup, keep order).
        std::vector<std::string> merged;
        std::unordered_set<std::string> mergedSeen;
        for (const std::string& b : present)
            for (const std::string& key : biomeFeatures.featuresForStep(b, step))
                if (mergedSeen.insert(key).second) merged.push_back(key);

        for (std::size_t index = 0; index < merged.size(); ++index) {
            // Every feature consumes its seed index, so ported features line up
            // with vanilla even when neighbours in the list aren't ported yet.
            random.setFeatureSeed(decorationSeed, static_cast<int>(index), step);
            std::shared_ptr<PlacedFeature> pf = featureFor(worldgenDir, merged[index]);
            if (!pf) continue; // feature type not ported yet
            level.curStep = step;
            level.curFeatureKey = merged[index];
            pf->place(level, random, origin, CHUNK_MIN_Y, CHUNK_MAX_Y - CHUNK_MIN_Y);
        }
    }
}

} // namespace mc::levelgen::feature

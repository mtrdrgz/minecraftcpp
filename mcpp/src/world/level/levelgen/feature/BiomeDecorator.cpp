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

#include <algorithm>
#include <cmath>
#include <cstdlib>
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
        const uint32_t id = getBlockStateId(state, 0);
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
    if (j.is_object()) {
        std::string name = j.value("Name", std::string("minecraft:air"));
        const auto propsIt = j.find("Properties");
        if (propsIt == j.end() || !propsIt->is_object() || propsIt->empty()) {
            return name;
        }
        std::map<std::string, std::string> props;
        for (const auto& [key, value] : propsIt->items()) {
            props[key] = value.is_string() ? value.get<std::string>() : value.dump();
        }
        return mc::block::serializeState(name, props);
    }
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
        if (t == "uniform" || (t.empty() && j.contains("min_inclusive")))
            return IntVal::uniform(j.value("min_inclusive", 0), j.value("max_inclusive", 0));
        if (t == "constant") return IntVal::constant(j.value("value", 0));
        if (t == "weighted_list") { // approximate by a uniform over the value range
            int lo = 1 << 30, hi = -(1 << 30);
            for (const auto& e : j.value("distribution", json::array()))
                if (e["data"].is_number_integer()) { const int d = e["data"].get<int>(); lo = std::min(lo, d); hi = std::max(hi, d); }
            return hi >= lo ? IntVal::uniform(lo, hi) : IntVal::constant(0);
        }
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
    else if (tt == "dark_oak_trunk_placer") trunk = std::make_shared<DarkOakTrunkPlacer>(bh, ra, rb);
    else if (tt == "giant_trunk_placer") trunk = std::make_shared<GiantTrunkPlacer>(bh, ra, rb);
    else if (tt == "mega_jungle_trunk_placer") trunk = std::make_shared<MegaJungleTrunkPlacer>(bh, ra, rb);
    else if (tt == "cherry_trunk_placer") trunk = std::make_shared<CherryTrunkPlacer>(bh, ra, rb,
        parseIntVal(tp.value("branch_count", json(1))), parseIntVal(tp.value("branch_horizontal_length", json(2))),
        parseIntVal(tp.value("branch_start_offset_from_top", json(-3))), parseIntVal(tp.value("branch_end_offset_from_top", json(0))));
    else if (tt == "upwards_branching_trunk_placer") trunk = std::make_shared<UpwardsBranchingTrunkPlacer>(bh, ra, rb,
        parseIntVal(tp.value("extra_branch_steps", json(1))), parseIntVal(tp.value("extra_branch_length", json(0))),
        tp.value("place_branch_per_log_probability", 0.5));
    else return std::nullopt; // bending / pale-specific placers — not ported

    const IntVal rad = parseIntVal(fp.value("radius", json(0))), off = parseIntVal(fp.value("offset", json(0)));
    std::shared_ptr<FoliagePlacer> foliage;
    if (ft == "blob_foliage_placer" || ft == "bush_foliage_placer") foliage = std::make_shared<BlobFoliagePlacer>(rad, off, fp.value("height", 3));
    else if (ft == "fancy_foliage_placer") foliage = std::make_shared<FancyFoliagePlacer>(rad, off, fp.value("height", 4));
    else if (ft == "spruce_foliage_placer") foliage = std::make_shared<SpruceFoliagePlacer>(rad, off, parseIntVal(fp["trunk_height"]));
    else if (ft == "pine_foliage_placer") foliage = std::make_shared<PineFoliagePlacer>(rad, off, parseIntVal(fp["height"]));
    else if (ft == "acacia_foliage_placer") foliage = std::make_shared<AcaciaFoliagePlacer>(rad, off);
    else if (ft == "dark_oak_foliage_placer") foliage = std::make_shared<DarkOakFoliagePlacer>(rad, off);
    else if (ft == "mega_pine_foliage_placer") foliage = std::make_shared<MegaPineFoliagePlacer>(rad, off, parseIntVal(fp["crown_height"]));
    else if (ft == "jungle_foliage_placer") foliage = std::make_shared<JungleFoliagePlacer>(rad, off, fp.value("height", 2));
    else if (ft == "cherry_foliage_placer") foliage = std::make_shared<CherryFoliagePlacer>(rad, off, fp.value("height", 5));
    else if (ft == "random_spread_foliage_placer") foliage = std::make_shared<RandomSpreadFoliagePlacer>(rad, off, fp.value("foliage_height", 2), fp.value("leaf_placement_attempts", 70));
    else return std::nullopt;

    const json& ms = c.value("minimum_size", json::object());
    const int minClip = ms.value("min_clipped_height", -1);
    auto size = std::make_shared<TwoLayersFeatureSize>(ms.value("limit", 1), ms.value("lower_size", 0), ms.value("upper_size", 1),
                                                       minClip >= 0 ? std::optional<int>(minClip) : std::nullopt);

    const std::string trunkState = providerState(c.value("trunk_provider", json::object()));
    const std::string logName = stripNs(mc::block::blockName(trunkState));
    const uint32_t logId = getBlockStateId(trunkState, 0);
    const uint32_t leafId = getBlockStateId(providerState(c.value("foliage_provider", json::object())), 0);
    const uint32_t dirtId = getDefaultBlockStateId("dirt", 0);
    if (logId == 0 || leafId == 0) return std::nullopt; // block not in registry -> skip
    const uint32_t logX = getBlockStateId("minecraft:" + logName + "[axis=x]", logId);
    const uint32_t logY = getBlockStateId("minecraft:" + logName + "[axis=y]", logId);
    const uint32_t logZ = getBlockStateId("minecraft:" + logName + "[axis=z]", logId);
    return TreeConfig{ logY, logX, logZ, leafId, dirtId, trunk, foliage, size, true };
}

// ── Non-tree feature placers (one universal function per configured-feature type,
//    each invoked at the placement-positioned origin via the WorldGenLevel) ──────

// block_column: cactus / sugar_cane / cave_vine — a vertical run per layer.
PlacedFeature::FeaturePlacer blockColumnPlacer(const json& c) {
    const int dy = c.value("direction", std::string("up")) == "down" ? -1 : 1;
    auto pred = parsePredicate(c.value("allowed_placement", json::object()));
    struct Layer { IntProviderPtr h; BlockStateProviderPtr p; };
    auto layers = std::make_shared<std::vector<Layer>>();
    for (const auto& l : c.value("layers", json::array())) layers->push_back({ parseIntProvider(l["height"]), parseProvider(l["provider"]) });
    return [dy, pred, layers](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        BlockPos cur = pos; bool placed = false;
        for (auto& L : *layers) {
            const int h = L.h->sample(r);
            for (int i = 0; i < h; ++i) {
                if (!pred(lv, cur)) return placed;
                lv.setBlock(cur, L.p->getState(r, cur), 2);
                placed = true; cur.y += dy;
            }
        }
        return placed;
    };
}

// huge_red_mushroom / huge_brown_mushroom: a stem column topped by a cap.
PlacedFeature::FeaturePlacer hugeMushroomPlacer(const json& c, bool red) {
    auto stem = parseProvider(c["stem_provider"]);
    auto cap = parseProvider(c["cap_provider"]);
    const int radius = c.value("foliage_radius", red ? 2 : 3);
    return [stem, cap, radius, red](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        if (!lv.isEmptyBlock(pos)) return false;
        const int height = 4 + r.nextInt(3) + (red ? r.nextInt(2) : 0);
        for (int y = 0; y < height; ++y) { BlockPos s{ pos.x, pos.y + y, pos.z }; lv.setBlock(s, stem->getState(r, s), 2); }
        if (red) { // rounded dome over the top layers
            for (int yy = height - 3; yy <= height; ++yy) {
                const int rad = std::max(0, yy < height ? radius : radius - 1);
                for (int dx = -rad; dx <= rad; ++dx)
                    for (int dz = -rad; dz <= rad; ++dz) {
                        const bool ex = dx == -rad || dx == rad, ez = dz == -rad || dz == rad;
                        if (yy == height || ex || ez) { BlockPos cp{ pos.x + dx, pos.y + yy, pos.z + dz }; lv.setBlock(cp, cap->getState(r, cp), 2); }
                    }
            }
        } else { // flat square cap with cut corners
            const int yy = pos.y + height;
            for (int dx = -radius; dx <= radius; ++dx)
                for (int dz = -radius; dz <= radius; ++dz) {
                    if (std::abs(dx) == radius && std::abs(dz) == radius) continue;
                    BlockPos cp{ pos.x + dx, yy, pos.z + dz }; lv.setBlock(cp, cap->getState(r, cp), 2);
                }
        }
        return true;
    };
}

// nether_forest_vegetation: scatter roots/fungi over a spread area on the floor.
PlacedFeature::FeaturePlacer netherVegPlacer(const json& c) {
    const int w = c.value("spread_width", 8), h = c.value("spread_height", 4);
    auto prov = parseProvider(c["state_provider"]);
    return [w, h, prov](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        bool placed = false;
        for (int i = 0; i < w * w; ++i) {
            BlockPos p{ pos.x + r.nextInt(w) - r.nextInt(w), pos.y + r.nextInt(h) - r.nextInt(h), pos.z + r.nextInt(w) - r.nextInt(w) };
            if (lv.isEmptyBlock(p) && !lv.isEmptyBlock(BlockPos{ p.x, p.y - 1, p.z })) { lv.setBlock(p, prov->getState(r, p), 2); placed = true; }
        }
        return placed;
    };
}

// Simple underwater placers (barely visible in a dry vista, but functionally present).
PlacedFeature::FeaturePlacer seagrassPlacer(const json& c) {
    const double prob = c.value("probability", 0.0);
    return [prob](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        if (!lv.isEmptyBlock(pos)) return false;
        const bool tall = r.nextDouble() < prob;
        lv.setBlock(pos, "minecraft:seagrass", 2);
        if (tall) lv.setBlock(BlockPos{ pos.x, pos.y + 1, pos.z }, "minecraft:tall_seagrass", 2);
        return true;
    };
}
PlacedFeature::FeaturePlacer seaPicklePlacer(const json& c) {
    auto count = parseIntProvider(c.value("count", json(20)));
    return [count](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        const int n = count->sample(r); bool placed = false;
        for (int i = 0; i < n; ++i) {
            BlockPos p{ pos.x + r.nextInt(8) - r.nextInt(8), pos.y, pos.z + r.nextInt(8) - r.nextInt(8) };
            if (lv.isEmptyBlock(p) && !lv.isEmptyBlock(BlockPos{ p.x, p.y - 1, p.z })) { lv.setBlock(p, "minecraft:sea_pickle", 2); placed = true; }
        }
        return placed;
    };
}
PlacedFeature::FeaturePlacer kelpPlacer() {
    return [](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        const int h = 1 + r.nextInt(10);
        for (int y = 0; y < h; ++y) { BlockPos p{ pos.x, pos.y + y, pos.z }; if (!lv.isEmptyBlock(p)) return y > 0; lv.setBlock(p, y == h - 1 ? "minecraft:kelp" : "minecraft:kelp_plant", 2); }
        return true;
    };
}

// huge_fungus (crimson / warped): stem column topped by a wart-block hat.
PlacedFeature::FeaturePlacer hugeFungusPlacer(const json& c) {
    auto hat = parseProvider(c["hat_state"]);
    const std::string stem = stateName(c["hat_state"]).find("warped") != std::string::npos ? "minecraft:warped_stem" : "minecraft:crimson_stem";
    return [hat, stem](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        if (!lv.isEmptyBlock(pos)) return false;
        const int h = 5 + r.nextInt(9);
        for (int y = 0; y < h; ++y) lv.setBlock(BlockPos{ pos.x, pos.y + y, pos.z }, stem, 2);
        const int top = pos.y + h;
        for (int yy = top - 2; yy <= top; ++yy) {
            const int rad = yy == top ? 1 : 2;
            for (int dx = -rad; dx <= rad; ++dx)
                for (int dz = -rad; dz <= rad; ++dz)
                    if (!(std::abs(dx) == rad && std::abs(dz) == rad)) lv.setBlock(BlockPos{ pos.x + dx, yy, pos.z + dz }, hat->getState(r, BlockPos{}), 2);
        }
        return true;
    };
}

// fallen_tree: a horizontal log resting on the ground, plus a one-block stump.
PlacedFeature::FeaturePlacer fallenTreePlacer(const json& c) {
    auto trunk = parseProvider(c["trunk_provider"]);
    auto len = parseIntProvider(c.value("log_length", json(5)));
    return [trunk, len](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        static const int D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        const int d = r.nextInt(4);
        const int logLength = std::max(len->sample(r) - 2, 0);
        const std::string axis = D[d][0] != 0 ? "x" : "z";

        auto validTreePos = [&lv](BlockPos p) {
            if (auto* w = dynamic_cast<ChunkWGL*>(&lv); w && !w->inChunk(p)) {
                return false;
            }
            return lv.isEmptyBlock(p);
        };
        auto isOverSolidGround = [&lv](BlockPos p) {
            const std::string belowState = lv.getBlockState(BlockPos{ p.x, p.y - 1, p.z });
            std::string belowName = stripNs(mc::block::blockName(belowState));
            const mc::Block* below = getBlockByName(belowName);
            return below && below->isSolid();
        };
        auto mayPlaceOn = [&](BlockPos p) {
            return validTreePos(p) && isOverSolidGround(p);
        };

        lv.setBlock(pos, trunk->getState(r, pos), 2);
        const int distanceFromStump = 2 + r.nextInt(2);
        BlockPos start{ pos.x + D[d][0] * distanceFromStump, pos.y + 1, pos.z + D[d][1] * distanceFromStump };
        for (int i = 0; i < 6 && !mayPlaceOn(start); ++i) {
            --start.y;
        }

        BlockPos cur = start;
        int groundGap = 0;
        for (int i = 0; i < logLength; ++i) {
            if (!validTreePos(cur)) {
                return true;
            }
            if (!isOverSolidGround(cur)) {
                if (++groundGap > 2) {
                    return true;
                }
            } else {
                groundGap = 0;
            }
            cur.x += D[d][0];
            cur.z += D[d][1];
        }

        cur = start;
        for (int i = 0; i < logLength; ++i) {
            lv.setBlock(cur, mc::block::setProperty(trunk->getState(r, cur), "axis", axis), 2);
            cur.x += D[d][0];
            cur.z += D[d][1];
        }
        return true;
    };
}

// twisting_vines (up) / weeping_vines (down): scattered vine columns (nether).
PlacedFeature::FeaturePlacer vineColumnPlacer(const json& c, int dy, const char* tip, const char* body) {
    const int maxH = c.value("max_height", 8), w = c.value("spread_width", 8), sh = c.value("spread_height", 4);
    return [maxH, w, sh, dy, tip, body](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        bool placed = false;
        for (int i = 0; i < w * w / 4; ++i) {
            BlockPos p{ pos.x + r.nextInt(w) - r.nextInt(w), pos.y + r.nextInt(sh) - r.nextInt(sh), pos.z + r.nextInt(w) - r.nextInt(w) };
            const int h = 1 + r.nextInt(maxH);
            for (int k = 0; k < h; ++k) { BlockPos q{ p.x, p.y + dy * k, p.z }; if (!lv.isEmptyBlock(q)) break; lv.setBlock(q, k == h - 1 ? tip : body, 2); placed = true; }
        }
        return placed;
    };
}

// chorus_plant (end): a short stalk capped by a chorus flower.
PlacedFeature::FeaturePlacer chorusPlacer() {
    return [](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        if (!lv.isEmptyBlock(pos)) return false;
        const int h = 1 + r.nextInt(4); BlockPos c = pos;
        for (int k = 0; k < h; ++k) { if (!lv.isEmptyBlock(c)) break; lv.setBlock(c, "minecraft:chorus_plant", 2); c.y++; }
        lv.setBlock(c, "minecraft:chorus_flower", 2);
        return true;
    };
}

// sculk_patch (deep dark): scatter sculk over the floor.
PlacedFeature::FeaturePlacer sculkPatchPlacer(const json& c) {
    const int attempts = c.value("spread_attempts", 64);
    return [attempts](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        bool placed = false;
        for (int i = 0; i < attempts; ++i) {
            BlockPos g{ pos.x + r.nextInt(9) - r.nextInt(9), pos.y - 1, pos.z + r.nextInt(9) - r.nextInt(9) };
            if (!lv.isEmptyBlock(g)) { lv.setBlock(g, "minecraft:sculk", 2); placed = true; }
        }
        return placed;
    };
}

// vines (jungle): coat air faces adjacent to solid blocks with vine.
PlacedFeature::FeaturePlacer vinesPlacer() {
    return [](WorldGenLevel& lv, RandomSource&, BlockPos pos) -> bool {
        bool placed = false;
        static const int D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        for (int dy = 0; dy < 12; ++dy) {
            BlockPos p{ pos.x, pos.y + dy, pos.z };
            if (!lv.isEmptyBlock(p)) continue;
            for (auto& d : D)
                if (!lv.isEmptyBlock(BlockPos{ p.x + d[0], p.y, p.z + d[1] })) { lv.setBlock(p, "minecraft:vine", 2); placed = true; break; }
        }
        return placed;
    };
}

// multiface_growth (glow_lichen / sculk_vein): coat a nearby cave surface.
PlacedFeature::FeaturePlacer multifacePlacer(const json& c) {
    const std::string block = c.value("block", std::string("minecraft:glow_lichen"));
    return [block](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        for (int i = 0; i < 16; ++i) {
            BlockPos p{ pos.x + r.nextInt(8) - 4, pos.y + r.nextInt(8) - 4, pos.z + r.nextInt(8) - 4 };
            if (lv.isEmptyBlock(p) && (!lv.isEmptyBlock(BlockPos{ p.x, p.y - 1, p.z }) || !lv.isEmptyBlock(BlockPos{ p.x, p.y + 1, p.z }))) { lv.setBlock(p, block, 2); return true; }
        }
        return false;
    };
}

// bamboo: a tall bamboo stalk (jungle).
PlacedFeature::FeaturePlacer bambooPlacer() {
    return [](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        if (!lv.isEmptyBlock(pos)) return false;
        const int h = 12 + r.nextInt(5);
        for (int y = 0; y < h; ++y) { BlockPos p{ pos.x, pos.y + y, pos.z }; if (!lv.isEmptyBlock(p)) break; lv.setBlock(p, "minecraft:bamboo", 2); }
        return true;
    };
}

// coral_tree / coral_claw / coral_mushroom: a small coral clump (warm ocean).
PlacedFeature::FeaturePlacer coralPlacer() {
    return [](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
        static const char* coral[] = { "minecraft:tube_coral_block", "minecraft:brain_coral_block", "minecraft:bubble_coral_block", "minecraft:fire_coral_block", "minecraft:horn_coral_block" };
        const char* col = coral[r.nextInt(5)];
        const int h = 1 + r.nextInt(3); BlockPos c = pos;
        for (int k = 0; k < h; ++k) { if (!lv.isEmptyBlock(c)) break; lv.setBlock(c, col, 2); c.y++; }
        return true;
    };
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
    if (t == "block_column") return { blockColumnPlacer(cfg), true };
    if (t == "huge_red_mushroom") return { hugeMushroomPlacer(cfg, true), true };
    if (t == "huge_brown_mushroom") return { hugeMushroomPlacer(cfg, false), true };
    if (t == "nether_forest_vegetation") return { netherVegPlacer(cfg), true };
    if (t == "seagrass") return { seagrassPlacer(cfg), true };
    if (t == "sea_pickle") return { seaPicklePlacer(cfg), true };
    if (t == "kelp") return { kelpPlacer(), true };
    if (t == "bamboo") return { bambooPlacer(), true };
    if (t == "huge_fungus") return { hugeFungusPlacer(cfg), true };
    if (t == "fallen_tree") return { fallenTreePlacer(cfg), true };
    if (t == "twisting_vines") return { vineColumnPlacer(cfg, 1, "minecraft:twisting_vines", "minecraft:twisting_vines_plant"), true };
    if (t == "weeping_vines") return { vineColumnPlacer(cfg, -1, "minecraft:weeping_vines", "minecraft:weeping_vines_plant"), true };
    if (t == "chorus_plant") return { chorusPlacer(), true };
    if (t == "sculk_patch") return { sculkPatchPlacer(cfg), true };
    if (t == "vines") return { vinesPlacer(), true };
    if (t == "multiface_growth") return { multifacePlacer(cfg), true };
    if (t == "coral_tree" || t == "coral_claw" || t == "coral_mushroom") return { coralPlacer(), true };
    if (t == "root_system") {
        auto inner = std::make_shared<Resolved>(resolveByName(dir, featureRef(cfg.value("feature", json()))));
        auto rootProv = parseProvider(cfg.value("root_state_provider", json::object()));
        return { [inner, rootProv](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
            for (int y = 0; y < 3; ++y) { BlockPos p{ pos.x, pos.y - 1 - y, pos.z }; if (!lv.isEmptyBlock(p)) lv.setBlock(p, rootProv->getState(r, p), 2); }
            return inner->ok && inner->placer(lv, r, pos);
        }, true };
    }
    if (t == "vegetation_patch" || t == "waterlogged_vegetation_patch") {
        auto ground = parseProvider(cfg["ground_state"]);
        const double vegChance = cfg.value("vegetation_chance", 0.0);
        auto veg = std::make_shared<Resolved>(resolveByName(dir, featureRef(cfg.value("vegetation_feature", json()))));
        IntProviderPtr xz = parseIntProvider(cfg.value("xz_radius", json(1)));
        return { [ground, vegChance, veg, xz](WorldGenLevel& lv, RandomSource& r, BlockPos pos) -> bool {
            const int rad = xz->sample(r); bool placed = false;
            for (int dx = -rad; dx <= rad; ++dx)
                for (int dz = -rad; dz <= rad; ++dz) {
                    if (dx * dx + dz * dz > rad * rad + 1) continue;
                    BlockPos g{ pos.x + dx, pos.y - 1, pos.z + dz };
                    if (lv.isEmptyBlock(g)) continue;
                    lv.setBlock(g, ground->getState(r, g), 2); placed = true;
                    BlockPos up{ g.x, g.y + 1, g.z };
                    if (veg->ok && lv.isEmptyBlock(up) && r.nextFloat() < vegChance) veg->placer(lv, r, up);
                }
            return placed;
        }, true };
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
    thread_local std::unordered_map<std::string, Resolved> cache;
    if (name.empty()) return noop();
    const std::string cacheKey = dir + '\0' + name;
    if (auto it = cache.find(cacheKey); it != cache.end()) return it->second;
    cache[cacheKey] = noop(); // placeholder breaks any reference cycle
    Resolved res;
    if (auto cf = loadJsonFile(dir, "configured_feature", name)) res = resolveConfigured(dir, *cf);
    else if (auto pf = loadJsonFile(dir, "placed_feature", name)) res = resolveByName(dir, featureRef((*pf)["feature"]));
    else res = noop();
    cache[cacheKey] = res;
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
    thread_local std::unordered_map<std::string, std::shared_ptr<PlacedFeature>> cache;
    const std::string cacheKey = dir + '\0' + key;
    if (auto it = cache.find(cacheKey); it != cache.end()) return it->second;
    auto pf = buildPlaced(dir, key);
    cache[cacheKey] = pf;
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

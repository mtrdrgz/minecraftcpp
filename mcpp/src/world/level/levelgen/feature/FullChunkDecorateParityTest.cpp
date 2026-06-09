// Whole-chunk DECORATION parity vs the real no-structures server (.mca).
//
// Generates terrain+carvers (already byte-exact) for a 5x5 block of chunks around each
// target chunk C, builds one shared MultiChunkLevel (WorldGenRegion-style: reads any
// chunk, writes radius-1 around the chunk currently being decorated), runs the real
// applyBiomeDecoration loop (FeatureSorter global order + setFeatureSeed(decoSeed,index,
// step)) over the inner 3x3 — but only for PORTED feature types (others are hard no-ops,
// logged+counted; each feature reseeds independently, so skipping does not perturb RNG)
// — then compares chunk C against the server dump.
//
// This certifies one decoration FAMILY at a time against the server:
//   ore     — the 20 ore blocks (ore configured features)
//   vegetal — seagrass / tall_seagrass / kelp / kelp_plant (aquatic vegetation)
//   all     — EVERY cell of chunk C vs the server dump, with a got=>want
//             transition breakdown so residuals stay attributable to named
//             unported features
//
//   full_chunk_decorate_parity --cases <server_chunk_cases.tsv>
//                              [--family ore|vegetal|all] [--datadir <dir>]
//
// After the 3x3 decoration, the FULL-promotion post-process pass runs for chunk C
// (LevelChunk.postProcessGeneration mirrored exactly as in the Java ground-truth
// harness FullChunkDecorateParity.postProcessChunk): marked positions come from
// the carvers (WorldCarver.java:147-158, carved fluid cells), the WorldGenRegion
// .setBlock getPostProcessPos hook (magma_block/soul_sand mark the block above,
// mushrooms mark themselves — Blocks.java:1087,1099,2097,4398 + :6934-6940), and
// features' explicit markPosForPostprocessing (DiskFeature's markAbove, Multiface
// placement/spread). Liquid marks run the bubble-column occupation check
// (LiquidBlock.java:162-167,191-193 -> BubbleColumnBlock.updateColumn:77-121);
// non-liquid marks run Block.updateFromNeighbourShapes (UPDATE_SHAPE_ORDER =
// WEST,EAST,NORTH,SOUTH,DOWN,UP, BlockBehaviour.java:85-87) — a no-op for the
// static full blocks, exact face revalidation for glow_lichen, and asserted
// invariants (throw if they would change) for the aquatic plants. The fluid
// SPREAD tick needs a real ServerLevel and is a counted hard no-op, exactly as
// in the certified Java harness.
//
// Heightmap model (mirrors the Java ground truth FullChunkDecorateParity.java +
// ChunkStatusTasks.generateFeatures): WorldGenRegion.getHeight(type,x,z) returns
// chunk.getHeight()+1 == Heightmap.getFirstAvailable (WorldGenRegion.java:391-393,
// ChunkAccess.java:182-194). The non-WG maps (OCEAN_FLOOR, WORLD_SURFACE) are primed
// at the FEATURES step start and then FROZEN: ProtoChunk.setBlockState only updates
// getPersistedStatus().heightmapsAfter(), which at CARVERS status is the *_WG pair
// (ProtoChunk.java:116-161). The *_WG maps stay live. Heightmap predicates:
// WORLD_SURFACE* = !isAir, OCEAN_FLOOR* = blocksMotion (Heightmap.java:147-150).

#include "../RandomSource.h"
#include "../IntProvider.h"
#include "../NoiseBasedChunkGenerator.h"
#include "../BiomeSource.h"
#include "../BiomeManager.h"
#include "../placement/PlacedFeature.h"
#include "../placement/PlacementModifier.h"
#include "../placement/NoiseCountPlacement.h"
#include "../placement/HeightmapPlacement.h"  // transitively brings HeightProvider.h + VerticalAnchor.h
#include "OreFeature.h"
#include "SeagrassFeature.h"
#include "KelpFeature.h"
#include "DiskFeature.h"
#include "SpringFeature.h"
#include "UnderwaterMagmaFeature.h"
#include "MultifaceGrowthFeature.h"
#include "FeatureSorter.h"
#include "BiomeFeatures.h"
#include "GenerationStep.h"
#include "../../block/Blocks.h"
#include "../../block/BlockState.h"
#include "../../block/BlockStates.h"
#include "../../block/BlockTags.h"
#include "../../block/BlockBehaviour.h"
#include "../../material/Fluids.h"
#include "../../chunk/LevelChunk.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <optional>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace mc::levelgen;
using namespace mc::levelgen::placement;
using namespace mc::valueproviders;
using mc::levelgen::feature::BiomeFeatures;
using mc::levelgen::feature::FeatureSorter;
using mc::levelgen::feature::GenerationStep;
using json = nlohmann::json;

namespace {

int floorDiv(int x, int y) { int q = x / y, r = x % y; if (r != 0 && ((r < 0) != (y < 0))) --q; return q; }
std::string stripNs(const std::string& id) { auto c = id.find(':'); return c == std::string::npos ? id : id.substr(c + 1); }
std::int64_t packChunk(int cx, int cz) { return (static_cast<std::int64_t>(static_cast<std::uint32_t>(cx)) << 32) | static_cast<std::uint32_t>(cz); }

void installBlockStatesEnv() {
    if (std::getenv("MCPP_BLOCK_STATES")) return;
    for (const char* p : { "mcpp/src/assets/block_states.json", "src/assets/block_states.json" }) {
        std::ifstream f(p, std::ios::binary);
        if (f) {
#if defined(_WIN32)
            _putenv_s("MCPP_BLOCK_STATES", p);
#else
            setenv("MCPP_BLOCK_STATES", p, 0);
#endif
            return;
        }
    }
}
std::string findDataDir() {
    for (const char* p : { "26.1.2/data/minecraft", "../26.1.2/data/minecraft" }) {
        std::ifstream f(std::string(p) + "/tags/block/dirt.json");
        if (f) return p;
    }
    return "26.1.2/data/minecraft";
}

const mc::block::BlockTags* g_tags = nullptr;        // data/minecraft/tags/block
const mc::block::BlockTags* g_fluidTags = nullptr;   // data/minecraft/tags/fluid
long long g_oreRuns = 0, g_orePlacedOk = 0;          // diagnostics
long long g_vegRuns = 0, g_vegPlacedOk = 0;
std::map<std::string, std::string> g_unportedType;   // featureKey -> configured type (hard no-ops)
std::map<std::string, long long> g_unportedSkips;    // configured type -> skipped RUNS
std::set<std::string> g_blocksMotionDefaulted;       // block ids the table defaulted (must be empty)
std::string stateName(const json& s) { return s.at("Name").get<std::string>(); }

// BlockStateBase.blocksMotion memoised by engine state id (the scan visits every
// block of every column; see BlockBehaviour.h for the Java-grounded table).
bool blocksMotionForStateId(std::uint32_t id) {
    static std::vector<std::int8_t> memo;   // -1 unknown, 0 false, 1 true
    if (id >= memo.size()) memo.resize(id + 1, -1);
    if (memo[id] < 0) {
        const mc::BlockState* s = mc::getBlockState(id);
        const std::string name = (s && s->block && !s->block->name.empty())
            ? "minecraft:" + s->block->name : std::string("minecraft:air");
        bool defaulted = false;
        memo[id] = mc::block::blocksMotion(name, &defaulted) ? 1 : 0;
        if (defaulted) g_blocksMotionDefaulted.insert(name);
    }
    return memo[id] == 1;
}

// ---- per-case biome lookup for the minecraft:biome placement filter ----
// BiomeFilter.shouldPlace (BiomeFilter.java:21-26): biome = level.getBiome(pos)
// (the BiomeManager fuzzed lookup over the chunk-cached quart biomes), then
// generator.getBiomeGenerationSettings(biome).hasFeature(placedFeature).
struct DecoBiomeContext {
    std::int64_t zoomSeed = 0;                                  // BiomeManager.obfuscateSeed(seed)
    std::function<std::string(int, int, int)> noiseBiome;       // quart -> biome (chunk-cache clamped)
    const BiomeFeatures* features = nullptr;

    bool hasFeatureAt(const std::string& featureKey, mc::BlockPos pos) const {
        const std::array<int, 3> q = BiomeManager::debugSelectQuart(zoomSeed, pos.x, pos.y, pos.z);
        const std::string biome = noiseBiome(q[0], q[1], q[2]);
        // BiomeGenerationSettings.hasFeature: the flattened feature set over all steps.
        for (int step = 0; step < GenerationStep::COUNT; ++step)
            if (features->biomeHasFeature(biome, step, featureKey)) return true;
        return false;
    }
};
const DecoBiomeContext* g_biomeCtx = nullptr;   // set per case before decorating

// The per-case level, reachable from cached feature placers (the multiface side
// map and postprocess marks live on it). Set/cleared alongside g_biomeCtx.
class MultiChunkLevel;
MultiChunkLevel* g_level = nullptr;

// ---- minimal loaders (throw on anything else: fail-closed) ----
IntProviderPtr loadIntProvider(const json& j) {
    if (j.is_number_integer()) return ConstantInt::of(j.get<int>());
    const std::string t = stripNs(j.at("type").get<std::string>());
    if (t == "constant") return ConstantInt::of(j.at("value").get<int>());
    if (t == "uniform") return UniformInt::of(j.at("min_inclusive").get<int>(), j.at("max_inclusive").get<int>());
    if (t == "biased_to_bottom") return BiasedToBottomInt::of(j.at("min_inclusive").get<int>(), j.at("max_inclusive").get<int>());
    if (t == "clamped") return ClampedInt::of(loadIntProvider(j.at("source")), j.at("min_inclusive").get<int>(), j.at("max_inclusive").get<int>());
    if (t == "trapezoid") return TrapezoidInt::of(j.at("min").get<int>(), j.at("max").get<int>(), j.value("plateau", 0));
    throw std::runtime_error("unsupported int_provider: " + t);
}
mc::levelgen::VerticalAnchorPtr loadVerticalAnchor(const json& j) {
    if (j.contains("absolute")) return mc::levelgen::VerticalAnchors::absolute(j.at("absolute").get<int>());
    if (j.contains("above_bottom")) return mc::levelgen::VerticalAnchors::aboveBottom(j.at("above_bottom").get<int>());
    if (j.contains("below_top")) return mc::levelgen::VerticalAnchors::belowTop(j.at("below_top").get<int>());
    throw std::runtime_error("unsupported vertical_anchor");
}
mc::levelgen::heightproviders::HeightProviderPtr loadHeightProvider(const json& j) {
    using namespace mc::levelgen::heightproviders;
    if (j.contains("absolute") || j.contains("above_bottom") || j.contains("below_top"))
        return std::make_shared<ConstantHeight>(loadVerticalAnchor(j));
    const std::string t = stripNs(j.at("type").get<std::string>());
    if (t == "constant") return std::make_shared<ConstantHeight>(loadVerticalAnchor(j.at("value")));
    if (t == "uniform") return std::make_shared<UniformHeight>(loadVerticalAnchor(j.at("min_inclusive")), loadVerticalAnchor(j.at("max_inclusive")));
    if (t == "biased_to_bottom") return std::make_shared<BiasedToBottomHeight>(loadVerticalAnchor(j.at("min_inclusive")), loadVerticalAnchor(j.at("max_inclusive")), j.value("inner", 1));
    if (t == "very_biased_to_bottom") return std::make_shared<VeryBiasedToBottomHeight>(loadVerticalAnchor(j.at("min_inclusive")), loadVerticalAnchor(j.at("max_inclusive")), j.value("inner", 1));
    if (t == "trapezoid") return std::make_shared<TrapezoidHeight>(loadVerticalAnchor(j.at("min_inclusive")), loadVerticalAnchor(j.at("max_inclusive")), j.value("plateau", 0));
    throw std::runtime_error("unsupported height_provider: " + t);
}
mc::levelgen::feature::OreRuleTest loadRuleTest(const json& j) {
    const std::string t = stripNs(j.at("predicate_type").get<std::string>());
    if (t == "always_true") return [](const std::string&, RandomSource&) { return true; };
    if (t == "tag_match") { const std::string tag = j.at("tag").get<std::string>();
        return [tag](const std::string& s, RandomSource&) { return g_tags->isInTag(s, tag); }; }
    if (t == "block_match") { const std::string b = j.at("block").get<std::string>();
        return [b](const std::string& s, RandomSource&) { return s == b; }; }
    if (t == "random_block_match") { const std::string b = j.at("block").get<std::string>(); const float p = j.at("probability").get<float>();
        return [b, p](const std::string& s, RandomSource& r) { return s == b && r.nextFloat() < p; }; }
    throw std::runtime_error("unsupported rule_test: " + t);
}
// RegistryCodecs.homogeneousList: a single id, a "#tag" reference, or a list of
// ids. Returns a membership predicate over canonical ids; `tags` resolves "#".
std::function<bool(const std::string&)> loadHolderSet(const json& j, const mc::block::BlockTags* tags) {
    auto entry = [tags](const std::string& e) -> std::function<bool(const std::string&)> {
        if (!e.empty() && e[0] == '#') {
            const std::string tag = e.substr(1);
            return [tags, tag](const std::string& id) { return tags->isInTag(id, tag); };
        }
        const std::string want = "minecraft:" + stripNs(e);
        return [want](const std::string& id) { return id == want; };
    };
    if (j.is_string()) return entry(j.get<std::string>());
    if (j.is_array()) {
        std::vector<std::function<bool(const std::string&)>> members;
        for (const auto& e : j) members.push_back(entry(e.get<std::string>()));
        return [members](const std::string& id) {
            for (const auto& m : members) if (m(id)) return true;
            return false;
        };
    }
    throw std::runtime_error("unsupported holder set");
}
// Materialise a holder set of PLAIN ids into a std::set (throws on "#tag" —
// callers that need an enumerable set, e.g. spring valid_blocks, never use tags).
std::set<std::string> loadIdSet(const json& j) {
    std::set<std::string> out;
    auto add = [&out](const std::string& e) {
        if (!e.empty() && e[0] == '#') throw std::runtime_error("tag in enumerable holder set");
        out.insert("minecraft:" + stripNs(e));
    };
    if (j.is_string()) add(j.get<std::string>());
    else if (j.is_array()) for (const auto& e : j) add(e.get<std::string>());
    else throw std::runtime_error("unsupported holder set");
    return out;
}

// BlockPredicate (fail-closed): matching_blocks (MatchingBlocksPredicate.java),
// matching_fluids (MatchingFluidsPredicate.java); both StateTestingPredicate
// subtypes testing the state at origin+offset (offset default 0,0,0).
std::function<bool(WorldGenLevel&, BlockPos)> loadBlockPredicate(const json& j) {
    const std::string t = stripNs(j.at("type").get<std::string>());
    std::array<int, 3> off{ 0, 0, 0 };
    if (j.contains("offset")) {
        const auto& o = j.at("offset");
        off = { o.at(0).get<int>(), o.at(1).get<int>(), o.at(2).get<int>() };
    }
    if (t == "matching_blocks") {
        auto member = loadHolderSet(j.at("blocks"), g_tags);
        return [member, off](WorldGenLevel& level, BlockPos p) {
            return member(level.getBlockState(BlockPos{ p.x + off[0], p.y + off[1], p.z + off[2] }));
        };
    }
    if (t == "matching_fluids") {
        auto member = loadHolderSet(j.at("fluids"), g_fluidTags);
        return [member, off](WorldGenLevel& level, BlockPos p) {
            const mc::material::FluidState fs =
                mc::material::fluidStateOf(level.getBlockState(BlockPos{ p.x + off[0], p.y + off[1], p.z + off[2] }));
            return !fs.isEmpty() && member(fs.fluid);
        };
    }
    throw std::runtime_error("unsupported block_predicate: " + t);
}

// BlockStateProvider.getOptionalState (fail-closed): simple_state_provider
// (SimpleStateProvider.java:23-25, no RNG) and rule_based_state_provider
// (RuleBasedStateProvider.java:57-66: first matching rule -> then.getState,
// none -> fallback or null). The dump stores block ids; properties on the
// provided states do not occur in the disk providers.
mc::levelgen::feature::DiskStateProvider loadStateProvider(const json& j) {
    const std::string t = stripNs(j.at("type").get<std::string>());
    if (t == "simple_state_provider") {
        const std::string s = stateName(j.at("state"));
        return [s](WorldGenLevel&, RandomSource&, BlockPos) { return std::optional<std::string>(s); };
    }
    if (t == "rule_based_state_provider") {
        struct Rule {
            std::function<bool(WorldGenLevel&, BlockPos)> ifTrue;
            mc::levelgen::feature::DiskStateProvider then;
        };
        auto rules = std::make_shared<std::vector<Rule>>();
        for (const auto& rj : j.at("rules"))
            rules->push_back({ loadBlockPredicate(rj.at("if_true")), loadStateProvider(rj.at("then")) });
        std::shared_ptr<mc::levelgen::feature::DiskStateProvider> fallback;
        if (j.contains("fallback"))
            fallback = std::make_shared<mc::levelgen::feature::DiskStateProvider>(loadStateProvider(j.at("fallback")));
        return [rules, fallback](WorldGenLevel& level, RandomSource& random, BlockPos pos) -> std::optional<std::string> {
            for (const Rule& r : *rules) {
                if (r.ifTrue(level, pos)) {
                    std::optional<std::string> s = r.then(level, random, pos);   // then.getState: never null here
                    if (!s.has_value()) throw std::logic_error("rule_based then-provider returned null");
                    return s;
                }
            }
            return fallback ? (*fallback)(level, random, pos) : std::nullopt;
        };
    }
    throw std::runtime_error("unsupported state_provider: " + t);
}

Heightmap::Types parseHeightmapType(const std::string& s) {
    if (s == "WORLD_SURFACE_WG") return Heightmap::Types::WORLD_SURFACE_WG;
    if (s == "WORLD_SURFACE") return Heightmap::Types::WORLD_SURFACE;
    if (s == "OCEAN_FLOOR_WG") return Heightmap::Types::OCEAN_FLOOR_WG;
    if (s == "OCEAN_FLOOR") return Heightmap::Types::OCEAN_FLOOR;
    if (s == "MOTION_BLOCKING") return Heightmap::Types::MOTION_BLOCKING;
    if (s == "MOTION_BLOCKING_NO_LEAVES") return Heightmap::Types::MOTION_BLOCKING_NO_LEAVES;
    throw std::runtime_error("unsupported heightmap type: " + s);
}
// `featureKey` (ns-qualified placed feature id) backs the minecraft:biome filter.
std::shared_ptr<const PlacementModifier> loadModifier(const json& j, const std::string& featureKey) {
    const std::string t = stripNs(j.at("type").get<std::string>());
    if (t == "count") return std::make_shared<CountPlacement>(loadIntProvider(j.at("count")));
    if (t == "rarity_filter") return std::make_shared<RarityFilter>(j.at("chance").get<int>());
    if (t == "in_square") return std::make_shared<InSquarePlacement>();
    if (t == "height_range") return std::make_shared<HeightRangePlacement>(loadHeightProvider(j.at("height")));
    if (t == "heightmap") return std::make_shared<HeightmapPlacement>(parseHeightmapType(j.at("heightmap").get<std::string>()));
    if (t == "noise_based_count") return std::make_shared<NoiseBasedCountPlacement>(
        j.at("noise_to_count_ratio").get<int>(), j.at("noise_factor").get<double>(), j.value("noise_offset", 0.0));
    if (t == "biome") return std::make_shared<BiomeFilter>(
        [featureKey](WorldGenLevel&, BlockPos pos) { return g_biomeCtx->hasFeatureAt(featureKey, pos); });
    // SurfaceRelativeThresholdFilter.java: optional min/max default to
    // Integer.MIN_VALUE / Integer.MAX_VALUE (compared in long).
    if (t == "surface_relative_threshold_filter") return std::make_shared<SurfaceRelativeThresholdFilter>(
        parseHeightmapType(j.at("heightmap").get<std::string>()),
        j.contains("min_inclusive") ? j.at("min_inclusive").get<std::int64_t>()
                                    : static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
        j.contains("max_inclusive") ? j.at("max_inclusive").get<std::int64_t>()
                                    : static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()));
    // BlockPredicateFilter.java:25-27: keep origin iff predicate.test(level, origin).
    if (t == "block_predicate_filter") {
        auto pred = loadBlockPredicate(j.at("predicate"));
        return std::make_shared<BlockPredicateFilter>(
            [pred](WorldGenLevel& level, BlockPos pos) { return pred(level, pos); });
    }
    throw std::runtime_error("unsupported placement modifier: " + t);
}

// MultiChunkLevel: WorldGenRegion-style level over a fixed grid of generated chunks.
class MultiChunkLevel final : public WorldGenLevel {
public:
    MultiChunkLevel(std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>>* chunks,
                    int minY, int maxY)
        : m_chunks(chunks), m_minY(minY), m_maxY(maxY) { m_airId = mc::getDefaultBlockStateId("air", 0); }

    void setDecorating(int cx, int cz) { m_dcx = cx; m_dcz = cz; }

    // Heightmap.primeHeightmaps for the non-WG maps at the FEATURES step start; they
    // FREEZE afterwards (ProtoChunk at CARVERS status only live-updates the WG pair).
    // Stored as the highest passing y per column (the +1 to getFirstAvailable happens
    // in getHeight, mirroring WorldGenRegion.getHeight = chunk.getHeight()+1).
    void freezeHeights() {
        for (auto& [key, chunkPtr] : *m_chunks) {
            const int cx = static_cast<int>(key >> 32);
            const int cz = static_cast<int>(static_cast<std::int32_t>(key & 0xffffffff));
            auto& fr = m_frozen[key];
            for (int i = 0; i < 256; ++i) {
                const int x = cx * 16 + (i & 15), z = cz * 16 + (i >> 4);
                fr.oceanFloor[i] = scan(chunkPtr.get(), x, z, /*motionBlocking=*/true);
                fr.worldSurface[i] = scan(chunkPtr.get(), x, z, /*motionBlocking=*/false);
            }
        }
    }

    int getMinY() const override { return m_minY; }

    // WorldGenRegion.getHeight (WorldGenRegion.java:391-393): chunk heightmap value
    // + 1 == Heightmap.getFirstAvailable. *_WG live (scan), non-WG frozen snapshot.
    int getHeight(Heightmap::Types type, int x, int z) const override {
        const int cx = floorDiv(x, 16), cz = floorDiv(z, 16);
        mc::LevelChunk* c = at(cx, cz);
        if (!c) return m_minY;   // empty column: getFirstAvailable == minY
        switch (type) {
            case Heightmap::Types::WORLD_SURFACE_WG: return scan(c, x, z, false) + 1;
            case Heightmap::Types::OCEAN_FLOOR_WG:   return scan(c, x, z, true) + 1;
            case Heightmap::Types::OCEAN_FLOOR:
            case Heightmap::Types::WORLD_SURFACE: {
                auto it = m_frozen.find(packChunk(cx, cz));
                if (it == m_frozen.end()) throw std::logic_error("non-WG heightmap read before freezeHeights()");
                const int idx = ((z - cz * 16) << 4) | (x - cx * 16);
                return (type == Heightmap::Types::OCEAN_FLOOR ? it->second.oceanFloor[idx]
                                                              : it->second.worldSurface[idx]) + 1;
            }
            default:
                // MOTION_BLOCKING* need the fluid-inclusive predicate; no ported
                // feature reads them — fail closed instead of guessing.
                throw std::logic_error("MOTION_BLOCKING heightmaps not ported");
        }
    }
    std::string getBlockState(BlockPos p) const override {
        mc::LevelChunk* c = at(floorDiv(p.x, 16), floorDiv(p.z, 16));
        if (!c || p.y < m_minY || p.y >= m_maxY) return "minecraft:air";
        return name(c->getBlock(p.x, p.y, p.z));
    }
    // WorldGenRegion.setBlock (WorldGenRegion.java:264-301): false when
    // ensureCanWrite rejects; otherwise write and — unless flag 16 is set and not
    // during the FULL post-process pass — ask the new state for a post-process
    // position and mark it (WorldGenRegion.java:288-293). The worldgen-reachable
    // blocks with the postProcess property are brown_mushroom (Blocks.java:1087)
    // and red_mushroom (:1099) -> postProcessSelf (pos, :6934-6936), soul_sand
    // (:2097) and magma_block (:4398) -> postProcessAbove (pos.above(), :6938-6940).
    bool setBlockChecked(BlockPos p, const std::string& state, int flags) override {
        if (!ensureCanWrite(p)) return false;
        mc::LevelChunk* c = at(floorDiv(p.x, 16), floorDiv(p.z, 16));
        if (!c) return false;
        // The grid stores block ids (default states); properties ([half=upper],
        // [age=N]) are not byte-compared (the server dump records block ids).
        // glow_lichen face/waterlogged bits live in the multiface side map: any
        // overwrite invalidates the entry (the multiface port re-registers its own).
        const std::string block = mc::block::blockName(state);
        c->setBlock(p.x, p.y, p.z, mc::getDefaultBlockStateId(stripNs(block), m_airId));
        m_multiface.erase(std::make_tuple(p.x, p.y, p.z));
        if ((flags & 16) == 0 && !m_postprocessing) {
            if (block == "minecraft:magma_block" || block == "minecraft:soul_sand") {
                markPosForPostprocessing(BlockPos{ p.x, p.y + 1, p.z });
            } else if (block == "minecraft:brown_mushroom" || block == "minecraft:red_mushroom") {
                markPosForPostprocessing(p);
            }
        }
        return true;
    }
    void setBlock(BlockPos p, const std::string& state, int flags) override {
        (void)setBlockChecked(p, state, flags);
    }
    bool isEmptyBlock(BlockPos p) const override { return getBlockState(p) == "minecraft:air"; }

    // ---- FULL-promotion post-processing marks ----
    // ChunkAccess.markPosForPostprocessing: append to the per-section ShortList of
    // the chunk CONTAINING pos (duplicates allowed, insertion order kept). The
    // postprocess pass iterates sections ascending, insertion order within
    // (LevelChunk.postProcessGeneration / the Java harness postProcessChunk).
    void markPosForPostprocessing(BlockPos p) override {
        const int cx = floorDiv(p.x, 16), cz = floorDiv(p.z, 16);
        if (at(cx, cz) == nullptr) throw std::logic_error("postprocess mark outside generated grid");
        if (p.y < m_minY || p.y >= m_maxY) throw std::logic_error("postprocess mark outside build height");
        m_marks[packChunk(cx, cz)].push_back(Mark{ (p.y - m_minY) >> 4, p });
    }
    std::vector<BlockPos> postProcessMarks(int cx, int cz) {
        auto it = m_marks.find(packChunk(cx, cz));
        if (it == m_marks.end()) return {};
        std::stable_sort(it->second.begin(), it->second.end(),
                         [](const Mark& a, const Mark& b) { return a.section < b.section; });
        std::vector<BlockPos> out;
        out.reserve(it->second.size());
        for (const Mark& m : it->second) out.push_back(m.pos);
        return out;
    }
    void setPostprocessing(bool v) { m_postprocessing = v; }

    // ---- multiface (glow_lichen) face/waterlogged side map ----
    mc::levelgen::feature::MultifaceBlockState multifaceStateAt(BlockPos p) const {
        mc::levelgen::feature::MultifaceBlockState s;
        s.block = getBlockState(p);
        if (s.block == "minecraft:glow_lichen") {
            auto it = m_multiface.find(std::make_tuple(p.x, p.y, p.z));
            if (it == m_multiface.end())
                throw std::logic_error("glow_lichen without face record (multiface side map out of sync)");
            s.isPlaceBlock = true;
            s.faces = it->second.first;
            s.waterlogged = it->second.second;
        }
        return s;
    }
    void putMultifaceState(BlockPos p, std::uint8_t faces, bool waterlogged) {
        m_multiface[std::make_tuple(p.x, p.y, p.z)] = { faces, waterlogged };
    }

    // BlockState.canSurvive(level, pos) for the block set the ported features gate
    // on. Fail-closed: anything else throws.
    bool canSurvive(const std::string& state, BlockPos pos) const override {
        const std::string block = mc::block::blockName(state);
        const BlockPos belowPos{ pos.x, pos.y - 1, pos.z };
        if (block == "minecraft:seagrass") {
            // VegetationBlock.canSurvive (VegetationBlock.java:44-47) with
            // SeagrassBlock.mayPlaceOn (SeagrassBlock.java:46-48).
            return seagrassMayPlaceOn(belowPos);
        }
        if (block == "minecraft:tall_seagrass") {
            // TallSeagrassBlock.canSurvive, LOWER branch (TallSeagrassBlock.java:67-76):
            // super.canSurvive (DoublePlantBlock.java:77-84 -> VegetationBlock with
            // TallSeagrassBlock.mayPlaceOn, identical to seagrass) && the fluid at pos
            // is(FluidTags.WATER) && isFull. (The UPPER half is never canSurvive-checked
            // during feature placement.)
            const mc::material::FluidState fs = mc::material::fluidStateOf(getBlockState(pos));
            return seagrassMayPlaceOn(belowPos) && fs.is(*g_fluidTags, "minecraft:water") && fs.isFull();
        }
        if (block == "minecraft:kelp" || block == "minecraft:kelp_plant") {
            // GrowingPlantBlock.canSurvive (GrowingPlantBlock.java:47-55); for both kelp
            // blocks head=KELP, body=KELP_PLANT, growthDirection=UP, and canAttachTo =
            // !is(CANNOT_SUPPORT_KELP) (KelpBlock.java:46-48, KelpPlantBlock.java:40-42).
            const std::string attached = mc::block::blockName(getBlockState(belowPos));
            if (g_tags->isInTag(attached, "minecraft:cannot_support_kelp")) return false;
            if (attached == "minecraft:kelp" || attached == "minecraft:kelp_plant") return true;
            bool defaulted = false;
            const bool sturdy = mc::block::isFaceSturdyUp(attached, &defaulted);
            if (defaulted) g_blocksMotionDefaulted.insert(attached);
            return sturdy;
        }
        throw std::logic_error("canSurvive not ported for " + block);
    }
    bool ensureCanWrite(BlockPos p) const override {
        if (p.y < m_minY || p.y >= m_maxY) return false;
        const int cx = floorDiv(p.x, 16), cz = floorDiv(p.z, 16);
        if (std::abs(cx - m_dcx) > 1 || std::abs(cz - m_dcz) > 1) return false;  // radius-1
        return at(cx, cz) != nullptr;
    }
    std::string name(std::uint32_t id) const {
        const mc::BlockState* s = mc::getBlockState(id);
        if (!s || !s->block || s->block->name.empty()) return "minecraft:air";
        return "minecraft:" + s->block->name;
    }
private:
    // SeagrassBlock/TallSeagrassBlock.mayPlaceOn (SeagrassBlock.java:46-48,
    // TallSeagrassBlock.java:45-47): belowState.isFaceSturdy(level, below, UP)
    // && !belowState.is(BlockTags.CANNOT_SUPPORT_SEAGRASS).
    bool seagrassMayPlaceOn(BlockPos belowPos) const {
        const std::string below = mc::block::blockName(getBlockState(belowPos));
        bool defaulted = false;
        const bool sturdy = mc::block::isFaceSturdyUp(below, &defaulted);
        if (defaulted) g_blocksMotionDefaulted.insert(below);
        return sturdy && !g_tags->isInTag(below, "minecraft:cannot_support_seagrass");
    }
    mc::LevelChunk* at(int cx, int cz) const {
        auto it = m_chunks->find(packChunk(cx, cz));
        return it == m_chunks->end() ? nullptr : it->second.get();
    }
    // Top-down scan: highest y whose state passes the heightmap predicate
    // (Heightmap.java:147-150 — WORLD_SURFACE* = !isAir, OCEAN_FLOOR* = blocksMotion),
    // or minY-1 when the column has none (getHeight adds 1 -> minY, matching an
    // unset Heightmap entry).
    int scan(mc::LevelChunk* c, int x, int z, bool motionBlocking) const {
        for (int y = m_maxY - 1; y >= m_minY; --y) {
            const std::uint32_t id = c->getBlock(x, y, z);
            if (id == m_airId) continue;
            if (motionBlocking && !blocksMotionForStateId(id)) continue;
            return y;
        }
        return m_minY - 1;
    }
    struct FrozenMaps { std::array<int, 256> oceanFloor; std::array<int, 256> worldSurface; };
    struct Mark { int section; BlockPos pos; };
    std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>>* m_chunks;
    std::map<std::int64_t, FrozenMaps> m_frozen;
    std::unordered_map<std::int64_t, std::vector<Mark>> m_marks;
    std::map<std::tuple<int, int, int>, std::pair<std::uint8_t, bool>> m_multiface;
    bool m_postprocessing = false;
    int m_minY, m_maxY, m_dcx = 0, m_dcz = 0; std::uint32_t m_airId{};
};

// ============================ FULL-promotion post-processing ============================
// Mirrors the Java harness postProcessChunk (FullChunkDecorateParity.java:392-424),
// itself LevelChunk.postProcessGeneration (LevelChunk.java:566) — proven
// byte-identical to the no-structures server.

// BubbleColumnBlock.canOccupy (BubbleColumnBlock.java:99-109): the state is the
// bubble column itself, or a LiquidBlock whose fluid is in the
// bubble_column_can_occupy fluid tag (== water), is a source, and has amount>=8.
// Plant blocks with a virtual water fluid (seagrass/kelp/...) are NOT LiquidBlocks
// and stop the column.
bool bubbleCanOccupy(const std::string& state) {
    if (state == "minecraft:bubble_column") return true;
    if (state != "minecraft:water" && state != "minecraft:lava") return false;   // instanceof LiquidBlock
    const mc::material::FluidState fs = mc::material::fluidStateOf(state);
    return fs.is(*g_fluidTags, "minecraft:bubble_column_can_occupy") && fs.isSource() && fs.amount >= 8;
}

// BubbleColumnBlock.getColumnState (BubbleColumnBlock.java:111-121), reduced to
// block ids (DRAG_DOWN does not change the id): below bubble -> bubble; below in
// enables_bubble_column_push_up (soul_sand) or enables_bubble_column_drag_down
// (magma_block) -> bubble; else the occupy state itself (a water id; the
// occupy-is-bubble -> WATER branch also yields a water id).
std::string bubbleColumnState(const std::string& below, const std::string& occupy) {
    if (below == "minecraft:bubble_column") return "minecraft:bubble_column";
    if (g_tags->isInTag(below, "minecraft:enables_bubble_column_push_up")) return "minecraft:bubble_column";
    if (g_tags->isInTag(below, "minecraft:enables_bubble_column_drag_down")) return "minecraft:bubble_column";
    return occupy == "minecraft:bubble_column" ? "minecraft:water" : occupy;
}

// BubbleColumnBlock.updateColumn (BubbleColumnBlock.java:77-97).
void bubbleUpdateColumn(MultiChunkLevel& level, BlockPos occupyAt) {
    const std::string occupyState = level.getBlockState(occupyAt);
    const std::string below = level.getBlockState(BlockPos{ occupyAt.x, occupyAt.y - 1, occupyAt.z });
    if (!bubbleCanOccupy(occupyState)) return;
    const std::string columnState = bubbleColumnState(below, occupyState);
    level.setBlockChecked(occupyAt, columnState, 2);
    BlockPos pos{ occupyAt.x, occupyAt.y + 1, occupyAt.z };
    while (bubbleCanOccupy(level.getBlockState(pos))) {
        if (!level.setBlockChecked(pos, columnState, 2)) return;
        pos = BlockPos{ pos.x, pos.y + 1, pos.z };
    }
}

// Block.updateFromNeighbourShapes (Block.java:204-214, UPDATE_SHAPE_ORDER =
// WEST,EAST,NORTH,SOUTH,DOWN,UP per BlockBehaviour.java:85-87) evaluated for the
// closed set of non-liquid blocks that can be marked here. Static full blocks use
// BlockBehaviour's identity updateShape; glow_lichen runs MultifaceBlock.updateShape
// exactly (face revalidation, MultifaceBlock.java:135-143 + removeFace:263-266);
// the aquatic plants' conversion/removal branches cannot fire under the placement
// invariants — asserted, throwing if one ever would (port it then, never guess).
void postUpdateFromNeighbourShapes(MultiChunkLevel& level, BlockPos bp, const std::string& bs) {
    static const std::set<std::string> staticNoOp = {
        // Block/BlockBehaviour default updateShape returns the state unchanged.
        "minecraft:air", "minecraft:stone", "minecraft:granite", "minecraft:diorite",
        "minecraft:andesite", "minecraft:tuff", "minecraft:deepslate", "minecraft:bedrock",
        "minecraft:obsidian", "minecraft:gravel", "minecraft:sand", "minecraft:red_sand",
        "minecraft:sandstone", "minecraft:dirt", "minecraft:grass_block", "minecraft:coarse_dirt",
        "minecraft:podzol", "minecraft:clay", "minecraft:mud", "minecraft:magma_block",
        "minecraft:ice", "minecraft:packed_ice", "minecraft:blue_ice", "minecraft:snow_block",
        "minecraft:coal_ore", "minecraft:copper_ore", "minecraft:iron_ore", "minecraft:gold_ore",
        "minecraft:redstone_ore", "minecraft:lapis_ore", "minecraft:diamond_ore", "minecraft:emerald_ore",
        "minecraft:deepslate_coal_ore", "minecraft:deepslate_copper_ore", "minecraft:deepslate_iron_ore",
        "minecraft:deepslate_gold_ore", "minecraft:deepslate_redstone_ore", "minecraft:deepslate_lapis_ore",
        "minecraft:deepslate_diamond_ore", "minecraft:deepslate_emerald_ore",
        // BubbleColumnBlock.updateShape (BubbleColumnBlock.java:160-179) only
        // schedules ticks (dropped during worldgen) and returns super == unchanged.
        "minecraft:bubble_column",
    };
    if (staticNoOp.count(bs) != 0) return;

    if (bs == "minecraft:glow_lichen") {
        // MultifaceBlock.updateShape per neighbour direction: hasFace(direction) &&
        // !canAttachTo -> removeFace; no faces left -> AIR (MultifaceBlock.java:
        // 135-143, 263-266). UPDATE_SHAPE_ORDER = WEST,EAST,NORTH,SOUTH,DOWN,UP.
        namespace md = mc::levelgen::feature::multiface_detail;
        mc::levelgen::feature::MultifaceBlockState st = level.multifaceStateAt(bp);
        static const int order[6] = { 4, 5, 2, 3, 0, 1 };
        bool changed = false;
        for (int dir : order) {
            if (md::hasFace(st, dir) && !md::canAttachTo(level, md::relative(bp, dir))) {
                st.faces = static_cast<std::uint8_t>(st.faces & ~(1u << dir));
                changed = true;
            }
        }
        if (!changed) return;
        if (st.faces == 0) {
            // removeFace -> Blocks.AIR (the waterlogged bit is dropped, verbatim).
            level.setBlockChecked(bp, "minecraft:air", 276);   // harness flags (:417)
        } else {
            level.putMultifaceState(bp, st.faces, st.waterlogged);   // id unchanged
        }
        return;
    }
    if (bs == "minecraft:seagrass") {
        // VegetationBlock.updateShape (VegetationBlock.java:28-41): !canSurvive -> AIR.
        if (!level.canSurvive("minecraft:seagrass", bp)) {
            throw std::logic_error("postprocess: seagrass updateShape would remove the block — port it");
        }
        return;
    }
    if (bs == "minecraft:tall_seagrass") {
        // DoublePlantBlock.updateShape (DoublePlantBlock.java:41-61). The grid drops
        // the HALF property; infer it (the upper half is exactly the cell whose
        // below is the lower tall_seagrass — stacks deeper than 2 cannot place).
        const std::string belowBlock = level.getBlockState(BlockPos{ bp.x, bp.y - 1, bp.z });
        if (belowBlock == "minecraft:tall_seagrass") return;   // upper: below is its lower half
        const std::string aboveBlock = level.getBlockState(BlockPos{ bp.x, bp.y + 1, bp.z });
        if (aboveBlock != "minecraft:tall_seagrass" || !level.canSurvive("minecraft:tall_seagrass", bp)) {
            throw std::logic_error("postprocess: tall_seagrass updateShape would remove the block — port it");
        }
        return;
    }
    if (bs == "minecraft:kelp") {
        // GrowingPlantHeadBlock.updateShape (GrowingPlantHeadBlock.java:76-105):
        // !canSurvive only schedules a tick (no-op here); the head->body conversion
        // fires iff kelp/kelp_plant sits directly above — impossible by construction.
        const std::string above = level.getBlockState(BlockPos{ bp.x, bp.y + 1, bp.z });
        if (above == "minecraft:kelp" || above == "minecraft:kelp_plant") {
            throw std::logic_error("postprocess: kelp head->body conversion would fire — port it");
        }
        return;
    }
    if (bs == "minecraft:kelp_plant") {
        // GrowingPlantBodyBlock.updateShape (GrowingPlantBodyBlock.java:36-60):
        // body->head conversion fires iff the block above is neither body nor head.
        const std::string above = level.getBlockState(BlockPos{ bp.x, bp.y + 1, bp.z });
        if (above != "minecraft:kelp" && above != "minecraft:kelp_plant") {
            throw std::logic_error("postprocess: kelp_plant body->head conversion would fire — port it");
        }
        return;
    }
    throw std::logic_error("postprocess: updateFromNeighbourShapes not ported for " + bs);
}

// FullChunkDecorateParity.postProcessChunk (the Java harness :392-424). Returns the
// number of skipped fluid-spread ticks (logged hard no-op, as in the harness).
long long postProcessChunk(MultiChunkLevel& level, int Cx, int Cz) {
    level.setDecorating(Cx, Cz);     // CUR_CX/CUR_CZ = C: updateColumn writes allowed
    level.setPostprocessing(true);   // suppress re-marking (POSTPROCESSING flag)
    long long skippedFluidTicks = 0;
    for (const BlockPos& bp : level.postProcessMarks(Cx, Cz)) {
        const std::string bs = level.getBlockState(bp);
        const mc::material::FluidState fs = mc::material::fluidStateOf(bs);
        const bool isLiquidBlock = (bs == "minecraft:water" || bs == "minecraft:lava");
        // Waterlogged-style fluid (non-LiquidBlock with a fluid state): the block-id
        // fluid model misses glow_lichen's WATERLOGGED property — the multiface side
        // map carries it (MultifaceBlock waterlogged states getFluidState == water).
        bool hasFluid = !fs.isEmpty();
        if (bs == "minecraft:glow_lichen") hasFluid = level.multifaceStateAt(bp).waterlogged;
        if (hasFluid && !isLiquidBlock) ++skippedFluidTicks;   // waterlogged-style
        if (isLiquidBlock) {
            ++skippedFluidTicks;   // fluid spread tick: hard no-op without a ServerLevel
            // LiquidBlock.tick == shouldBubbleColumnOccupy -> updateColumn
            // (LiquidBlock.java:162-167, 191-193).
            if (fs.is(*g_fluidTags, "minecraft:bubble_column_can_occupy") && fs.isSource() && fs.isFull()) {
                bubbleUpdateColumn(level, bp);
            }
        } else {
            postUpdateFromNeighbourShapes(level, bp, bs);
        }
    }
    level.setPostprocessing(false);
    return skippedFluidTicks;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath, dataDir, family = "ore";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
        else if (a == "--datadir" && i + 1 < argc) dataDir = argv[++i];
        else if (a == "--family" && i + 1 < argc) family = argv[++i];
    }
    if (casesPath.empty()) { std::cerr << "usage: full_chunk_decorate_parity --cases <server tsv> [--family ore|vegetal|all]\n"; return 2; }
    if (dataDir.empty()) dataDir = findDataDir();
    if (family != "ore" && family != "vegetal" && family != "all") { std::cerr << "--family must be ore, vegetal or all\n"; return 2; }

    installBlockStatesEnv();
    mc::initBlocks();
    static mc::block::BlockTags tags = mc::block::BlockTags::loadFromDirectory(dataDir + "/tags/block");
    g_tags = &tags;
    // Fluid tags: TallSeagrassBlock.canSurvive consults FluidTags.WATER (the Java
    // ground truth binds them too — unbound tags silently test false and tall
    // seagrass never places).
    static mc::block::BlockTags fluidTags = mc::block::BlockTags::loadFromDirectory(dataDir + "/tags/fluid");
    g_fluidTags = &fluidTags;
    BiomeFeatures biomeFeatures = BiomeFeatures::loadFromDirectory(dataDir + "/worldgen/biome");

    // Global per-step feature order/index (== the setFeatureSeed index), over all biomes.
    const std::vector<std::string> sources = BiomeSource::collectOverworldPossibleBiomes();
    const std::set<std::string> sourcesSet(sources.begin(), sources.end());
    const auto stepData = FeatureSorter::buildFeaturesPerStep(sources, biomeFeatures, true);

    // Resolve every placed_feature whose configured type is ported (ore, seagrass,
    // kelp) once; cache by feature key. Unported types are hard no-ops, counted.
    std::set<std::string> oreFamily;
    const std::set<std::string> vegetalFamily = {
        "minecraft:seagrass", "minecraft:tall_seagrass", "minecraft:kelp", "minecraft:kelp_plant",
    };
    std::map<std::string, std::shared_ptr<PlacedFeature>> cache;   // key -> placed (nullptr if unported)
    auto resolveFeature = [&](const std::string& featureKey) -> std::shared_ptr<PlacedFeature> {
        auto it = cache.find(featureKey);
        if (it != cache.end()) return it->second;
        std::shared_ptr<PlacedFeature> result;
        try {
            const std::string pname = stripNs(featureKey);
            json placedJson; { std::ifstream f(dataDir + "/worldgen/placed_feature/" + pname + ".json"); if (!f) throw std::runtime_error("no placed_feature"); f >> placedJson; }
            const std::string cfgId = stripNs(placedJson.at("feature").get<std::string>());
            json cfgJson; { std::ifstream f(dataDir + "/worldgen/configured_feature/" + cfgId + ".json"); if (!f) throw std::runtime_error("no configured_feature"); f >> cfgJson; }
            const std::string type = stripNs(cfgJson.at("type").get<std::string>());

            PlacedFeature::FeaturePlacer placer;
            if (type == "ore") {
                const json& cc = cfgJson.at("config");
                const int size = cc.at("size").get<int>();
                const float discard = cc.at("discard_chance_on_air_exposure").get<float>();
                std::vector<mc::levelgen::feature::OreTarget> targets;
                for (const auto& tj : cc.at("targets")) {
                    const std::string st = stateName(tj.at("state"));
                    oreFamily.insert(st);
                    targets.push_back({ loadRuleTest(tj.at("target")), st });
                }
                placer = mc::levelgen::feature::makeOrePlacer(std::move(targets), size, discard);
            } else if (type == "seagrass") {
                // ProbabilityFeatureConfiguration (seagrass_short=0.3 / mid=0.6 / tall=0.8)
                placer = mc::levelgen::feature::makeSeagrassPlacer(cfgJson.at("config").at("probability").get<double>());
            } else if (type == "kelp") {
                placer = mc::levelgen::feature::makeKelpPlacer();
            } else if (type == "disk") {
                // DiskConfiguration: state_provider, target (BlockPredicate),
                // radius (IntProvider 0..8), half_height (0..4).
                const json& cc = cfgJson.at("config");
                placer = mc::levelgen::feature::makeDiskPlacer(
                    loadStateProvider(cc.at("state_provider")),
                    loadBlockPredicate(cc.at("target")),
                    loadIntProvider(cc.at("radius")),
                    cc.at("half_height").get<int>());
            } else if (type == "spring_feature") {
                // SpringConfiguration. config.state is a FLUID state; the placed
                // block is state.createLegacyBlock() == the plain source liquid
                // block (FlowingFluid.getLegacyLevel: isSource -> LEVEL 0,
                // independent of FALLING) — see SpringFeature.h.
                const json& cc = cfgJson.at("config");
                const std::string fluid = "minecraft:" + stripNs(cc.at("state").at("Name").get<std::string>());
                if (fluid != "minecraft:water" && fluid != "minecraft:lava")
                    throw std::runtime_error("unsupported spring fluid: " + fluid);
                placer = mc::levelgen::feature::makeSpringPlacer(
                    fluid,
                    cc.value("requires_block_below", true),
                    cc.value("rock_count", 4),
                    cc.value("hole_count", 1),
                    loadIdSet(cc.at("valid_blocks")));
            } else if (type == "underwater_magma") {
                const json& cc = cfgJson.at("config");
                placer = mc::levelgen::feature::makeUnderwaterMagmaPlacer(
                    cc.at("floor_search_range").get<int>(),
                    cc.at("placement_radius_around_floor").get<int>(),
                    cc.at("placement_probability_per_valid_position").get<float>());
            } else if (type == "multiface_growth") {
                // MultifaceGrowthConfiguration codec defaults: block orElse
                // GLOW_LICHEN, search_range orElse 10, floor/ceiling/wall orElse
                // false, chance_of_spreading orElse 0.5. Only glow_lichen is
                // ported (sculk_vein etc. fail closed).
                const json& cc = cfgJson.at("config");
                const std::string block = "minecraft:" + stripNs(cc.value("block", std::string("minecraft:glow_lichen")));
                if (block != "minecraft:glow_lichen")
                    throw std::runtime_error("multiface block not ported: " + block);
                mc::levelgen::feature::MultifaceLevelHooks hooks;
                hooks.getState = [](BlockPos p) { return g_level->multifaceStateAt(p); };
                hooks.putState = [](BlockPos p, std::uint8_t faces, bool waterlogged) {
                    g_level->putMultifaceState(p, faces, waterlogged);
                };
                placer = mc::levelgen::feature::makeMultifaceGrowthPlacer(
                    loadIdSet(cc.at("can_be_placed_on")),
                    cc.value("search_range", 10),
                    cc.value("can_place_on_floor", false),
                    cc.value("can_place_on_ceiling", false),
                    cc.value("can_place_on_wall", false),
                    cc.value("chance_of_spreading", 0.5f),
                    std::move(hooks));
            } else {
                // Hard no-op: not yet ported. Counted per run at the call site; never "pass".
                g_unportedType[featureKey] = type;
                cache[featureKey] = nullptr;
                return nullptr;
            }
            std::vector<std::shared_ptr<const PlacementModifier>> mods;
            for (const auto& m : placedJson.at("placement")) mods.push_back(loadModifier(m, featureKey));
            result = std::make_shared<PlacedFeature>(placer, mods);
        } catch (const std::exception& e) {
            std::cerr << "FAILED-TO-PORT " << featureKey << ": " << e.what() << "\n";
            cache[featureKey] = nullptr; return nullptr;
        }
        cache[featureKey] = result;
        return result;
    };

    // Parse the server dump: cells[(seed,cx,cz)][(x,y,z)] = block name.
    std::map<std::tuple<long long,int,int>, std::map<std::tuple<int,int,int>, std::string>> server;
    { std::ifstream in(casesPath); if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
      std::string line;
      while (std::getline(in, line)) {
          if (line.empty()) continue;
          std::istringstream ss(line); std::string seed, x, z, y, blk;
          std::getline(ss, seed, '\t'); std::getline(ss, x, '\t'); std::getline(ss, z, '\t'); std::getline(ss, y, '\t'); std::getline(ss, blk, '\t');
          if (blk.empty()) continue;
          const long long s = std::strtoll(seed.c_str(), nullptr, 10);
          const int bx = std::atoi(x.c_str()), bz = std::atoi(z.c_str()), by = std::atoi(y.c_str());
          server[{s, floorDiv(bx,16), floorDiv(bz,16)}][{bx, by, bz}] = blk;
      } }

    const int minY = mc::CHUNK_MIN_Y, maxY = mc::CHUNK_MAX_Y;
    long long oreCells = 0, oreMism = 0, vegCells = 0, vegMism = 0, otherFeatureCells = 0; int shown = 0;
    long long allCells = 0, allMism = 0;
    struct PerChunk { long long vegCells = 0, vegMism = 0, allCells = 0, allMism = 0; };
    std::map<std::tuple<long long,int,int>, PerChunk> perChunk;
    std::map<std::pair<std::string, std::string>, long long> transitions;   // got=>want over all mismatched cells

    for (const auto& [key, cells] : server) {
        const long long seed = std::get<0>(key);
        const int Cx = std::get<1>(key), Cz = std::get<2>(key);

        // 5x5 terrain (inner 3x3 decorated; outer ring for neighbour reads).
        NoiseBasedChunkGenerator gen(static_cast<std::uint64_t>(seed));
        std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>> chunks;
        std::vector<std::vector<BlockPos>> genMarks;   // per-chunk noise+carver marks, generation order
        for (int dz = -2; dz <= 2; ++dz) for (int dx = -2; dx <= 2; ++dx) {
            auto chunk = std::make_unique<mc::LevelChunk>(mc::ChunkPos{ Cx + dx, Cz + dz });
            std::vector<BlockPos> marks;   // NOISE-step marks first, then carver marks
            gen.fillFromNoise(*chunk, &marks); gen.buildSurface(*chunk); gen.applyCarvers(*chunk, &marks);
            genMarks.push_back(std::move(marks));
            chunks.emplace(packChunk(Cx + dx, Cz + dz), std::move(chunk));
        }
        MultiChunkLevel level(&chunks, minY, maxY);
        // The NOISE step's aquifer-fluid marks (NoiseBasedChunkGenerator.java:
        // 442-446) and the carvers' marks (WorldCarver.java:147-158) precede every
        // decoration mark — inject them first, preserving generation order.
        for (const auto& marks : genMarks)
            for (const BlockPos& p : marks) level.markPosForPostprocessing(p);
        // EXACTLY as ChunkStatusTasks.generateFeatures: prime the non-WG heightmaps
        // for every chunk before any decoration runs; they freeze from here on.
        level.freezeHeights();

        // Cached quart-resolution noise biome (matches the section biome containers
        // that ChunkGenerator.applyBiomeDecoration reads for possibleBiomes).
        std::map<std::tuple<int,int,int>, std::string> biomeCache;
        auto nb = [&](int qx, int qy, int qz) -> const std::string& {
            auto k = std::make_tuple(qx, qy, qz);
            auto it = biomeCache.find(k);
            if (it != biomeCache.end()) return it->second;
            return biomeCache.emplace(k, gen.getNoiseBiome(qx, qy, qz)).first->second;
        };

        // The biome filter's level.getBiome goes through the chunk-cached biomes:
        // ChunkAccess.getNoiseBiome CLAMPS quartY to the chunk's section range. For
        // quarts outside the 5x5 grid the Java proxy falls back to the unclamped
        // sampler — replicate both paths.
        const int qyMin = minY >> 2, qyMax = (maxY >> 2) - 1;
        DecoBiomeContext biomeCtx;
        biomeCtx.zoomSeed = BiomeManager::obfuscateSeed(seed);
        biomeCtx.features = &biomeFeatures;
        biomeCtx.noiseBiome = [&, qyMin, qyMax](int qx, int qy, int qz) -> std::string {
            const int scx = qx >> 2, scz = qz >> 2;   // QuartPos.toSection
            const bool inGrid = chunks.count(packChunk(scx, scz)) != 0;
            const int cqy = inGrid ? std::max(qyMin, std::min(qyMax, qy)) : qy;
            return nb(qx, cqy, qz);
        };
        g_biomeCtx = &biomeCtx;
        g_level = &level;

        // Decorate the inner 3x3 in xz order (x outer asc, z inner asc) — the order under
        // which the Java ground truth (FullChunkDecorateParity.java) byte-matches the real
        // server .mca (6/6 primary chunks, commit 2772bdb6). Cross-chunk spill overlap is
        // last-writer-wins, so the order must match the ground truth exactly.
        // NOTE: while only part of the families is ported, mismatch counts vs the FULL
        // ground truth are confounded at borders by unported families overwriting cells;
        // they converge to a true 1:1 measure only as the remaining families are ported.
        // Do not tune the order against these numbers.
        for (int dx = -1; dx <= 1; ++dx) for (int dz = -1; dz <= 1; ++dz) {
            const int nx = Cx + dx, nz = Cz + dz;
            level.setDecorating(nx, nz);

            // possibleBiomes for chunk N = distinct section biomes over the 3x3 around N
            // (ChunkPos.rangeClosed(N,1)) intersected with the overworld possible set.
            std::set<std::string> pbSet;
            for (int ddz = -1; ddz <= 1; ++ddz) for (int ddx = -1; ddx <= 1; ++ddx) {
                const int qx0 = (nx + ddx) * 4, qz0 = (nz + ddz) * 4;
                for (int qy = (minY >> 2); qy < (maxY >> 2); ++qy)
                    for (int qx = qx0; qx < qx0 + 4; ++qx)
                        for (int qz = qz0; qz < qz0 + 4; ++qz) {
                            const std::string& b = nb(qx, qy, qz);
                            if (sourcesSet.count(b)) pbSet.insert(b);
                        }
            }
            const std::vector<std::string> possibleBiomes(pbSet.begin(), pbSet.end());

            WorldgenRandom random(std::make_shared<XoroshiroRandomSource>(static_cast<std::uint64_t>(seed)));
            const std::int64_t deco = random.setDecorationSeed(static_cast<std::int64_t>(seed), nx * 16, nz * 16);
            const int genSteps = std::max(static_cast<int>(stepData.size()), (int)GenerationStep::COUNT);
            for (int step = 0; step < genSteps && step < static_cast<int>(stepData.size()); ++step) {
                const std::vector<int> indices = FeatureSorter::selectFeatureIndicesForStep(possibleBiomes, biomeFeatures, stepData[step], step);
                for (int index : indices) {
                    const std::string& featureKey = stepData[step].features[static_cast<std::size_t>(index)];
                    const std::string normKey = "minecraft:" + stripNs(featureKey);
                    std::shared_ptr<PlacedFeature> placed = resolveFeature(normKey);
                    if (!placed) {   // unported type (hard no-op; reseeded next feature)
                        auto ut = g_unportedType.find(normKey);
                        ++g_unportedSkips[ut != g_unportedType.end() ? ut->second : "load-failed"];
                        continue;
                    }
                    random.setFeatureSeed(deco, index, step);
                    const bool any = placed->place(level, random, BlockPos{ nx * 16, 0, nz * 16 }, minY, maxY - minY);
                    if (step == (int)GenerationStep::VEGETAL_DECORATION) { ++g_vegRuns; if (any) ++g_vegPlacedOk; }
                    else { ++g_oreRuns; if (any) ++g_orePlacedOk; }
                }
            }
        }
        // FULL-promotion post-processing for chunk C (bubble columns above magma,
        // glow_lichen revalidation, water-over-water column writes) — exactly the
        // Java harness postProcessChunk, which the server byte-match certifies.
        const long long skippedFluidTicks = postProcessChunk(level, Cx, Cz);
        if (skippedFluidTicks > 0)
            std::cerr << "POSTPROCESS chunk=(" << Cx << "," << Cz
                      << ") fluid-spread ticks skipped (no ServerLevel): " << skippedFluidTicks << "\n";
        g_biomeCtx = nullptr;
        g_level = nullptr;

        // Compare chunk C: every family cell (server or ours) must match.
        mc::LevelChunk* c = chunks[packChunk(Cx, Cz)].get();
        PerChunk& pc = perChunk[key];
        for (int lx = 0; lx < 16; ++lx) for (int lz = 0; lz < 16; ++lz) {
            const int bx = Cx * 16 + lx, bz = Cz * 16 + lz;
            for (int y = minY; y < maxY; ++y) {
                const std::string mine = level.name(c->getBlock(bx, y, bz));
                auto sit = cells.find({bx, y, bz});
                const std::string srv = (sit != cells.end()) ? sit->second : std::string("minecraft:air");
                ++allCells; ++pc.allCells;
                if (mine != srv) {
                    ++allMism; ++pc.allMism;
                    ++transitions[{ mine, srv }];
                    if (family == "all" && shown++ < 200)
                        std::cerr << "ALL-MISMATCH seed=" << seed << " (" << bx << "," << y << "," << bz
                                  << ") got=" << mine << " server=" << srv << "\n";
                }
                const bool srvOre = oreFamily.count(srv) != 0, myOre = oreFamily.count(mine) != 0;
                const bool srvVeg = vegetalFamily.count(srv) != 0, myVeg = vegetalFamily.count(mine) != 0;
                if (srvOre || myOre) {
                    ++oreCells;
                    if (mine != srv) {
                        ++oreMism;
                        if (family == "ore" && shown++ < 200) std::cerr << "ORE-MISMATCH seed=" << seed << " (" << bx << "," << y << "," << bz
                            << ") got=" << mine << " server=" << srv << "\n";
                    }
                }
                if (srvVeg || myVeg) {
                    ++vegCells; ++pc.vegCells;
                    if (mine != srv) {
                        ++vegMism; ++pc.vegMism;
                        if (family == "vegetal" && shown++ < 200) std::cerr << "VEG-MISMATCH seed=" << seed << " (" << bx << "," << y << "," << bz
                            << ") got=" << mine << " server=" << srv << "\n";
                    }
                }
                if (!srvOre && !myOre && !srvVeg && !myVeg && mine != srv) {
                    ++otherFeatureCells;   // un-ported feature (expected until ported)
                }
            }
        }
    }

    for (const auto& [key, pc] : perChunk)
        std::cout << "DecorateVegetalChunk seed=" << std::get<0>(key) << " chunk=(" << std::get<1>(key) << "," << std::get<2>(key)
                  << ") veg_cells=" << pc.vegCells << " veg_mismatches=" << pc.vegMism << "\n";
    for (const auto& [key, pc] : perChunk)
        std::cout << "DecorateAllChunk seed=" << std::get<0>(key) << " chunk=(" << std::get<1>(key) << "," << std::get<2>(key)
                  << ") cells=" << pc.allCells << " mismatches=" << pc.allMism << "\n";
    std::cout << "DecorateOre ore_cells=" << oreCells << " ore_mismatches=" << oreMism
              << " other_feature_cells=" << otherFeatureCells
              << " | oreRuns=" << g_oreRuns << " orePlacedOk=" << g_orePlacedOk
              << " oreFamilySize=" << oreFamily.size() << "\n";
    std::cout << "DecorateVegetal veg_cells=" << vegCells << " veg_mismatches=" << vegMism
              << " | vegRuns=" << g_vegRuns << " vegPlacedOk=" << g_vegPlacedOk << "\n";
    std::cout << "DecorateAll cells=" << allCells << " mismatches=" << allMism << "\n";
    for (const auto& [t, n] : transitions)
        std::cout << "DecorateAllTransition got=" << t.first << " want=" << t.second << " n=" << n << "\n";
    for (const auto& [type, n] : g_unportedSkips)
        std::cout << "UNPORTED-FEATURE-TYPE " << type << " skipped_placed_features=" << n << "\n";
    if (!g_blocksMotionDefaulted.empty()) {
        std::cout << "BLOCKSMOTION-DEFAULTED (verify each vs Blocks.java and add to the table):";
        for (const auto& b : g_blocksMotionDefaulted) std::cout << " " << b;
        std::cout << "\n";
    }
    if (!mc::levelgen::feature::underwaterMagmaOcclusionDefaulted().empty()) {
        std::cout << "OCCLUSION-DEFAULTED (verify each vs Blocks.java and add to the table):";
        for (const auto& b : mc::levelgen::feature::underwaterMagmaOcclusionDefaulted()) std::cout << " " << b;
        std::cout << "\n";
    }
    if (!mc::levelgen::feature::multifaceAttachDefaulted().empty()) {
        std::cout << "MULTIFACE-ATTACH-DEFAULTED (verify each vs Blocks.java and add to the table):";
        for (const auto& b : mc::levelgen::feature::multifaceAttachDefaulted()) std::cout << " " << b;
        std::cout << "\n";
    }
    const long long famMism = (family == "vegetal") ? vegMism : (family == "all") ? allMism : oreMism;
    return famMism == 0 ? 0 : 1;
}

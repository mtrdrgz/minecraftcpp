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
// Heightmap model (mirrors ChunkStatusTasks.generateFeatures + ProtoChunk):
// WorldGenRegion.getHeight(type,x,z) returns chunk.getHeight()+1 ==
// Heightmap.getFirstAvailable (WorldGenRegion.java:391-393, ChunkAccess.java:182-194).
// At persisted status CARVERS+ the maintained set is ChunkStatus.FINAL_HEIGHTMAPS
// = {OCEAN_FLOOR, WORLD_SURFACE, MOTION_BLOCKING, MOTION_BLOCKING_NO_LEAVES}
// (ChunkStatus.java:18,27): ProtoChunk.setBlockState primes every missing FINAL map
// and live-updates the written column (ProtoChunk.java:147-165) — EXCEPT ore writes,
// which go through BulkSectionAccess raw sections (OreFeature.java:110) and leave
// the maps stale until the chunk's own FEATURES-turn re-prime (generateFeatures).
// The *_WG pair freezes at its post-carver values for the whole FEATURES phase.
// Heightmap predicates (Heightmap.java:147-156): WORLD_SURFACE* = !isAir,
// OCEAN_FLOOR* = blocksMotion, MOTION_BLOCKING = blocksMotion || fluid,
// MOTION_BLOCKING_NO_LEAVES additionally excludes LeavesBlock.

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
#include "TreeFeature.h"
#include "FallenTreeFeature.h"
#include "SimpleBlockFeature.h"
#include "RandomSelectorFeature.h"
#include "MonsterRoomFeature.h"
#include "SnowAndFreezeFeature.h"
#include "LakeFeature.h"
#include "BlockColumnFeature.h"
#include "SpikeFeature.h"
#include "BlockBlobFeature.h"
#include "GeodeFeature.h"
#include "DesertWellFeature.h"
#include "CaveFeatures.h"
#include "DripstoneFeatures.h"
#include "HugeMushroomFeatures.h"
#include "BambooFeature.h"
#include "IcebergFeatures.h"
#include "CoralFeatures.h"
#include "SculkFeatures.h"
#include "FossilFeature.h"
#include "../FloatProvider.h"
#include "EngineDecoration.h"
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
#include <filesystem>
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

[[maybe_unused]] void installBlockStatesEnv() {   // parity-main only (unused in MCPP_DECORATE_NO_MAIN builds)
    if (std::getenv("MCPP_BLOCK_STATES")) return;
    for (const char* p : { "src/assets/block_states.json", "src/assets/block_states.json" }) {
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
[[maybe_unused]] std::string findDataDir() {      // parity-main only (unused in MCPP_DECORATE_NO_MAIN builds)
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
std::set<std::string> g_solidRenderDefaulted;        // isSolidRender defaults (must be empty)

// Thrown by the configured-feature loader for a clean "type not yet ported"
// (recorded as a hard no-op, not a load failure).
struct UnportedFeatureType {
    std::string type;
};
std::string stateName(const json& s) { return s.at("Name").get<std::string>(); }

// Per-state-id heightmap predicate flags, memoised (the scans visit every block
// of every column; see BlockBehaviour.h for the Java-grounded tables).
struct StateFlags {
    bool isAir;          // BlockStateBase.isAir (air / cave_air / void_air)
    bool blocksMotion;   // BlockStateBase.blocksMotion
    bool hasFluid;       // !getFluidState().isEmpty() (block-id fluid model, Fluids.h)
    bool isLeaves;       // instanceof LeavesBlock (BlockBehaviour.h isLeavesBlock)
};
const StateFlags& stateFlagsForId(std::uint32_t id) {
    static std::vector<std::optional<StateFlags>> memo;
    if (id >= memo.size()) memo.resize(id + 1);
    if (!memo[id].has_value()) {
        const mc::BlockState* s = mc::getBlockState(id);
        const std::string name = (s && s->block && !s->block->name.empty())
            ? "minecraft:" + s->block->name : std::string("minecraft:air");
        bool defaulted = false;
        StateFlags f;
        f.isAir = mc::block::isAirBlock(name);
        f.blocksMotion = mc::block::blocksMotion(name, &defaulted);
        f.hasFluid = !mc::material::fluidStateOf(name).isEmpty();
        f.isLeaves = mc::block::isLeavesBlock(name);
        if (defaulted) g_blocksMotionDefaulted.insert(name);
        memo[id] = f;
    }
    return *memo[id];
}
// Heightmap.Types isOpaque predicates (Heightmap.java:147-156):
//   WORLD_SURFACE*          NOT_AIR = !isAir
//   OCEAN_FLOOR*            MATERIAL_MOTION_BLOCKING = blocksMotion
//   MOTION_BLOCKING         blocksMotion || !fluid.isEmpty()
//   MOTION_BLOCKING_NO_LEAVES  (blocksMotion || fluid) && !(block instanceof LeavesBlock)
bool heightmapOpaque(Heightmap::Types type, std::uint32_t id) {
    const StateFlags& f = stateFlagsForId(id);
    switch (type) {
        case Heightmap::Types::WORLD_SURFACE_WG:
        case Heightmap::Types::WORLD_SURFACE: return !f.isAir;
        case Heightmap::Types::OCEAN_FLOOR_WG:
        case Heightmap::Types::OCEAN_FLOOR: return f.blocksMotion;
        case Heightmap::Types::MOTION_BLOCKING: return f.blocksMotion || f.hasFluid;
        case Heightmap::Types::MOTION_BLOCKING_NO_LEAVES:
            return (f.blocksMotion || f.hasFluid) && !f.isLeaves;
    }
    return false;
}

// ---- per-case biome lookup for the minecraft:biome placement filter ----
// BiomeFilter.shouldPlace (BiomeFilter.java:21-26): biome = level.getBiome(pos)
// (the BiomeManager fuzzed lookup over the chunk-cached quart biomes), then
// generator.getBiomeGenerationSettings(biome).hasFeature(placedFeature).
struct DecoBiomeContext {
    std::int64_t zoomSeed = 0;                                  // BiomeManager.obfuscateSeed(seed)
    std::function<std::string(int, int, int)> noiseBiome;       // quart -> biome (chunk-cache clamped)
    const BiomeFeatures* features = nullptr;

    // WorldGenRegion.getBiome(pos) == BiomeManager zoomed lookup over the
    // chunk-cached quart biomes (the same resolution the surface/biome filter use).
    std::string zoomBiome(mc::BlockPos pos) const {
        const std::array<int, 3> q = BiomeManager::debugSelectQuart(zoomSeed, pos.x, pos.y, pos.z);
        return noiseBiome(q[0], q[1], q[2]);
    }

    bool hasFeatureAt(const std::string& featureKey, mc::BlockPos pos) const {
        const std::string biome = zoomBiome(pos);
        // BiomeGenerationSettings.hasFeature: the flattened feature set over all steps.
        for (int step = 0; step < GenerationStep::COUNT; ++step)
            if (features->biomeHasFeature(biome, step, featureKey)) return true;
        return false;
    }
};
const DecoBiomeContext* g_biomeCtx = nullptr;   // set per case before decorating

// Per-biome climate settings (Biome.ClimateSettings codec: has_precipitation,
// temperature, optional temperature_modifier), loaded once from the biome JSONs;
// fail-closed lookup for freeze_top_layer / lake freeze.
std::map<std::string, mc::levelgen::feature::BiomeClimate> g_biomeClimate;
long long g_curLevelSeed = 0;                   // level.getSeed() for the running case
long long g_skippedScheduleTicks = 0;           // scheduleTick: needs a ServerLevel (hard no-op, counted)

// The per-case level, reachable from cached feature placers (the multiface side
// map and postprocess marks live on it). Set/cleared alongside g_biomeCtx.
class MultiChunkLevel;
MultiChunkLevel* g_level = nullptr;
// Debug attribution: the placed feature currently running + decorating chunk
// (MCPP_WATCH=x,y,z prints every write at that cell with its owner).
std::string g_curFeatureKey = "?";
int g_curTurnCx = 0, g_curTurnCz = 0;

// WorldGenRegion.getRandom() (WorldGenRegion.java:69,86,386-388): one stateful
// RandomSource per decoration turn, from the "minecraft:worldgen_region_random"
// positional factory at the decorating chunk's getWorldPosition() (minBlockX, 0,
// minBlockZ). Reset at every turn start, exactly as the Java GT's REGION_RANDOM
// (FullChunkDecorateParity.java:646). Sole worldgen consumer: MossyCarpetBlock
// .placeAt's topper nextBoolean draws (SimpleBlockFeature.java:33-35).
std::shared_ptr<RandomSource> g_regionRandom;

// ---- minimal loaders (throw on anything else: fail-closed) ----
IntProviderPtr loadIntProvider(const json& j) {
    if (j.is_number_integer()) return ConstantInt::of(j.get<int>());
    const std::string t = stripNs(j.at("type").get<std::string>());
    if (t == "constant") return ConstantInt::of(j.at("value").get<int>());
    if (t == "uniform") return UniformInt::of(j.at("min_inclusive").get<int>(), j.at("max_inclusive").get<int>());
    if (t == "biased_to_bottom") return BiasedToBottomInt::of(j.at("min_inclusive").get<int>(), j.at("max_inclusive").get<int>());
    if (t == "clamped") return ClampedInt::of(loadIntProvider(j.at("source")), j.at("min_inclusive").get<int>(), j.at("max_inclusive").get<int>());
    if (t == "trapezoid") return TrapezoidInt::of(j.at("min").get<int>(), j.at("max").get<int>(), j.value("plateau", 0));
    // ClampedNormalInt.sample (ClampedNormalInt.java:31-37):
    // (int)Mth.clamp(Mth.normal(random, mean, deviation), min, max) — one
    // nextGaussian draw.
    if (t == "clamped_normal") {
        struct ClampedNormalInt final : public mc::valueproviders::IntProvider {
            float mean, deviation, minInclusiveF, maxInclusiveF;
            ClampedNormalInt(float m, float d, float mn, float mx) : mean(m), deviation(d), minInclusiveF(mn), maxInclusiveF(mx) {}
            std::int32_t sample(RandomSource& random) const override {
                const float v = mc::valueproviders::mthClampF(
                    mc::valueproviders::mthNormal(random, mean, deviation), minInclusiveF, maxInclusiveF);
                return static_cast<std::int32_t>(v);
            }
            std::int32_t minInclusive() const override { return static_cast<std::int32_t>(minInclusiveF); }
            std::int32_t maxInclusive() const override { return static_cast<std::int32_t>(maxInclusiveF); }
        };
        return std::make_shared<ClampedNormalInt>(
            j.at("mean").get<float>(), j.at("deviation").get<float>(),
            static_cast<float>(j.at("min_inclusive").get<int>()), static_cast<float>(j.at("max_inclusive").get<int>()));
    }
    // WeightedListInt (WeightedListInt.java): distribution entries {data: IntProvider,
    // weight}; one nextInt(totalWeight) then the chosen provider samples.
    if (t == "weighted_list") {
        std::vector<WeightedListInt::Entry> dist;
        for (const auto& e : j.at("distribution"))
            dist.push_back({ loadIntProvider(e.at("data")), e.value("weight", 1) });
        return std::make_shared<WeightedListInt>(std::move(dist));
    }
    throw std::runtime_error("unsupported int_provider: " + t);
}
// FloatProvider codecs: a bare number is a constant (FloatProviderType dispatch
// falls back to the inline constant), else {type: uniform|clamped_normal|trapezoid}.
mc::valueproviders::FloatProviderPtr loadFloatProvider(const json& j) {
    using namespace mc::valueproviders;
    if (j.is_number()) return ConstantFloat::of(j.get<float>());
    const std::string t = stripNs(j.at("type").get<std::string>());
    if (t == "constant") return ConstantFloat::of(j.at("value").get<float>());
    if (t == "uniform") return UniformFloat::of(j.at("min_inclusive").get<float>(), j.at("max_exclusive").get<float>());
    if (t == "clamped_normal") return ClampedNormalFloat::of(
        j.at("mean").get<float>(), j.at("deviation").get<float>(), j.at("min").get<float>(), j.at("max").get<float>());
    if (t == "trapezoid") return TrapezoidFloat::of(j.at("min").get<float>(), j.at("max").get<float>(), j.at("plateau").get<float>());
    throw std::runtime_error("unsupported float_provider: " + t);
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
// matching_fluids (MatchingFluidsPredicate.java), matching_block_tag
// (MatchingBlockTagPredicate.java) — StateTestingPredicate subtypes testing the
// state at origin+offset (offset default 0,0,0) — plus not (NotPredicate.java)
// and would_survive (WouldSurvivePredicate.java: state.canSurvive(level, origin+offset)).
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
            // MatchingFluidsPredicate (MatchingFluidsPredicate.java): state
            // .getFluidState().is(holderSet). The EMPTY fluid state's type IS
            // minecraft:empty — a "minecraft:empty" member set passes exactly on
            // fluid-free cells (patch_melon/patch_pumpkin gates).
            return member(fs.isEmpty() ? "minecraft:empty" : fs.fluid);
        };
    }
    if (t == "matching_block_tag") {
        const std::string tag = "minecraft:" + stripNs(j.at("tag").get<std::string>());
        return [tag, off](WorldGenLevel& level, BlockPos p) {
            return g_tags->isInTag(
                mc::block::blockName(level.getBlockState(BlockPos{ p.x + off[0], p.y + off[1], p.z + off[2] })), tag);
        };
    }
    if (t == "not") {
        auto inner = loadBlockPredicate(j.at("predicate"));
        return [inner](WorldGenLevel& level, BlockPos p) { return !inner(level, p); };
    }
    // ReplaceablePredicate (ReplaceablePredicate.java:16-18): state.canBeReplaced()
    // at origin+offset — the replaceable property == the #replaceable tag.
    if (t == "replaceable") {
        return [off](WorldGenLevel& level, BlockPos p) {
            return g_tags->isInTag(
                mc::block::blockName(level.getBlockState(BlockPos{ p.x + off[0], p.y + off[1], p.z + off[2] })),
                "minecraft:replaceable");
        };
    }
    // AllOfPredicate / AnyOfPredicate (CombiningPredicate subtypes): short-circuit
    // conjunction/disjunction over "predicates".
    if (t == "all_of" || t == "any_of") {
        std::vector<std::function<bool(WorldGenLevel&, BlockPos)>> preds;
        for (const auto& pj : j.at("predicates")) preds.push_back(loadBlockPredicate(pj));
        const bool isAll = t == "all_of";
        return [preds, isAll](WorldGenLevel& level, BlockPos p) {
            for (const auto& pred : preds) {
                if (pred(level, p) != isAll) return !isAll;
            }
            return isAll;
        };
    }
    if (t == "would_survive") {
        const std::string state = stateName(j.at("state"));
        return [state, off](WorldGenLevel& level, BlockPos p) {
            return level.canSurvive(state, BlockPos{ p.x + off[0], p.y + off[1], p.z + off[2] });
        };
    }
    // HasSturdyFacePredicate (HasSturdyFacePredicate.java): the state at
    // origin+offset isFaceSturdy(direction).
    if (t == "has_sturdy_face") {
        const std::string d = j.at("direction").get<std::string>();
        int dirIdx;
        if (d == "down") dirIdx = 0; else if (d == "up") dirIdx = 1;
        else if (d == "north") dirIdx = 2; else if (d == "south") dirIdx = 3;
        else if (d == "west") dirIdx = 4; else if (d == "east") dirIdx = 5;
        else throw std::runtime_error("has_sturdy_face: bad direction " + d);
        return [off, dirIdx](WorldGenLevel& level, BlockPos p) {
            const std::string st = level.getBlockState(BlockPos{ p.x + off[0], p.y + off[1], p.z + off[2] });
            bool defaulted = false;
            const bool r = mc::block::isFaceSturdyFull(st, dirIdx, &defaulted);
            if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(st));
            return r;
        };
    }
    // SolidPredicate (SolidPredicate.java:17-19): state.isSolid() at origin+offset.
    if (t == "solid") {
        return [off](WorldGenLevel& level, BlockPos p) {
            bool defaulted = false;
            const bool r = mc::block::isSolid(
                level.getBlockState(BlockPos{ p.x + off[0], p.y + off[1], p.z + off[2] }), &defaulted);
            if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(
                level.getBlockState(BlockPos{ p.x + off[0], p.y + off[1], p.z + off[2] })));
            return r;
        };
    }
    // InsideWorldBoundsPredicate (InsideWorldBoundsPredicate.java:19-21):
    // level.isInsideBuildHeight(pos + offset) — minY <= y < maxY.
    if (t == "inside_world_bounds") {
        return [off](WorldGenLevel&, BlockPos p) {
            const int y = p.y + off[1];
            return y >= mc::CHUNK_MIN_Y && y < mc::CHUNK_MAX_Y;
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
    // WeightedStateProvider.getState (WeightedStateProvider.java:36-39) ->
    // WeightedList.getRandomOrThrow (WeightedList.java:75-82): one
    // nextInt(totalWeight), then the cumulative-weight pick in entry order
    // (matches both the Flat and Compact selectors). Properties on the entries
    // (leaf_litter facing/segment_amount, mushroom none) are id-invisible.
    if (t == "weighted_state_provider") {
        struct Entry { std::string state; int weight; };
        auto entries = std::make_shared<std::vector<Entry>>();
        int total = 0;
        for (const auto& e : j.at("entries")) {
            const int w = e.value("weight", 1);
            entries->push_back({ stateName(e.at("data")), w });
            total += w;
        }
        if (total <= 0) throw std::runtime_error("weighted_state_provider: empty");
        return [entries, total](WorldGenLevel&, RandomSource& random, BlockPos) -> std::optional<std::string> {
            int selection = random.nextInt(total);
            for (const Entry& e : *entries) {
                selection -= e.weight;
                if (selection < 0) return e.state;
            }
            throw std::logic_error("weighted_state_provider: selection exceeded total weight");
        };
    }
    // NoiseThresholdProvider.getState (NoiseThresholdProvider.java:58-66) over
    // NoiseBasedStateProvider's noise = NormalNoise.create(WorldgenRandom(
    // LegacyRandomSource(seed)), parameters) and getNoiseValue(pos, scale) =
    // noise.getValue(x*scale, y*scale, z*scale) (NoiseBasedStateProvider.java:28-38).
    // value < threshold -> Util.getRandom(lowStates) (one nextInt); else ONE
    // nextFloat: < highChance -> Util.getRandom(highStates) else defaultState.
    if (t == "noise_threshold_provider") {
        const long long seed = j.at("seed").get<long long>();
        const json& nj = j.at("noise");
        mc::levelgen::NoiseParameters params;
        params.firstOctave = nj.at("firstOctave").get<int>();
        for (const auto& a : nj.at("amplitudes")) params.amplitudes.push_back(a.get<double>());
        const float scale = j.at("scale").get<float>();
        const float threshold = j.at("threshold").get<float>();
        const float highChance = j.at("high_chance").get<float>();
        const std::string defaultState = stateName(j.at("default_state"));
        auto lowStates = std::make_shared<std::vector<std::string>>();
        for (const auto& s : j.at("low_states")) lowStates->push_back(stateName(s));
        auto highStates = std::make_shared<std::vector<std::string>>();
        for (const auto& s : j.at("high_states")) highStates->push_back(stateName(s));
        if (lowStates->empty() || highStates->empty()) throw std::runtime_error("noise_threshold_provider: empty state list");
        auto noise = std::make_shared<mc::levelgen::NormalNoise>([&] {
            mc::levelgen::WorldgenRandom random(std::make_shared<mc::levelgen::LegacyRandomSource>(seed));
            return mc::levelgen::NormalNoise::create(random, params);
        }());
        return [noise, scale, threshold, highChance, defaultState, lowStates, highStates](
                   WorldGenLevel&, RandomSource& random, BlockPos pos) -> std::optional<std::string> {
            const double s = static_cast<double>(scale);
            const double localValue = noise->getValue(pos.x * s, pos.y * s, pos.z * s);
            if (localValue < static_cast<double>(threshold)) {
                return (*lowStates)[static_cast<std::size_t>(random.nextInt(static_cast<int>(lowStates->size())))];
            }
            return random.nextFloat() < highChance
                ? (*highStates)[static_cast<std::size_t>(random.nextInt(static_cast<int>(highStates->size())))]
                : defaultState;
        };
    }
    // NoiseProvider.getState (NoiseProvider.java:38-52): pick states[(int)
    // (clamp((1+noise)/2, 0, 0.9999) * size)] — NO RNG draws.
    if (t == "noise_provider") {
        const long long seed = j.at("seed").get<long long>();
        const json& nj = j.at("noise");
        mc::levelgen::NoiseParameters params;
        params.firstOctave = nj.at("firstOctave").get<int>();
        for (const auto& a : nj.at("amplitudes")) params.amplitudes.push_back(a.get<double>());
        const float scale = j.at("scale").get<float>();
        auto states = std::make_shared<std::vector<std::string>>();
        for (const auto& e : j.at("states")) states->push_back(stateName(e));
        if (states->empty()) throw std::runtime_error("noise_provider: empty states");
        auto noise = std::make_shared<mc::levelgen::NormalNoise>([&] {
            mc::levelgen::WorldgenRandom random(std::make_shared<mc::levelgen::LegacyRandomSource>(seed));
            return mc::levelgen::NormalNoise::create(random, params);
        }());
        return [noise, scale, states](WorldGenLevel&, RandomSource&, BlockPos pos) -> std::optional<std::string> {
            const double sc = static_cast<double>(scale);
            const double noiseValue = noise->getValue(pos.x * sc, pos.y * sc, pos.z * sc);
            double placementValue = (1.0 + noiseValue) / 2.0;                       // Mth.clamp(.., 0, 0.9999)
            placementValue = placementValue < 0.0 ? 0.0 : std::min(placementValue, 0.9999);
            return (*states)[static_cast<std::size_t>(placementValue * static_cast<double>(states->size()))];
        };
    }
    // DualNoiseProvider.getState (DualNoiseProvider.java:56-72): NO RNG draws.
    //   varietyNoise = slowNoise.getValue(x*slowScale, y*slowScale, z*slowScale)
    //     (getSlowNoiseValue:70-72 — slowScale is a FLOAT field, so the coordinate
    //      products happen in float precision before widening to double);
    //   localVariety = (int)Mth.clampedMap(varietyNoise, -1, 1, variety.min,
    //     variety.max + 1)  (Mth.java:636-638 clampedMap = clampedLerp(inverseLerp);
    //     :109-115 clampedLerp; :326-328 inverseLerp);
    //   for i in [0, localVariety): pick from `states` by the SLOW noise at
    //     pos.offset(i*54545, 0, i*34234) via NoiseProvider.getRandomState
    //     (NoiseProvider.java:49-52: states[(int)(clamp((1+v)/2, 0, 0.9999)*size)]);
    //   final pick from that list by the FAST noise at pos with `scale`
    //     (NoiseProvider.getRandomState(list, pos, scale):44-47, double products).
    // Both noises share the SAME seed: NormalNoise.create(WorldgenRandom(
    // LegacyRandomSource(seed)), params) (DualNoiseProvider.java:48,
    // NoiseBasedStateProvider.java:33).
    if (t == "dual_noise_provider") {
        const long long seed = j.at("seed").get<long long>();
        auto loadParams = [](const json& nj) {
            mc::levelgen::NoiseParameters p;
            p.firstOctave = nj.at("firstOctave").get<int>();
            for (const auto& a : nj.at("amplitudes")) p.amplitudes.push_back(a.get<double>());
            return p;
        };
        const mc::levelgen::NoiseParameters params = loadParams(j.at("noise"));
        const mc::levelgen::NoiseParameters slowParams = loadParams(j.at("slow_noise"));
        const float scale = j.at("scale").get<float>();
        const float slowScale = j.at("slow_scale").get<float>();
        // InclusiveRange.codec(Codec.INT, 1, 64): either [min, max] or
        // {"min_inclusive": .., "max_inclusive": ..} (InclusiveRange.java codec).
        int varietyMin, varietyMax;
        const json& vj = j.at("variety");
        if (vj.is_array() && vj.size() == 2) {
            varietyMin = vj[0].get<int>();
            varietyMax = vj[1].get<int>();
        } else if (vj.is_object()) {
            varietyMin = vj.at("min_inclusive").get<int>();
            varietyMax = vj.at("max_inclusive").get<int>();
        } else {
            throw std::runtime_error("dual_noise_provider: malformed variety");
        }
        if (varietyMin < 1 || varietyMax > 64 || varietyMin > varietyMax)
            throw std::runtime_error("dual_noise_provider: variety out of [1,64]");
        auto states = std::make_shared<std::vector<std::string>>();
        for (const auto& e : j.at("states")) states->push_back(stateName(e));
        if (states->empty()) throw std::runtime_error("dual_noise_provider: empty states");
        auto makeNoise = [seed](const mc::levelgen::NoiseParameters& p) {
            mc::levelgen::WorldgenRandom random(std::make_shared<mc::levelgen::LegacyRandomSource>(seed));
            return std::make_shared<mc::levelgen::NormalNoise>(mc::levelgen::NormalNoise::create(random, p));
        };
        auto noise = makeNoise(params);
        auto slowNoise = makeNoise(slowParams);
        return [noise, slowNoise, scale, slowScale, varietyMin, varietyMax, states](
                   WorldGenLevel&, RandomSource&, BlockPos pos) -> std::optional<std::string> {
            // NoiseProvider.getRandomState(states, noiseValue) (NoiseProvider.java:49-52)
            auto pickByNoise = [](const std::vector<std::string>& list, double v) -> const std::string& {
                double placementValue = (1.0 + v) / 2.0;
                placementValue = placementValue < 0.0 ? 0.0 : std::min(placementValue, 0.9999);
                return list[static_cast<std::size_t>(placementValue * static_cast<double>(list.size()))];
            };
            // getSlowNoiseValue (DualNoiseProvider.java:70-72): float products.
            auto slowValueAt = [&](BlockPos p) {
                return slowNoise->getValue(
                    static_cast<double>(static_cast<float>(p.x) * slowScale),
                    static_cast<double>(static_cast<float>(p.y) * slowScale),
                    static_cast<double>(static_cast<float>(p.z) * slowScale));
            };
            const double varietyNoise = slowValueAt(pos);
            // Mth.clampedMap(varietyNoise, -1, 1, min, max+1)
            const double factor = (varietyNoise - (-1.0)) / (1.0 - (-1.0));         // inverseLerp
            const double toMin = static_cast<double>(varietyMin);
            const double toMax = static_cast<double>(varietyMax) + 1.0;
            double mapped;
            if (factor < 0.0) mapped = toMin;
            else if (factor > 1.0) mapped = toMax;
            else mapped = toMin + factor * (toMax - toMin);                          // clampedLerp
            const int localVariety = static_cast<int>(mapped);
            std::vector<std::string> possibleStates;
            possibleStates.reserve(static_cast<std::size_t>(localVariety));
            for (int i = 0; i < localVariety; ++i) {
                const BlockPos off{ pos.x + i * 54545, pos.y, pos.z + i * 34234 };
                possibleStates.push_back(pickByNoise(*states, slowValueAt(off)));
            }
            const double sc = static_cast<double>(scale);                            // double products
            const double fast = noise->getValue(pos.x * sc, pos.y * sc, pos.z * sc);
            return pickByNoise(possibleStates, fast);
        };
    }
    // RandomizedIntStateProvider.getState (RandomizedIntStateProvider.java:58-70):
    // source.getState first, then values.sample(random) — the DRAW HAPPENS even
    // though the int property (cave_vines AGE) is id-invisible.
    if (t == "randomized_int_state_provider") {
        auto source = std::make_shared<mc::levelgen::feature::DiskStateProvider>(loadStateProvider(j.at("source")));
        auto values = loadIntProvider(j.at("values"));
        return [source, values](WorldGenLevel& level, RandomSource& random, BlockPos pos) -> std::optional<std::string> {
            std::optional<std::string> state = (*source)(level, random, pos);
            (void)values->sample(random);   // setValue(property, sample): id unchanged
            return state;
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
    if (t == "biome") {
        if (!featureKey.empty() && featureKey[0] == '?')
            throw std::runtime_error("biome filter inside a nested placed feature (no registry key)");
        return std::make_shared<BiomeFilter>(
            [featureKey](WorldGenLevel&, BlockPos pos) { return g_biomeCtx->hasFeatureAt(featureKey, pos); });
    }
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
    if (t == "surface_water_depth_filter")
        return std::make_shared<SurfaceWaterDepthFilter>(j.at("max_water_depth").get<int>());
    // RandomOffsetPlacement.getPositions (RandomOffsetPlacement.java:48-53):
    // xz_spread sampled for X, then y_spread, then xz_spread again for Z.
    if (t == "random_offset")
        return std::make_shared<RandomOffsetPlacement>(loadIntProvider(j.at("xz_spread")), loadIntProvider(j.at("y_spread")));
    // NoiseThresholdCountPlacement.count (NoiseThresholdCountPlacement.java:33-37):
    // BIOME_INFO_NOISE at (x/200, z/200) < noise_level ? below_noise : above_noise.
    if (t == "noise_threshold_count")
        return std::make_shared<NoiseThresholdCountPlacement>(
            j.at("noise_level").get<double>(), j.at("below_noise").get<int>(), j.at("above_noise").get<int>());
    // EnvironmentScanPlacement (EnvironmentScanPlacement.java:48-71); the codec's
    // allowed_search_condition defaults to BlockPredicate.alwaysTrue() (:22).
    if (t == "environment_scan") {
        const std::string dir = j.at("direction_of_search").get<std::string>();
        if (dir != "up" && dir != "down") throw std::runtime_error("environment_scan: bad direction " + dir);
        auto target = loadBlockPredicate(j.at("target_condition"));
        std::function<bool(WorldGenLevel&, BlockPos)> allowed;
        if (j.contains("allowed_search_condition")) allowed = loadBlockPredicate(j.at("allowed_search_condition"));
        else allowed = [](WorldGenLevel&, BlockPos) { return true; };
        return std::make_shared<EnvironmentScanPlacement>(
            dir == "up" ? 1 : -1, std::move(target), std::move(allowed),
            j.at("max_steps").get<int>(), mc::CHUNK_MIN_Y, mc::CHUNK_MAX_Y);
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

    // ChunkStatusTasks.generateFeatures / the Java GT decorate() (FullChunkDecorate-
    // Parity.java:533-536): at the START of a chunk's FEATURES turn, prime (OVERWRITE)
    // its four FINAL heightmaps. They are LIVE afterwards (see getHeight); this
    // re-prime resyncs columns staled by heightmap-BYPASSING bulk writes (ores).
    void primeNonWgHeightmaps(int cx, int cz) {
        for (int t = 0; t < 4; ++t) primeType(cx, cz, t);
    }
    // The *_WG pair stops live-updating once the chunk's persisted status reaches
    // CARVERS (ChunkStatus.CARVERS uses FINAL_HEIGHTMAPS, ChunkStatus.java:18,27;
    // ProtoChunk.setBlockState only updates getPersistedStatus().heightmapsAfter(),
    // ProtoChunk.java:147-165). Decoration therefore reads the post-carver TERRAIN
    // snapshot for *_WG — snapshot it now (called once before any decoration).
    //
    // freezeChunkHeights is the per-chunk unit (the engine streaming model snapshots
    // each chunk at generation time, right after its carvers, before the chunk is
    // reachable by any decoration); the harness freezeHeights() loops it over the
    // whole generated grid, identical behaviour.
    void freezeChunkHeights(mc::LevelChunk* c, int cx, int cz) {
        auto& wg = m_wgSnapshot[packChunk(cx, cz)];
        for (int i = 0; i < 256; ++i) {
            const int x = cx * 16 + (i & 15), z = cz * 16 + (i >> 4);
            wg.oceanFloorWg[static_cast<std::size_t>(i)] = scan(c, x, z, Heightmap::Types::OCEAN_FLOOR_WG);
            wg.worldSurfaceWg[static_cast<std::size_t>(i)] = scan(c, x, z, Heightmap::Types::WORLD_SURFACE_WG);
        }
    }
    void freezeHeights() {
        for (auto& [key, chunkPtr] : *m_chunks) {
            const int cx = static_cast<int>(key >> 32);
            const int cz = static_cast<int>(static_cast<std::int32_t>(key & 0xffffffff));
            freezeChunkHeights(chunkPtr.get(), cx, cz);
        }
    }
    // Engine chunk (RE)generation at coordinates seen before (unload + regenerate):
    // drop the per-chunk state of the previous generation. A no-op in the batch
    // harness (each case builds a fresh level; nothing exists before freeze).
    void resetChunkGenState(int cx, int cz) {
        const std::int64_t key = packChunk(cx, cz);
        m_wgSnapshot.erase(key);
        m_heightmaps.erase(key);
        m_marks.erase(key);
    }

    int getMinY() const override { return m_minY; }

    // WorldGenRegion.getHeight (WorldGenRegion.java:391-393): chunk heightmap value
    // + 1 == Heightmap.getFirstAvailable.
    //   *_WG: FROZEN post-carver snapshot (see freezeHeights).
    //   OCEAN_FLOOR / WORLD_SURFACE / MOTION_BLOCKING / MOTION_BLOCKING_NO_LEAVES
    //   (ChunkStatus.FINAL_HEIGHTMAPS): primed lazily on first touch (ChunkAccess.
    //   getHeight primes the read type, ChunkAccess.java:182-194; ProtoChunk.
    //   setBlockState primes every missing FINAL type, ProtoChunk.java:147-160) and
    //   LIVE-updated by every non-bulk write (Heightmap.update — equivalent to a
    //   fresh column scan of the current blocks).
    int getHeight(Heightmap::Types type, int x, int z) const override {
        const int cx = floorDiv(x, 16), cz = floorDiv(z, 16);
        mc::LevelChunk* c = at(cx, cz);
        if (!c) return m_minY;   // empty column: getFirstAvailable == minY
        const int idx = ((z - cz * 16) << 4) | (x - cx * 16);
        switch (type) {
            case Heightmap::Types::WORLD_SURFACE_WG:
            case Heightmap::Types::OCEAN_FLOOR_WG: {
                auto it = m_wgSnapshot.find(packChunk(cx, cz));
                if (it == m_wgSnapshot.end()) throw std::logic_error("WG heightmap read before freezeHeights()");
                return (type == Heightmap::Types::OCEAN_FLOOR_WG
                            ? it->second.oceanFloorWg[static_cast<std::size_t>(idx)]
                            : it->second.worldSurfaceWg[static_cast<std::size_t>(idx)]) + 1;
            }
            default: {
                const int t = nonWgIndex(type);
                auto& maps = m_heightmaps[packChunk(cx, cz)];
                if (!maps[t].has_value()) const_cast<MultiChunkLevel*>(this)->primeType(cx, cz, t);
                return (*maps[t])[static_cast<std::size_t>(idx)] + 1;
            }
        }
    }
    // BulkSectionAccess writes (OreFeature.java:110) bypass ProtoChunk.setBlockState
    // and thus never prime/update heightmaps; toggled by the ore placer wrapper.
    void setBulkWriting(bool v) { m_bulkWriting = v; }
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
        if (const char* w = std::getenv("MCPP_WATCH")) {
            int wx, wy, wz;
            if (std::sscanf(w, "%d,%d,%d", &wx, &wy, &wz) == 3 && wx == p.x && wy == p.y && wz == p.z)
                std::cerr << "WATCH-PUT turn=" << g_curTurnCx << "," << g_curTurnCz
                          << " " << g_curFeatureKey << " -> " << state << "\n";
        }
        if (const char* tw = std::getenv("MCPP_TRACE_WRITES");
            tw != nullptr && g_curFeatureKey.find(tw) != std::string::npos) {
            std::cerr << "PUT\t" << g_curTurnCx << "," << g_curTurnCz << "\t" << g_curFeatureKey
                      << "\t" << p.x << "\t" << p.y << "\t" << p.z << "\t" << mc::block::blockName(state) << "\n";
        }
        // The grid stores block ids (default states); properties ([half=upper],
        // [age=N]) are not byte-compared (the server dump records block ids).
        // glow_lichen face/waterlogged bits live in the multiface side map: any
        // overwrite invalidates the entry (the multiface port re-registers its own).
        const std::string block = mc::block::blockName(state);
        c->setBlock(p.x, p.y, p.z, mc::getDefaultBlockStateId(stripNs(block), m_airId));
        m_multiface.erase(std::make_tuple(p.x, p.y, p.z));
        // pale_moss_carpet BASE/side properties live in the carpet side map: any
        // overwrite invalidates the entry (the carpet placeAt re-registers its own).
        m_carpet.erase(std::make_tuple(p.x, p.y, p.z));
        // Leaf DISTANCE side map: tree foliage providers place distance=7 leaves
        // (the configured foliage states); TreeFeature.updateLeaves rewrites the
        // property via setLeafDistance. Overwrites drop the record.
        if (mc::block::isLeavesBlock(block)) m_leafDistance[std::make_tuple(p.x, p.y, p.z)] = 7;
        else m_leafDistance.erase(std::make_tuple(p.x, p.y, p.z));
        // Vine face side map (TrunkVineDecorator.placeVine sets exactly one face;
        // the placer registers it via the putVineFaces hook after this write).
        if (block != "minecraft:vine") m_vineFaces.erase(std::make_tuple(p.x, p.y, p.z));
        // FACING side map (cocoa / coral wall fans): drop on overwrite; the placer
        // re-registers its own after a successful write.
        m_facing.erase(std::make_tuple(p.x, p.y, p.z));
        // ProtoChunk.setBlockState heightmap maintenance (ProtoChunk.java:147-165):
        // prime every missing FINAL heightmap of this chunk, then update the written
        // column — column recompute == Heightmap.update on the current state. Bulk
        // (BulkSectionAccess) writes skip all of it.
        if (!m_bulkWriting) {
            const int cx = floorDiv(p.x, 16), cz = floorDiv(p.z, 16);
            auto& maps = m_heightmaps[packChunk(cx, cz)];
            const int idx = ((p.z - cz * 16) << 4) | (p.x - cx * 16);
            for (int t = 0; t < 4; ++t) {
                if (!maps[t].has_value()) {
                    primeType(cx, cz, t);
                } else {
                    (*maps[t])[static_cast<std::size_t>(idx)] = scan(c, p.x, p.z, nonWgType(t));
                }
            }
        }
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
    // BlockStateBase.isAir — cave_air (MonsterRoom interiors) and void_air count.
    bool isEmptyBlock(BlockPos p) const override { return mc::block::isAirBlock(getBlockState(p)); }

    // ---- leaf DISTANCE side map (LeavesBlock DISTANCE property at id-level grid) ----
    int leafDistanceAt(BlockPos p) const {
        auto it = m_leafDistance.find(std::make_tuple(p.x, p.y, p.z));
        if (it == m_leafDistance.end())
            throw std::logic_error("leaves without a DISTANCE record (side map out of sync)");
        return it->second;
    }
    // TreeFeature.setBlockKnownShape(pos, state.setValue(DISTANCE, d)): a real
    // level.setBlock(pos, sameId, 19) in Java — radius-gated by ensureCanWrite, id
    // unchanged, so only the side map updates (and only when the write would land).
    void setLeafDistance(BlockPos p, int distance) {
        if (!ensureCanWrite(p)) return;
        auto it = m_leafDistance.find(std::make_tuple(p.x, p.y, p.z));
        if (it == m_leafDistance.end())
            throw std::logic_error("setLeafDistance on a non-leaves cell");
        it->second = distance;
    }

    // ---- vine face side map (VineBlock UP/N/S/W/E booleans at id-level grid) ----
    std::uint8_t vineFacesAt(BlockPos p) const {
        auto it = m_vineFaces.find(std::make_tuple(p.x, p.y, p.z));
        if (it == m_vineFaces.end())
            throw std::logic_error("vine without a face record (side map out of sync)");
        return it->second;
    }
    void putVineFaces(BlockPos p, std::uint8_t faces) {
        m_vineFaces[std::make_tuple(p.x, p.y, p.z)] = faces;
    }

    // ---- FACING side map (CocoaBlock FACING / coral wall fan FACING at id grid;
    // dropped on overwrite in setBlockChecked) ----
    int facingAt(BlockPos p) const {
        auto it = m_facing.find(std::make_tuple(p.x, p.y, p.z));
        if (it == m_facing.end())
            throw std::logic_error("facing block without a FACING record (side map out of sync)");
        return it->second;
    }
    void putFacing(BlockPos p, int direction) {
        m_facing[std::make_tuple(p.x, p.y, p.z)] = direction;
    }

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

    // ---- multiface (glow_lichen / sculk_vein) face/waterlogged side map ----
    mc::levelgen::feature::MultifaceBlockState multifaceStateAt(BlockPos p) const {
        mc::levelgen::feature::MultifaceBlockState s;
        s.block = getBlockState(p);
        if (s.block == "minecraft:glow_lichen" || s.block == "minecraft:sculk_vein") {
            auto it = m_multiface.find(std::make_tuple(p.x, p.y, p.z));
            if (it == m_multiface.end())
                throw std::logic_error(s.block + " without face record (multiface side map out of sync)");
            s.isPlaceBlock = true;   // callers re-derive vs their own configured block
            s.faces = it->second.first;
            s.waterlogged = it->second.second;
        }
        return s;
    }
    void putMultifaceState(BlockPos p, std::uint8_t faces, bool waterlogged) {
        m_multiface[std::make_tuple(p.x, p.y, p.z)] = { faces, waterlogged };
    }

    // ---- pale_moss_carpet (MossyCarpetBlock) BASE + wall-side property map ----
    // sides indexed NORTH,EAST,SOUTH,WEST (Direction.Plane.HORIZONTAL order,
    // Direction.java:577); values 0=NONE 1=LOW 2=TALL (WallSide).
    struct CarpetState { bool base = true; std::array<std::uint8_t, 4> sides{ 0, 0, 0, 0 }; };
    std::optional<CarpetState> carpetStateAt(BlockPos p) const {
        if (getBlockState(p) != "minecraft:pale_moss_carpet") return std::nullopt;
        auto it = m_carpet.find(std::make_tuple(p.x, p.y, p.z));
        if (it == m_carpet.end())
            throw std::logic_error("pale_moss_carpet without property record (carpet side map out of sync)");
        return it->second;
    }
    void putCarpetState(BlockPos p, const CarpetState& s) {
        m_carpet[std::make_tuple(p.x, p.y, p.z)] = s;
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
        // VegetationBlock.canSurvive (VegetationBlock.java:44-47): mayPlaceOn(below)
        // = below.is(BlockTags.SUPPORTS_VEGETATION) (:23-25). The family (Blocks.java
        // registrations): SaplingBlock (oak/birch_sapling), TallGrassBlock
        // (short_grass, fern), BushBlock (bush; FireflyBushBlock extends it),
        // FlowerBlock (dandelion, poppy, blue_orchid, allium, azure_bluet, the four
        // tulips, oxeye_daisy, cornflower, lily_of_the_valley), SweetBerryBushBlock,
        // and DoublePlantBlock LOWER halves (tall_grass, large_fern, sunflower,
        // lilac, rose_bush, peony — DoublePlantBlock.java:77-84 defers to super for
        // LOWER, the half the features test).
        static const std::set<std::string> vegetationFamily = {
            "minecraft:oak_sapling", "minecraft:birch_sapling", "minecraft:spruce_sapling",
            // New-class SaplingBlock registrations (VegetationBlock canSurvive) and
            // EyeblossomBlock (FlowerBlock subclass, no canSurvive override).
            "minecraft:jungle_sapling", "minecraft:acacia_sapling", "minecraft:dark_oak_sapling",
            "minecraft:cherry_sapling", "minecraft:pale_oak_sapling",
            "minecraft:closed_eyeblossom", "minecraft:open_eyeblossom",
            // FlowerBedBlock (wildflowers, pink_petals) extends VegetationBlock
            // without overriding canSurvive/mayPlaceOn (FlowerBedBlock.java:23).
            "minecraft:wildflowers", "minecraft:pink_petals",
            "minecraft:short_grass", "minecraft:fern", "minecraft:bush", "minecraft:firefly_bush",
            "minecraft:dandelion", "minecraft:poppy", "minecraft:blue_orchid", "minecraft:allium",
            "minecraft:azure_bluet", "minecraft:red_tulip", "minecraft:orange_tulip",
            "minecraft:white_tulip", "minecraft:pink_tulip", "minecraft:oxeye_daisy",
            "minecraft:cornflower", "minecraft:lily_of_the_valley", "minecraft:sweet_berry_bush",
            "minecraft:tall_grass", "minecraft:large_fern", "minecraft:sunflower",
            "minecraft:lilac", "minecraft:rose_bush", "minecraft:peony",
        };
        if (vegetationFamily.count(block) != 0) {
            return g_tags->isInTag(mc::block::blockName(getBlockState(belowPos)), "minecraft:supports_vegetation");
        }
        // DryVegetationBlock.mayPlaceOn (DryVegetationBlock.java): SUPPORTS_DRY_VEGETATION.
        if (block == "minecraft:dead_bush" || block == "minecraft:short_dry_grass"
            || block == "minecraft:tall_dry_grass") {
            return g_tags->isInTag(mc::block::blockName(getBlockState(belowPos)), "minecraft:supports_dry_vegetation");
        }
        // MushroomBlock.canSurvive (MushroomBlock.java:84-87): below in
        // #overrides_mushroom_light_requirement -> true; else rawBrightness(pos,0) < 13
        // — always 0 < 13 during worldgen (the certified GT proxy returns brightness 0,
        // FullChunkDecorateParity.java:697-699) — && mayPlaceOn(below) =
        // below.isSolidRender (MushroomBlock.java:78-80).
        if (block == "minecraft:brown_mushroom" || block == "minecraft:red_mushroom") {
            const std::string below = mc::block::blockName(getBlockState(belowPos));
            if (g_tags->isInTag(below, "minecraft:overrides_mushroom_light_requirement")) return true;
            bool defaulted = false;
            const bool solid = mc::block::isSolidRender(below, &defaulted);
            if (defaulted) g_solidRenderDefaulted.insert(below);
            return solid;
        }
        // PumpkinBlock / MelonBlock extend Block: canSurvive is the Block default (true);
        // their placement gate is the placed feature's block_predicate_filter.
        if (block == "minecraft:pumpkin" || block == "minecraft:melon") {
            return true;
        }
        // LeafLitterBlock.canSurvive (LeafLitterBlock.java:43-47): below isFaceSturdy UP.
        if (block == "minecraft:leaf_litter") {
            bool defaulted = false;
            const bool sturdy = mc::block::isFaceSturdyUp(mc::block::blockName(getBlockState(belowPos)), &defaulted);
            if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(getBlockState(belowPos)));
            return sturdy;
        }
        // CactusBlock.canSurvive (CactusBlock.java:113-123): every horizontal
        // neighbour must be non-solid and not lava; below cactus or
        // #supports_cactus; above must not be liquid.
        if (block == "minecraft:cactus") {
            static constexpr int HX[4] = { 0, 1, 0, -1 }, HZ[4] = { -1, 0, 1, 0 };   // N,E,S,W
            for (int d = 0; d < 4; ++d) {
                const BlockPos np{ pos.x + HX[d], pos.y, pos.z + HZ[d] };
                const std::string neighbor = getBlockState(np);
                bool defaulted = false;
                const bool solid = mc::block::isSolid(neighbor, &defaulted);
                if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(neighbor));
                if (solid || mc::material::fluidStateOf(neighbor).is(*g_fluidTags, "minecraft:lava")) return false;
            }
            const std::string below = mc::block::blockName(getBlockState(belowPos));
            const std::string above = getBlockState(BlockPos{ pos.x, pos.y + 1, pos.z });
            const bool aboveLiquid = above == "minecraft:water" || above == "minecraft:lava";   // BlockStateBase.liquid()
            return (below == "minecraft:cactus" || g_tags->isInTag(below, "minecraft:supports_cactus")) && !aboveLiquid;
        }
        // SugarCaneBlock.canSurvive (SugarCaneBlock.java:89-108): below sugar_cane
        // -> true; below #supports_sugar_cane -> true iff any horizontal neighbour
        // OF THE BELOW CELL has fluid #supports_sugar_cane_adjacently or block
        // #supports_sugar_cane_adjacently.
        if (block == "minecraft:sugar_cane") {
            const std::string below = mc::block::blockName(getBlockState(belowPos));
            if (below == "minecraft:sugar_cane") return true;
            if (g_tags->isInTag(below, "minecraft:supports_sugar_cane")) {
                static constexpr int HX[4] = { 0, 1, 0, -1 }, HZ[4] = { -1, 0, 1, 0 };   // N,E,S,W
                for (int d = 0; d < 4; ++d) {
                    const BlockPos np{ belowPos.x + HX[d], belowPos.y, belowPos.z + HZ[d] };
                    const std::string st = getBlockState(np);
                    if (mc::material::fluidStateOf(st).is(*g_fluidTags, "minecraft:supports_sugar_cane_adjacently")
                        || g_tags->isInTag(mc::block::blockName(st), "minecraft:supports_sugar_cane_adjacently")) {
                        return true;
                    }
                }
            }
            return false;
        }
        // LilyPadBlock via VegetationBlock.canSurvive: mayPlaceOn(belowState, level,
        // belowPos) (LilyPadBlock.java:49-53) = (fluid(below) #supports_lily_pad ||
        // belowBlock #supports_lily_pad) && fluid(below.above() == pos) is EMPTY.
        if (block == "minecraft:lily_pad") {
            const std::string below = getBlockState(belowPos);
            const bool support = mc::material::fluidStateOf(below).is(*g_fluidTags, "minecraft:supports_lily_pad")
                || g_tags->isInTag(mc::block::blockName(below), "minecraft:supports_lily_pad");
            return support && mc::material::fluidStateOf(getBlockState(pos)).isEmpty();
        }
        // HangingRootsBlock.canSurvive (HangingRootsBlock.java:59-63): the block
        // ABOVE must be face-sturdy DOWN.
        if (block == "minecraft:hanging_roots") {
            bool defaulted = false;
            const bool r = mc::block::isFaceSturdyFull(
                mc::block::blockName(getBlockState(BlockPos{ pos.x, pos.y + 1, pos.z })), 0, &defaulted);
            if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(getBlockState(BlockPos{ pos.x, pos.y + 1, pos.z })));
            return r;
        }
        // CaveVinesBlock/CaveVinesPlantBlock via GrowingPlantBlock.canSurvive
        // (GrowingPlantBlock.java:47-55, growthDirection DOWN, canAttachTo true):
        // above is head/body or face-sturdy DOWN.
        if (block == "minecraft:cave_vines" || block == "minecraft:cave_vines_plant") {
            const BlockPos abovePos{ pos.x, pos.y + 1, pos.z };
            const std::string above = mc::block::blockName(getBlockState(abovePos));
            if (above == "minecraft:cave_vines" || above == "minecraft:cave_vines_plant") return true;
            bool defaulted = false;
            const bool r = mc::block::isFaceSturdyFull(above, 0, &defaulted);
            if (defaulted) g_blocksMotionDefaulted.insert(above);
            return r;
        }
        // SporeBlossomBlock.canSurvive (SporeBlossomBlock.java:35-37):
        // Block.canSupportCenter(level, above, DOWN) && !isWaterAt(pos). CENTER
        // support coincides with FULL for the cave ceiling blocks here (full cubes);
        // partial-center supporters (fences, ...) cannot occur in this pipeline.
        if (block == "minecraft:spore_blossom") {
            const std::string above = mc::block::blockName(getBlockState(BlockPos{ pos.x, pos.y + 1, pos.z }));
            bool defaulted = false;
            const bool sturdy = mc::block::isFaceSturdyFull(above, 0, &defaulted);
            if (defaulted) g_blocksMotionDefaulted.insert(above);
            return sturdy && getBlockState(pos) != "minecraft:water";
        }
        // CarpetBlock.canSurvive (CarpetBlock.java:50-52): below not empty.
        if (block == "minecraft:moss_carpet") {
            return !mc::block::isAirBlock(getBlockState(belowPos));
        }
        // AzaleaBlock via VegetationBlock.canSurvive with mayPlaceOn override
        // (AzaleaBlock.java:38-40): below in #supports_azalea.
        if (block == "minecraft:azalea" || block == "minecraft:flowering_azalea") {
            return g_tags->isInTag(mc::block::blockName(getBlockState(belowPos)), "minecraft:supports_azalea");
        }
        // SmallDripleafBlock (LOWER half) via DoublePlantBlock.canSurvive with
        // mayPlaceOn override (SmallDripleafBlock.java:51-54): below in
        // #supports_small_dripleaf OR (source water AT pos && below in
        // #supports_vegetation).
        if (block == "minecraft:small_dripleaf") {
            const std::string below = mc::block::blockName(getBlockState(belowPos));
            if (g_tags->isInTag(below, "minecraft:supports_small_dripleaf")) return true;
            const mc::material::FluidState fs = mc::material::fluidStateOf(getBlockState(pos));
            return fs.is(*g_fluidTags, "minecraft:water") && fs.isSource()
                && g_tags->isInTag(below, "minecraft:supports_vegetation");
        }
        // BigDripleafBlock.canSurvive (BigDripleafBlock.java:139-143).
        if (block == "minecraft:big_dripleaf") {
            const std::string below = mc::block::blockName(getBlockState(belowPos));
            return below == "minecraft:big_dripleaf" || below == "minecraft:big_dripleaf_stem"
                || g_tags->isInTag(below, "minecraft:supports_big_dripleaf");
        }
        // BigDripleafStemBlock.canSurvive (BigDripleafStemBlock.java:60-64).
        if (block == "minecraft:big_dripleaf_stem") {
            const std::string below = mc::block::blockName(getBlockState(belowPos));
            const std::string above = mc::block::blockName(getBlockState(BlockPos{ pos.x, pos.y + 1, pos.z }));
            return (below == "minecraft:big_dripleaf_stem" || g_tags->isInTag(below, "minecraft:supports_big_dripleaf"))
                && (above == "minecraft:big_dripleaf_stem" || above == "minecraft:big_dripleaf");
        }
        // MangrovePropaguleBlock.canSurvive (MangrovePropaguleBlock.java:73-75):
        // the DEFAULT state is hanging=false (the would_survive predicate's state)
        // -> super (VegetationBlock) with mayPlaceOn = #supports_mangrove_propagule
        // (:55-57). Hanging propagules are revalidated in updateShapeOnce.
        if (block == "minecraft:mangrove_propagule") {
            return g_tags->isInTag(mc::block::blockName(getBlockState(belowPos)), "minecraft:supports_mangrove_propagule");
        }
        // BambooStalkBlock.canSurvive (BambooStalkBlock.java:130-132).
        if (block == "minecraft:bamboo") {
            return g_tags->isInTag(mc::block::blockName(getBlockState(belowPos)), "minecraft:supports_bamboo");
        }
        // SeaPickleBlock.canSurvive (SeaPickleBlock.java:64-72): mayPlaceOn(below)
        // = !below.getCollisionShape().getFaceShape(UP).isEmpty() || below
        // .isFaceSturdy(UP). Over the aquatic worldgen set the non-empty-face-shape
        // disjunct coincides with the collision UP-face-full test (full cubes and
        // coral blocks; the partial blocks reachable under water have no geometry
        // at the top plane).
        if (block == "minecraft:sea_pickle") {
            const std::string below = mc::block::blockName(getBlockState(belowPos));
            bool defaulted = false;
            const bool r = mc::block::isCollisionFaceFullUp(below, &defaulted)
                || mc::block::isFaceSturdyUp(below, &defaulted);
            if (defaulted) g_blocksMotionDefaulted.insert(below);
            return r;
        }
        // MossyCarpetBlock.canSurvive (MossyCarpetBlock.java:104-107): worldgen
        // places bottom=true (the codec state) -> !below.isAir().
        if (block == "minecraft:pale_moss_carpet") {
            return !mc::block::isAirBlock(getBlockState(belowPos));
        }
        // BaseCoralPlantTypeBlock.canSurvive (BaseCoralPlantTypeBlock.java:89-92):
        // below isFaceSturdy UP (coral plants and fans; wall fans are facing-based
        // and revalidated in updateShapeOnce via the facing side map).
        if (block == "minecraft:tube_coral" || block == "minecraft:brain_coral"
            || block == "minecraft:bubble_coral" || block == "minecraft:fire_coral"
            || block == "minecraft:horn_coral"
            || block == "minecraft:tube_coral_fan" || block == "minecraft:brain_coral_fan"
            || block == "minecraft:bubble_coral_fan" || block == "minecraft:fire_coral_fan"
            || block == "minecraft:horn_coral_fan") {
            const std::string below = mc::block::blockName(getBlockState(belowPos));
            bool defaulted = false;
            const bool r = mc::block::isFaceSturdyUp(below, &defaulted);
            if (defaulted) g_blocksMotionDefaulted.insert(below);
            return r;
        }
        // SnowLayerBlock.canSurvive (SnowLayerBlock.java:77-86): below
        // #cannot_support_snow_layer -> false; #support_override_snow_layer -> true;
        // else collision top-face-full (Block.isFaceFull(getCollisionShape, UP)) ||
        // (below is snow && LAYERS == 8 — worldgen places layers=1 only, never 8).
        if (block == "minecraft:snow") {
            const std::string below = mc::block::blockName(getBlockState(belowPos));
            if (g_tags->isInTag(below, "minecraft:cannot_support_snow_layer")) return false;
            if (g_tags->isInTag(below, "minecraft:support_override_snow_layer")) return true;
            bool defaulted = false;
            const bool full = mc::block::isCollisionFaceFullUp(below, &defaulted);
            if (defaulted) g_blocksMotionDefaulted.insert(below);
            return full;
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
    // (Heightmap.java:147-156), or minY-1 when the column has none (getHeight adds
    // 1 -> minY, matching an unset Heightmap entry).
    int scan(mc::LevelChunk* c, int x, int z, Heightmap::Types type) const {
        for (int y = m_maxY - 1; y >= m_minY; --y) {
            const std::uint32_t id = c->getBlock(x, y, z);
            if (id == m_airId) continue;                     // primeHeightmaps' !is(AIR) fast path
            if (!heightmapOpaque(type, id)) continue;
            return y;
        }
        return m_minY - 1;
    }
    // 0 = OCEAN_FLOOR, 1 = WORLD_SURFACE, 2 = MOTION_BLOCKING, 3 = MOTION_BLOCKING_NO_LEAVES.
    static int nonWgIndex(Heightmap::Types type) {
        switch (type) {
            case Heightmap::Types::OCEAN_FLOOR: return 0;
            case Heightmap::Types::WORLD_SURFACE: return 1;
            case Heightmap::Types::MOTION_BLOCKING: return 2;
            case Heightmap::Types::MOTION_BLOCKING_NO_LEAVES: return 3;
            default: throw std::logic_error("nonWgIndex on a WG heightmap");
        }
    }
    static Heightmap::Types nonWgType(int t) {
        static constexpr Heightmap::Types types[4] = {
            Heightmap::Types::OCEAN_FLOOR, Heightmap::Types::WORLD_SURFACE,
            Heightmap::Types::MOTION_BLOCKING, Heightmap::Types::MOTION_BLOCKING_NO_LEAVES,
        };
        return types[t];
    }
    void primeType(int cx, int cz, int t) {
        mc::LevelChunk* c = at(cx, cz);
        if (!c) throw std::logic_error("priming heightmaps outside the generated grid");
        auto& maps = m_heightmaps[packChunk(cx, cz)];
        std::array<int, 256> snap{};
        for (int i = 0; i < 256; ++i) {
            const int x = cx * 16 + (i & 15), z = cz * 16 + (i >> 4);
            snap[static_cast<std::size_t>(i)] = scan(c, x, z, nonWgType(t));
        }
        maps[t] = snap;
    }
    struct Mark { int section; BlockPos pos; };
    struct WgMaps { std::array<int, 256> oceanFloorWg; std::array<int, 256> worldSurfaceWg; };
    std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>>* m_chunks;
    // Post-carver *_WG snapshot (frozen for the whole FEATURES phase).
    std::map<std::int64_t, WgMaps> m_wgSnapshot;
    // Per chunk: the four FINAL heightmaps (highest passing y per column), primed
    // lazily on first touch and LIVE-updated by non-bulk writes.
    mutable std::map<std::int64_t, std::array<std::optional<std::array<int, 256>>, 4>> m_heightmaps;
    bool m_bulkWriting = false;
    std::unordered_map<std::int64_t, std::vector<Mark>> m_marks;
    std::map<std::tuple<int, int, int>, std::pair<std::uint8_t, bool>> m_multiface;
    std::map<std::tuple<int, int, int>, CarpetState> m_carpet;
    std::map<std::tuple<int, int, int>, int> m_leafDistance;
    std::map<std::tuple<int, int, int>, std::uint8_t> m_vineFaces;
    std::map<std::tuple<int, int, int>, int> m_facing;   // cocoa / coral wall fans
    bool m_postprocessing = false;
    int m_minY, m_maxY, m_dcx = 0, m_dcz = 0; std::uint32_t m_airId{};
};

// ============================ MossyCarpetBlock (pale_moss_carpet) ============================
// 1:1 port of MossyCarpetBlock.placeAt + getUpdatedState + createTopperWithSideChance
// + hasFaces (MossyCarpetBlock.java:109-208) over the harness carpet side map. The
// grid stores the block id; BASE + the four WallSide properties live in m_carpet.
// Direction.Plane.HORIZONTAL iteration order = NORTH, EAST, SOUTH, WEST
// (Direction.java:577) == tree dirs {2, 5, 3, 4}.
namespace mossy_carpet {

using CarpetState = MultiChunkLevel::CarpetState;
inline constexpr int HORIZ[4] = { 2, 5, 3, 4 };          // N, E, S, W (tree encoding)
inline constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };

// MossyCarpetBlock.canSupportAtFace (:123-125): UP -> false; else
// MultifaceBlock.canAttachTo(level, pos, direction) (MultifaceBlock.java:250-261).
inline bool canSupportAtFace(MultiChunkLevel& level, BlockPos pos, int dir) {
    if (dir == 1) return false;   // Direction.UP
    return mc::levelgen::feature::multiface_detail::canAttachTo(
        level, mc::levelgen::feature::treeRelative(pos, dir), OPP[dir]);
}

// MossyCarpetBlock.hasFaces (:109-121).
inline bool hasFaces(const CarpetState& s) {
    if (s.base) return true;
    for (int i = 0; i < 4; ++i) if (s.sides[i] != 0) return true;
    return false;
}

// MossyCarpetBlock.getUpdatedState (:127-159). WallSide: 0=NONE 1=LOW 2=TALL.
inline CarpetState getUpdatedState(MultiChunkLevel& level, CarpetState state, BlockPos pos, bool createSides) {
    std::optional<CarpetState> aboveState; bool aboveLoaded = false;
    std::optional<CarpetState> belowState; bool belowLoaded = false;
    createSides |= state.base;
    for (int i = 0; i < 4; ++i) {
        const int dir = HORIZ[i];
        std::uint8_t side = canSupportAtFace(level, pos, dir) ? (createSides ? 1 : state.sides[i]) : 0;
        if (side == 1) {
            if (!aboveLoaded) {
                aboveState = level.carpetStateAt(BlockPos{ pos.x, pos.y + 1, pos.z });
                aboveLoaded = true;
            }
            // aboveState.is(PALE_MOSS_CARPET) && getValue(property)!=NONE && !getValue(BASE)
            if (aboveState && aboveState->sides[i] != 0 && !aboveState->base) side = 2;
            if (!state.base) {
                if (!belowLoaded) {
                    belowState = level.carpetStateAt(BlockPos{ pos.x, pos.y - 1, pos.z });
                    belowLoaded = true;
                }
                if (belowState && belowState->sides[i] == 0) side = 0;
            }
        }
        state.sides[i] = side;
    }
    return state;
}

// MossyCarpetBlock.createTopperWithSideChance (:189-208). Returns the topper state
// to place above, or nullopt for the AIR result.
inline std::optional<CarpetState> createTopperWithSideChance(
        MultiChunkLevel& level, BlockPos pos, const std::function<bool()>& sideSurvivalTest,
        const std::function<bool(const std::string&)>& canBeReplaced) {
    const BlockPos above{ pos.x, pos.y + 1, pos.z };
    const std::string abovePrevId = level.getBlockState(above);
    const std::optional<CarpetState> abovePrev = level.carpetStateAt(above);
    const bool isMossyCarpetAbove = abovePrev.has_value();
    if ((!isMossyCarpetAbove || !abovePrev->base) && (isMossyCarpetAbove || canBeReplaced(abovePrevId))) {
        CarpetState noCarpetBase; noCarpetBase.base = false;   // default.setValue(BASE, false)
        CarpetState aboveState = getUpdatedState(level, noCarpetBase, above, true);
        for (int i = 0; i < 4; ++i) {
            if (aboveState.sides[i] != 0 && !sideSurvivalTest()) aboveState.sides[i] = 0;
        }
        // hasFaces(aboveState) && aboveState != abovePreviousState (BlockState compare:
        // a non-carpet abovePreviousState is trivially unequal).
        const bool unequal = !isMossyCarpetAbove
            || abovePrev->base != aboveState.base || abovePrev->sides != aboveState.sides;
        if (hasFaces(aboveState) && unequal) return aboveState;
        return std::nullopt;
    }
    return std::nullopt;
}

// MossyCarpetBlock.placeAt(level, pos, level.getRandom(), 2) (:166-176).
inline void placeAt(MultiChunkLevel& level, BlockPos pos,
                    const std::function<bool(const std::string&)>& canBeReplaced) {
    CarpetState simpleCarpetLayer;   // defaultBlockState(): BASE=true, sides NONE
    const CarpetState adjustedCarpetLayer = getUpdatedState(level, simpleCarpetLayer, pos, true);
    if (level.setBlockChecked(pos, "minecraft:pale_moss_carpet", 2)) level.putCarpetState(pos, adjustedCarpetLayer);
    const std::optional<CarpetState> topper = createTopperWithSideChance(
        level, pos, [] { return g_regionRandom->nextBoolean(); }, canBeReplaced);
    if (topper) {
        const BlockPos above{ pos.x, pos.y + 1, pos.z };
        if (level.setBlockChecked(above, "minecraft:pale_moss_carpet", 2)) level.putCarpetState(above, *topper);
        const CarpetState updateBottomCarpet = getUpdatedState(level, adjustedCarpetLayer, pos, true);
        if (level.setBlockChecked(pos, "minecraft:pale_moss_carpet", 2)) level.putCarpetState(pos, updateBottomCarpet);
    }
}

} // namespace mossy_carpet

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

// BlockStateBase.updateShape for one (pos, direction, neighbor) visit, at block-id
// granularity over the closed set the forest/ocean pipelines can produce. Returns
// the new block id for `pos` (unchanged for the static set). Property-only changes
// (leaf DISTANCE ticks, chest type, liquid ticks) are id-invisible no-ops here.
//   - static full blocks / air / liquids / leaves / logs: unchanged
//     (LeavesBlock.updateShape LeavesBlock.java:91-110 only schedules ticks)
//   - VegetationBlock family (short_grass, bush, lily_of_the_valley, saplings,
//     leaf_litter): !canSurvive -> AIR (VegetationBlock.java:28-41)
//   - DoublePlantBlock (lilac/rose_bush/peony): the HALF logic
//     (DoublePlantBlock.java:41-61); the grid drops HALF — the upper half is
//     exactly the cell whose below holds the same block id
//   - anything else: fail closed (throw) — port before passing
std::string updateShapeOnce(MultiChunkLevel& level, const std::string& bs, BlockPos pos,
                            int direction, BlockPos /*neighborPos*/) {
    static const std::set<std::string> idNoOp = {
        "minecraft:air", "minecraft:cave_air", "minecraft:void_air",
        "minecraft:stone", "minecraft:granite", "minecraft:diorite",
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
        // liquids / bubble column: tick scheduling only (LiquidBlock.updateShape,
        // BubbleColumnBlock.updateShape BubbleColumnBlock.java:160-179)
        "minecraft:water", "minecraft:lava", "minecraft:bubble_column",
        // LeavesBlock.updateShape: DISTANCE tick scheduling only (id unchanged)
        "minecraft:oak_leaves", "minecraft:birch_leaves", "minecraft:spruce_leaves",
        // RotatedPillarBlock/Block default updateShape: unchanged
        "minecraft:oak_log", "minecraft:birch_log", "minecraft:spruce_log",
        "minecraft:cobblestone", "minecraft:mossy_cobblestone",
        // ChestBlock.updateShape only reconnects the TYPE property; SpawnerBlock /
        // BeehiveBlock use the Block default
        "minecraft:chest", "minecraft:spawner", "minecraft:bee_nest",
        // SnowyBlock.updateShape (SnowyBlock.java:32-45): UP -> SNOWY property only
        // (id unchanged); grass_block/podzol already in the static list above.
        "minecraft:mycelium",
        // CactusBlock.updateShape (CactusBlock.java:95-110) and SugarCaneBlock
        // .updateShape (SugarCaneBlock.java:81-86): !canSurvive only SCHEDULES a
        // tick (no ServerLevel during worldgen) — id unchanged.
        "minecraft:cactus", "minecraft:sugar_cane",
        // SlabBlock.updateShape (SlabBlock.java:116-130): waterlogged tick only;
        // BrushableBlock.updateShape schedules a tick — both id-unchanged.
        "minecraft:sandstone_slab", "minecraft:suspicious_sand",
        // Geode shell full cubes: Block default updateShape.
        "minecraft:amethyst_block", "minecraft:budding_amethyst",
        "minecraft:calcite", "minecraft:smooth_basalt",
        // Lush/dripstone cave full cubes (Block default updateShape) + the
        // tick-only updateShapes: GrowingPlantBlock head/body (cave_vines: schedule
        // only when the support breaks — the head->body conversion needs the plant
        // directly below which postprocess marks cannot create), pointed_dripstone
        // (PointedDripstoneBlock.updateShape: tick scheduling, returns state).
        "minecraft:moss_block", "minecraft:rooted_dirt", "minecraft:dripstone_block",
        "minecraft:raw_iron_block", "minecraft:raw_copper_block", "minecraft:raw_gold_block",
        "minecraft:azalea_leaves", "minecraft:flowering_azalea_leaves",
        "minecraft:pointed_dripstone",
        // New-class logs/leaves (LeavesBlock tick-only; RotatedPillarBlock default),
        // HugeMushroomBlock (updateShape strips the touching-face property only —
        // id unchanged), mangrove_roots (waterlog tick), muddy_mangrove_roots,
        // pale_moss_block / creaking_heart (property-only state changes) /
        // sculk_catalyst (BaseEntityBlock default), bone_block, coral BLOCKS
        // (Block default), terracottas/red_sandstone (full cubes), bamboo
        // (BambooStalkBlock.updateShape: tick + AGE cycle, id unchanged,
        // BambooStalkBlock.java:135-152), pale_hanging_moss (tick + TIP only,
        // HangingMossBlock.java:71-86), sculk (full cube), sculk_sensor /
        // sculk_shrieker (waterlog tick only).
        "minecraft:jungle_log", "minecraft:acacia_log", "minecraft:dark_oak_log",
        "minecraft:cherry_log", "minecraft:mangrove_log", "minecraft:pale_oak_log",
        "minecraft:jungle_leaves", "minecraft:acacia_leaves", "minecraft:dark_oak_leaves",
        "minecraft:cherry_leaves", "minecraft:mangrove_leaves", "minecraft:pale_oak_leaves",
        "minecraft:brown_mushroom_block", "minecraft:red_mushroom_block", "minecraft:mushroom_stem",
        "minecraft:mangrove_roots", "minecraft:muddy_mangrove_roots", "minecraft:mud",
        "minecraft:pale_moss_block", "minecraft:creaking_heart", "minecraft:sculk_catalyst",
        "minecraft:bone_block",
        "minecraft:tube_coral_block", "minecraft:brain_coral_block", "minecraft:bubble_coral_block",
        "minecraft:fire_coral_block", "minecraft:horn_coral_block",
        "minecraft:terracotta", "minecraft:white_terracotta", "minecraft:orange_terracotta",
        "minecraft:yellow_terracotta", "minecraft:brown_terracotta", "minecraft:red_terracotta",
        "minecraft:light_gray_terracotta", "minecraft:red_sandstone",
        "minecraft:bamboo", "minecraft:pale_hanging_moss", "minecraft:sculk",
        "minecraft:sculk_sensor", "minecraft:sculk_shrieker",
        // PowderSnowBlock extends Block with NO updateShape override
        // (PowderSnowBlock.java) -> BlockBehaviour identity updateShape.
        "minecraft:powder_snow",
    };
    if (idNoOp.count(bs) != 0) return bs;
    // CocoaBlock.updateShape (CocoaBlock.java:91-104): direction == FACING &&
    // !canSurvive -> AIR; canSurvive = state at pos.relative(FACING) in
    // #supports_cocoa (CocoaBlock.java:62-65). FACING lives in the side map.
    if (bs == "minecraft:cocoa") {
        const int facing = level.facingAt(pos);
        if (direction == facing) {
            const std::string attached = level.getBlockState(mc::levelgen::feature::treeRelative(pos, facing));
            if (!g_tags->isInTag(mc::block::blockName(attached), "minecraft:supports_cocoa")) {
                return "minecraft:air";
            }
        }
        return bs;
    }
    // MangrovePropaguleBlock.updateShape (MangrovePropaguleBlock.java:78-95):
    // UP && !canSurvive -> AIR. Worldgen propagules are HANGING (the
    // attached_to_leaves decorator state): canSurvive = above in
    // #supports_hanging_mangrove_propagule (:73-75).
    if (bs == "minecraft:mangrove_propagule") {
        if (direction == 1) {
            const std::string above = mc::block::blockName(level.getBlockState(BlockPos{ pos.x, pos.y + 1, pos.z }));
            if (!g_tags->isInTag(above, "minecraft:supports_hanging_mangrove_propagule")) {
                return "minecraft:air";
            }
        }
        return bs;
    }
    // MossyCarpetBlock.updateShape (MossyCarpetBlock.java:211-227): !canSurvive ->
    // AIR (canSurvive :104-107: BASE -> !below.isAir(); topper -> below is a BASE
    // carpet); then getUpdatedState(state, level, pos, false) recomputes the sides
    // (id-invisible, side map updated) and !hasFaces -> AIR.
    if (bs == "minecraft:pale_moss_carpet") {
        const std::optional<MultiChunkLevel::CarpetState> st = level.carpetStateAt(pos);
        const BlockPos below{ pos.x, pos.y - 1, pos.z };
        if (st->base) {
            if (mc::block::isAirBlock(level.getBlockState(below))) return "minecraft:air";
        } else {
            const std::optional<MultiChunkLevel::CarpetState> belowSt = level.carpetStateAt(below);
            if (!belowSt || !belowSt->base) return "minecraft:air";
        }
        const MultiChunkLevel::CarpetState updated = mossy_carpet::getUpdatedState(level, *st, pos, false);
        if (!mossy_carpet::hasFaces(updated)) return "minecraft:air";
        level.putCarpetState(pos, updated);
        return bs;
    }
    // SeaPickleBlock.updateShape (SeaPickleBlock.java:75-94): !canSurvive -> AIR
    // (direction-independent).
    if (bs == "minecraft:sea_pickle") {
        return level.canSurvive(bs, pos) ? bs : "minecraft:air";
    }
    // BaseCoralPlantTypeBlock.updateShape (BaseCoralPlantTypeBlock.java:69-86):
    // DOWN && !canSurvive -> AIR (coral plants + fans).
    if (bs == "minecraft:tube_coral" || bs == "minecraft:brain_coral"
        || bs == "minecraft:bubble_coral" || bs == "minecraft:fire_coral"
        || bs == "minecraft:horn_coral"
        || bs == "minecraft:tube_coral_fan" || bs == "minecraft:brain_coral_fan"
        || bs == "minecraft:bubble_coral_fan" || bs == "minecraft:fire_coral_fan"
        || bs == "minecraft:horn_coral_fan") {
        if (direction == 0 && !level.canSurvive(bs, pos)) return "minecraft:air";
        return bs;
    }
    // BaseCoralWallFanBlock.updateShape (BaseCoralWallFanBlock.java:58-73):
    // directionToNeighbour.getOpposite() == FACING && !canSurvive -> AIR;
    // canSurvive (:76-81) = state at pos.relative(FACING.getOpposite())
    // isFaceSturdy(FACING).
    if (bs == "minecraft:tube_coral_wall_fan" || bs == "minecraft:brain_coral_wall_fan"
        || bs == "minecraft:bubble_coral_wall_fan" || bs == "minecraft:fire_coral_wall_fan"
        || bs == "minecraft:horn_coral_wall_fan") {
        static constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };
        const int facing = level.facingAt(pos);
        if (OPP[direction] == facing) {
            const std::string rel = level.getBlockState(mc::levelgen::feature::treeRelative(pos, OPP[facing]));
            bool defaulted = false;
            const bool sturdy = mc::block::isFaceSturdyFull(rel, facing, &defaulted);
            if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(rel));
            if (!sturdy) return "minecraft:air";
        }
        return bs;
    }
    // MultifaceBlock.updateShape for ONE direction (MultifaceBlock.java:135-143):
    // !hasAnyFace -> AIR; hasFace(direction) && !canAttachTo -> removeFace.
    // Applies to glow_lichen and sculk_vein (face bits in the multiface side map).
    if (bs == "minecraft:glow_lichen" || bs == "minecraft:sculk_vein") {
        namespace md = mc::levelgen::feature::multiface_detail;
        mc::levelgen::feature::MultifaceBlockState st = level.multifaceStateAt(pos);
        if (st.faces == 0) return "minecraft:air";   // hasAnyFace false
        static constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };
        if ((st.faces & (1u << direction)) != 0
            && !md::canAttachTo(level, md::relative(pos, direction), OPP[direction])) {
            const std::uint8_t newFaces = static_cast<std::uint8_t>(st.faces & ~(1u << direction));
            // MultifaceBlock.removeFace (:263-266): no faces left -> plain AIR
            // (the waterlogged bit is dropped, verbatim).
            if (newFaces == 0) return "minecraft:air";
            level.putMultifaceState(pos, newFaces, st.waterlogged);   // id unchanged
        }
        return bs;
    }
    // HangingRootsBlock.updateShape (HangingRootsBlock.java:81-83): UP ->
    // !canSurvive -> AIR; SporeBlossomBlock likewise (SporeBlossomBlock.java:50-51);
    // BigDripleaf DOWN -> !canSurvive -> AIR (BigDripleafBlock.java:156-157).
    if (bs == "minecraft:hanging_roots" || bs == "minecraft:spore_blossom") {
        if (direction == 1 && !level.canSurvive(bs, pos)) return "minecraft:air";
        return bs;
    }
    if (bs == "minecraft:big_dripleaf") {
        if (direction == 0 && !level.canSurvive(bs, pos)) return "minecraft:air";
        return bs;
    }
    // SnowLayerBlock.updateShape (SnowLayerBlock.java:88-102): !canSurvive -> AIR,
    // direction-independent; otherwise unchanged.
    if (bs == "minecraft:snow") {
        return level.canSurvive(bs, pos) ? bs : "minecraft:air";
    }
    // VegetationBlock.updateShape (VegetationBlock.java:28-41): !canSurvive -> AIR,
    // direction-independent. The single-cell family: TallGrassBlock, BushBlock,
    // FireflyBushBlock, FlowerBlock, SaplingBlock, SweetBerryBushBlock,
    // MushroomBlock, DryVegetationBlock — all extend VegetationBlock without
    // overriding updateShape; leaf_litter inherits it with its own canSurvive
    // (LeafLitterBlock.java:43-47).
    static const std::set<std::string> vegUpdateShape = {
        "minecraft:short_grass", "minecraft:fern", "minecraft:bush", "minecraft:firefly_bush",
        "minecraft:lily_of_the_valley", "minecraft:dandelion", "minecraft:poppy",
        "minecraft:blue_orchid", "minecraft:allium", "minecraft:azure_bluet",
        "minecraft:red_tulip", "minecraft:orange_tulip", "minecraft:white_tulip",
        "minecraft:pink_tulip", "minecraft:oxeye_daisy", "minecraft:cornflower",
        "minecraft:sweet_berry_bush", "minecraft:brown_mushroom", "minecraft:red_mushroom",
        "minecraft:dead_bush", "minecraft:short_dry_grass", "minecraft:tall_dry_grass",
        "minecraft:oak_sapling", "minecraft:birch_sapling", "minecraft:spruce_sapling",
        "minecraft:leaf_litter",
        // LilyPadBlock extends VegetationBlock without overriding updateShape.
        "minecraft:lily_pad",
        // FlowerBedBlock: VegetationBlock updateShape (canSurvive-or-air).
        "minecraft:wildflowers", "minecraft:pink_petals",
        // AzaleaBlock (VegetationBlock) and CarpetBlock both collapse to AIR when
        // canSurvive fails (CarpetBlock.java:34-47).
        "minecraft:azalea", "minecraft:flowering_azalea", "minecraft:moss_carpet",
    };
    if (vegUpdateShape.count(bs) != 0) {
        return level.canSurvive(bs, pos) ? bs : "minecraft:air";
    }
    // SeagrassBlock.updateShape (SeagrassBlock.java:57-75): result = super
    // (VegetationBlock.java:28-41: !canSurvive -> AIR, direction-independent);
    // if the result is not air it schedules a WATER fluid tick — a pending-tick
    // record only (never a block change during generation): counted hard no-op.
    if (bs == "minecraft:seagrass") {
        if (!level.canSurvive(bs, pos)) return "minecraft:air";
        ++g_skippedScheduleTicks;
        return bs;
    }
    // TallSeagrassBlock has no updateShape override — DoublePlantBlock.updateShape
    // (DoublePlantBlock.java:40-61) applies, whose super is VegetationBlock.updateShape
    // (VegetationBlock.java:28-41: !canSurvive -> AIR on ANY direction). The grid
    // drops HALF — the upper half is exactly the cell whose below holds the same id
    // (3-stacks cannot place: the lower needs a sturdy-UP floor). Folding the
    // DOWN-only ternary with the super re-check: LOWER -> !canSurvive(LOWER) -> AIR
    // on any direction (canSurvive = TallSeagrassBlock.java:67-76 LOWER branch);
    // UPPER -> canSurvive (TallSeagrassBlock.java:67-71) is `below is(this) &&
    // below.HALF == LOWER`, true by the same inference -> unchanged.
    if (bs == "minecraft:tall_seagrass") {
        const std::string below = level.getBlockState(BlockPos{ pos.x, pos.y - 1, pos.z });
        const bool upper = below == bs;
        const bool dirIsY = direction == 0 || direction == 1;
        const bool dirIsUp = direction == 1;
        const BlockPos npos = mc::levelgen::feature::treeRelative(pos, direction);
        const std::string nstate = level.getBlockState(npos);
        bool neighbourSameOtherHalf = false;
        if (nstate == bs) {
            const bool nUpper = level.getBlockState(BlockPos{ npos.x, npos.y - 1, npos.z }) == bs;
            neighbourSameOtherHalf = nUpper != upper;
        }
        if (dirIsY && ((!upper) == dirIsUp) && !neighbourSameOtherHalf) {
            // axis==Y && (half==LOWER)==(dir==UP) && !(neighbour same block, other half)
            return "minecraft:air";
        }
        if (!upper && !level.canSurvive(bs, pos)) return "minecraft:air";
        return bs;
    }
    // Plain Block subclasses: default updateShape (unchanged).
    if (bs == "minecraft:pumpkin" || bs == "minecraft:melon") return bs;
    // VineBlock.updateShape (VineBlock.java:128-143): DOWN -> unchanged; otherwise
    // getUpdatedState (:103-126) re-validates every set face — UP face by
    // isAcceptableNeighbour(above, DOWN) (MultifaceBlock.canAttachTo full-cube test),
    // each horizontal face by canSupportAtFace (:79-95: attached block OR the vine
    // above carrying the same face) — and no faces left -> AIR. Face bits live in
    // the harness side map (the grid is id-only).
    if (bs == "minecraft:vine") {
        if (direction == 0) return bs;   // Direction.DOWN -> super (unchanged)
        const std::uint8_t faces = level.vineFacesAt(pos);
        const BlockPos above{ pos.x, pos.y + 1, pos.z };
        auto aboveVineFace = [&](int d) {
            return level.getBlockState(above) == "minecraft:vine"
                   && (level.vineFacesAt(above) & static_cast<std::uint8_t>(1u << d)) != 0;
        };
        std::uint8_t updated = faces;
        if ((faces & (1u << 1)) != 0
            && !mc::levelgen::feature::multiface_detail::canAttachTo(level, above, 0)) {
            updated &= static_cast<std::uint8_t>(~(1u << 1));
        }
        for (int d : { 2, 5, 3, 4 }) {   // Direction.Plane.HORIZONTAL: N, E, S, W
            if ((faces & (1u << d)) == 0) continue;
            static constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };
            const bool canSupport =
                mc::levelgen::feature::multiface_detail::canAttachTo(
                    level, mc::levelgen::feature::treeRelative(pos, d), OPP[d])
                || aboveVineFace(d);
            if (!canSupport) updated &= static_cast<std::uint8_t>(~(1u << d));
        }
        if (updated == 0) return "minecraft:air";
        level.putVineFaces(pos, updated);   // property-only change: id unchanged
        return bs;
    }
    // DoublePlantBlock.updateShape (DoublePlantBlock.java:41-61); TallFlowerBlock
    // extends DoublePlantBlock without overriding it.
    if (bs == "minecraft:lilac" || bs == "minecraft:rose_bush" || bs == "minecraft:peony"
        || bs == "minecraft:tall_grass" || bs == "minecraft:large_fern" || bs == "minecraft:sunflower") {
        const std::string below = level.getBlockState(BlockPos{ pos.x, pos.y - 1, pos.z });
        const bool upper = below == bs;   // the upper half sits on its own lower half
        const bool dirIsY = direction == 0 || direction == 1;   // DOWN/UP
        const bool dirIsUp = direction == 1;
        // neighbour = pos.relative(direction); its half by the same inference
        const BlockPos npos = mc::levelgen::feature::treeRelative(pos, direction);
        const std::string nstate = level.getBlockState(npos);
        bool neighbourSameOtherHalf = false;
        if (nstate == bs) {
            const bool nUpper = level.getBlockState(BlockPos{ npos.x, npos.y - 1, npos.z }) == bs;
            neighbourSameOtherHalf = nUpper != upper;
        }
        if (dirIsY && !((!upper) != dirIsUp) && !neighbourSameOtherHalf) {
            // axis==Y && (half==LOWER) == (dir==UP) && !(neighbour is same block other half)
            return "minecraft:air";
        }
        if (!upper && direction == 0 && !level.canSurvive(bs, pos)) {
            return "minecraft:air";
        }
        return bs;
    }
    throw std::logic_error("updateShape not ported for " + bs);
}

// Block.updateFromNeighbourShapes (Block.java:204-214, UPDATE_SHAPE_ORDER =
// WEST,EAST,NORTH,SOUTH,DOWN,UP per BlockBehaviour.java:85-87) evaluated for the
// closed set of non-liquid blocks that can be marked here. Static full blocks use
// BlockBehaviour's identity updateShape; glow_lichen runs MultifaceBlock.updateShape
// exactly (face revalidation, MultifaceBlock.java:135-143 + removeFace:263-266);
// the aquatic plants' conversion/removal branches cannot fire under the placement
// invariants — asserted, throwing if one ever would (port it then, never guess).
// The land plants (grass/litter/flowers/double plants) DO legitimately revalidate
// here (LevelChunk.postProcessGeneration writes the result with flags 276) — they
// fold through updateShapeOnce.
void postUpdateFromNeighbourShapes(MultiChunkLevel& level, BlockPos bp, const std::string& bs) {
    static const std::set<std::string> staticNoOp = {
        // Block/BlockBehaviour default updateShape returns the state unchanged.
        "minecraft:air", "minecraft:cave_air", "minecraft:void_air",
        "minecraft:stone", "minecraft:granite", "minecraft:diorite",
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
        // LeavesBlock (tick-only), logs, dungeon shell, chest/spawner/bee_nest.
        "minecraft:oak_leaves", "minecraft:birch_leaves", "minecraft:spruce_leaves",
        "minecraft:oak_log", "minecraft:birch_log", "minecraft:spruce_log",
        "minecraft:cobblestone", "minecraft:mossy_cobblestone",
        "minecraft:chest", "minecraft:spawner", "minecraft:bee_nest",
        // SnowyBlock UP -> SNOWY property only; cactus/sugar_cane/slab/brushable
        // updateShape are tick-only (see updateShapeOnce); geode shell = Block default.
        "minecraft:mycelium", "minecraft:cactus", "minecraft:sugar_cane",
        "minecraft:sandstone_slab", "minecraft:suspicious_sand",
        "minecraft:amethyst_block", "minecraft:budding_amethyst",
        "minecraft:calcite", "minecraft:smooth_basalt",
        "minecraft:moss_block", "minecraft:rooted_dirt", "minecraft:dripstone_block",
        "minecraft:raw_iron_block", "minecraft:raw_copper_block", "minecraft:raw_gold_block",
        "minecraft:azalea_leaves", "minecraft:flowering_azalea_leaves",
        "minecraft:pointed_dripstone",
        // New-class static/tick-only updateShapes (see updateShapeOnce idNoOp).
        "minecraft:jungle_log", "minecraft:acacia_log", "minecraft:dark_oak_log",
        "minecraft:cherry_log", "minecraft:mangrove_log", "minecraft:pale_oak_log",
        "minecraft:jungle_leaves", "minecraft:acacia_leaves", "minecraft:dark_oak_leaves",
        "minecraft:cherry_leaves", "minecraft:mangrove_leaves", "minecraft:pale_oak_leaves",
        "minecraft:brown_mushroom_block", "minecraft:red_mushroom_block", "minecraft:mushroom_stem",
        "minecraft:mangrove_roots", "minecraft:muddy_mangrove_roots",
        "minecraft:pale_moss_block", "minecraft:creaking_heart", "minecraft:sculk_catalyst",
        "minecraft:bone_block",
        "minecraft:tube_coral_block", "minecraft:brain_coral_block", "minecraft:bubble_coral_block",
        "minecraft:fire_coral_block", "minecraft:horn_coral_block",
        "minecraft:terracotta", "minecraft:white_terracotta", "minecraft:orange_terracotta",
        "minecraft:yellow_terracotta", "minecraft:brown_terracotta", "minecraft:red_terracotta",
        "minecraft:light_gray_terracotta", "minecraft:red_sandstone",
        "minecraft:bamboo", "minecraft:pale_hanging_moss", "minecraft:sculk",
        "minecraft:sculk_sensor", "minecraft:sculk_shrieker",
        "minecraft:powder_snow",   // PowderSnowBlock: no updateShape override
    };
    if (staticNoOp.count(bs) != 0) return;

    // Land plants: fold through the six UPDATE_SHAPE_ORDER directions exactly as
    // Block.updateFromNeighbourShapes, writing the result if changed (flags 276,
    // LevelChunk.postProcessGeneration / the Java harness :415-417).
    static const std::set<std::string> landPlants = {
        "minecraft:short_grass", "minecraft:fern", "minecraft:bush", "minecraft:firefly_bush",
        "minecraft:lily_of_the_valley", "minecraft:dandelion", "minecraft:poppy",
        "minecraft:blue_orchid", "minecraft:allium", "minecraft:azure_bluet",
        "minecraft:red_tulip", "minecraft:orange_tulip", "minecraft:white_tulip",
        "minecraft:pink_tulip", "minecraft:oxeye_daisy", "minecraft:cornflower",
        "minecraft:sweet_berry_bush", "minecraft:brown_mushroom", "minecraft:red_mushroom",
        "minecraft:dead_bush", "minecraft:short_dry_grass", "minecraft:tall_dry_grass",
        "minecraft:oak_sapling", "minecraft:birch_sapling", "minecraft:spruce_sapling",
        "minecraft:leaf_litter",
        "minecraft:lilac", "minecraft:rose_bush", "minecraft:peony",
        "minecraft:tall_grass", "minecraft:large_fern", "minecraft:sunflower",
        "minecraft:pumpkin", "minecraft:melon",
        "minecraft:lily_pad", "minecraft:snow",
        "minecraft:wildflowers", "minecraft:pink_petals",
        "minecraft:hanging_roots", "minecraft:spore_blossom", "minecraft:big_dripleaf",
        "minecraft:azalea", "minecraft:flowering_azalea", "minecraft:moss_carpet",
        // VineBlock: updateShapeOnce folds its per-direction face revalidation
        // (VineBlock.java:128-143) over the UPDATE_SHAPE_ORDER directions.
        "minecraft:vine",
        // New-class revalidating blocks (fold through updateShapeOnce).
        "minecraft:jungle_sapling", "minecraft:acacia_sapling", "minecraft:dark_oak_sapling",
        "minecraft:cherry_sapling", "minecraft:pale_oak_sapling",
        "minecraft:closed_eyeblossom", "minecraft:open_eyeblossom",
        "minecraft:cocoa", "minecraft:mangrove_propagule", "minecraft:pale_moss_carpet",
        "minecraft:sea_pickle",
        "minecraft:tube_coral", "minecraft:brain_coral", "minecraft:bubble_coral",
        "minecraft:fire_coral", "minecraft:horn_coral",
        "minecraft:tube_coral_fan", "minecraft:brain_coral_fan", "minecraft:bubble_coral_fan",
        "minecraft:fire_coral_fan", "minecraft:horn_coral_fan",
        "minecraft:tube_coral_wall_fan", "minecraft:brain_coral_wall_fan", "minecraft:bubble_coral_wall_fan",
        "minecraft:fire_coral_wall_fan", "minecraft:horn_coral_wall_fan",
    };
    if (landPlants.count(bs) != 0) {
        static const int order[6] = { 4, 5, 2, 3, 0, 1 };
        std::string current = bs;
        for (int dir : order) {
            if (current != bs) break;   // once AIR, further updateShape is the air no-op
            current = updateShapeOnce(level, current, bp, dir, mc::levelgen::feature::treeRelative(bp, dir));
        }
        if (current != bs) level.setBlockChecked(bp, current, 276);
        return;
    }

    if (bs == "minecraft:glow_lichen" || bs == "minecraft:sculk_vein") {
        // MultifaceBlock.updateShape per neighbour direction: hasFace(direction) &&
        // !canAttachTo -> removeFace; no faces left -> AIR (MultifaceBlock.java:
        // 135-143, 263-266). UPDATE_SHAPE_ORDER = WEST,EAST,NORTH,SOUTH,DOWN,UP.
        // sculk_vein shares MultifaceBlock.updateShape verbatim (SculkVeinBlock
        // has no updateShape override).
        namespace md = mc::levelgen::feature::multiface_detail;
        mc::levelgen::feature::MultifaceBlockState st = level.multifaceStateAt(bp);
        static const int order[6] = { 4, 5, 2, 3, 0, 1 };
        bool changed = false;
        static constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };
        for (int dir : order) {
            if (md::hasFace(st, dir) && !md::canAttachTo(level, md::relative(bp, dir), OPP[dir])) {
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
        if (bs == "minecraft:glow_lichen" || bs == "minecraft:sculk_vein")
            hasFluid = level.multifaceStateAt(bp).waterlogged;
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

// ====================== shared decoration core (parity main + engine) ======================
// The following pieces were mechanically extracted from the parity main so the
// engine can run the EXACT SAME certified machinery (no behavioural change; the
// parity gates prove it): main-locals became DecorationResolver members, the
// mutually recursive loader lambdas became std::function members, and the
// per-chunk decoration turn became decorateOneChunk.

// Direction name -> tree-encoding index (DOWN,UP,N,S,W,E).
int parseDirection(const std::string& d) {
    if (d == "down") return 0;
    if (d == "up") return 1;
    if (d == "north") return 2;
    if (d == "south") return 3;
    if (d == "west") return 4;
    if (d == "east") return 5;
    throw std::runtime_error("unsupported direction: " + d);
}

bool isFaceSturdyUpState(const std::string& s) {
    bool defaulted = false;
    const bool r = mc::block::isFaceSturdyUp(s, &defaulted);
    if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(s));
    return r;
}

// TreeConfiguration.below_trunk_provider codec default: PLACE_BELOW_OVERWORLD_TRUNKS
// = rule_based(if !#cannot_replace_below_tree_trunk then dirt), no fallback
// (TreeConfiguration.java:5-10, codec orElse).
mc::levelgen::feature::DiskStateProvider defaultBelowTrunkProvider() {
    return [](WorldGenLevel& level, RandomSource&, BlockPos pos) -> std::optional<std::string> {
        if (!g_tags->isInTag(mc::block::blockName(level.getBlockState(pos)), "minecraft:cannot_replace_below_tree_trunk"))
            return std::optional<std::string>("minecraft:dirt");
        return std::nullopt;
    };
}

constexpr int genMinY = mc::CHUNK_MIN_Y, genDepth = mc::CHUNK_MAX_Y - mc::CHUNK_MIN_Y;

// All certified placed/configured-feature loading machinery, as one long-lived
// object. Construction loads block/fluid tags, biome feature lists, per-biome
// climate and the global FeatureSorter step data from `dataDir`
// (…/data/minecraft) and binds g_tags / g_fluidTags / g_biomeClimate.
// NOT thread-safe; must outlive every cached placer (main-thread only).
struct DecorationResolver {
    std::string dataDir;
    mc::block::BlockTags tags;        // data/minecraft/tags/block
    mc::block::BlockTags fluidTags;   // data/minecraft/tags/fluid
    BiomeFeatures biomeFeatures;

    // Global per-step feature order/index (== the setFeatureSeed index), over all biomes.
    std::vector<std::string> sources;
    std::set<std::string> sourcesSet;
    std::vector<FeatureSorter::StepFeatureData> stepData;

    // Resolve every placed_feature whose configured type is ported (ore, seagrass,
    // kelp) once; cache by feature key. Unported types are hard no-ops, counted.
    std::set<std::string> oreFamily;
    const std::set<std::string> vegetalFamily = {
        "minecraft:seagrass", "minecraft:tall_seagrass", "minecraft:kelp", "minecraft:kelp_plant",
    };
    std::map<std::string, std::shared_ptr<PlacedFeature>> cache;   // key -> placed (nullptr if unported)

    // ---- shared hooks for the feature families, routed through g_level/g_tags ----
    std::shared_ptr<mc::levelgen::feature::TreeHooks> treeHooks;
    std::shared_ptr<mc::levelgen::feature::MonsterRoomHooks> monsterHooks;
    std::shared_ptr<mc::levelgen::feature::SnowFreezeHooks> snowFreezeHooks;
    std::shared_ptr<mc::levelgen::feature::LakeHooks> lakeHooks;
    std::shared_ptr<mc::levelgen::feature::GeodeHooks> geodeHooks;
    std::shared_ptr<mc::levelgen::feature::CaveFeatureHooks> caveHooks;
    std::shared_ptr<mc::levelgen::feature::DripstoneHooks> dripstoneHooks;
    std::shared_ptr<mc::levelgen::feature::HugeMushroomHooks> hugeMushroomHooks;
    std::shared_ptr<mc::levelgen::feature::CoralHooks> coralHooks;
    std::shared_ptr<mc::levelgen::feature::SculkHooks> sculkHooks;

    // Mutually recursive loaders, bound in the ctor (capturing `this`): tree
    // decorators (pale_moss) and several configured types reference other
    // configured/placed features.
    std::function<std::shared_ptr<PlacedFeature>(const std::string&)> resolveFeature;
    std::function<PlacedFeature::FeaturePlacer(const json&)> loadConfiguredPlacer;
    std::function<std::shared_ptr<PlacedFeature>(const json&, const std::string&)> loadPlacedFromJson;
    std::function<mc::levelgen::feature::TreeDecoratorConfig(const json&)> loadTreeDecorator;
    std::function<mc::levelgen::feature::FallenTreeDecoratorConfig(const json&)> loadFallenTreeDecorator;
    std::function<std::shared_ptr<PlacedFeature>(const json&, const std::string&)> resolvePlacedRef;

    DecorationResolver(const DecorationResolver&) = delete;
    DecorationResolver& operator=(const DecorationResolver&) = delete;

    explicit DecorationResolver(const std::string& dataDirIn)
        : dataDir(dataDirIn),
          tags(mc::block::BlockTags::loadFromDirectory(dataDirIn + "/tags/block")),
          // Fluid tags: TallSeagrassBlock.canSurvive consults FluidTags.WATER (the Java
          // ground truth binds them too — unbound tags silently test false and tall
          // seagrass never places).
          fluidTags(mc::block::BlockTags::loadFromDirectory(dataDirIn + "/tags/fluid")),
          biomeFeatures(BiomeFeatures::loadFromDirectory(dataDirIn + "/worldgen/biome")) {
    g_tags = &tags;
    g_fluidTags = &fluidTags;

    // Per-biome climate (Biome.ClimateSettings codec fields straight from the biome
    // JSONs: temperature, has_precipitation, optional temperature_modifier) for
    // freeze_top_layer / lake-rim freezing. Fail-closed: unknown biome -> throw.
    {
        namespace fs = std::filesystem;
        for (const auto& entry : fs::directory_iterator(dataDir + "/worldgen/biome")) {
            if (entry.path().extension() != ".json") continue;
            std::ifstream f(entry.path());
            json bj; f >> bj;
            mc::levelgen::feature::BiomeClimate climate;
            climate.temperature = bj.at("temperature").get<float>();
            climate.hasPrecipitation = bj.at("has_precipitation").get<bool>();
            climate.frozenModifier = bj.value("temperature_modifier", std::string("none")) == "frozen";
            g_biomeClimate["minecraft:" + entry.path().stem().string()] = climate;
        }
    }

    sources = BiomeSource::collectOverworldPossibleBiomes();
    sourcesSet = std::set<std::string>(sources.begin(), sources.end());
    stepData = FeatureSorter::buildFeaturesPerStep(sources, biomeFeatures, true);

    // ---- shared hooks for the tree/dungeon family, routed through g_level/g_tags ----
    treeHooks = std::make_shared<mc::levelgen::feature::TreeHooks>();
    treeHooks->isAir = [](const std::string& s) { return mc::block::isAirBlock(s); };
    treeHooks->validTreePosState = [](const std::string& s) {   // TreeFeature.validTreePos
        return mc::block::isAirBlock(s) || g_tags->isInTag(mc::block::blockName(s), "minecraft:replaceable_by_trees");
    };
    treeHooks->isLog = [](const std::string& s) { return g_tags->isInTag(mc::block::blockName(s), "minecraft:logs"); };
    treeHooks->isVine = [](const std::string& s) { return mc::block::blockName(s) == "minecraft:vine"; };
    treeHooks->isSolidRender = [](const std::string& s) {
        bool defaulted = false;
        const bool r = mc::block::isSolidRender(s, &defaulted);
        if (defaulted) g_solidRenderDefaulted.insert(mc::block::blockName(s));
        return r;
    };
    treeHooks->optionalDistanceAt = [](BlockPos p) -> std::optional<int> {
        // LeavesBlock.getOptionalDistanceAt (LeavesBlock.java:131-137).
        const std::string s = g_level->getBlockState(p);
        if (g_tags->isInTag(mc::block::blockName(s), "minecraft:prevents_nearby_leaf_decay")) return 0;
        if (mc::block::isLeavesBlock(s)) return g_level->leafDistanceAt(p);
        return std::nullopt;
    };
    treeHooks->setLeafDistance = [](BlockPos p, int d) {
        // Trace parity with the Java GT: setBlockKnownShape routes through the proxy
        // setBlock (logged as PUT when it lands), id unchanged.
        if (const char* tw = std::getenv("MCPP_TRACE_WRITES");
            tw != nullptr && g_curFeatureKey.find(tw) != std::string::npos && g_level->ensureCanWrite(p)) {
            std::cerr << "PUT\t" << g_curTurnCx << "," << g_curTurnCz << "\t" << g_curFeatureKey
                      << "\t" << p.x << "\t" << p.y << "\t" << p.z << "\t"
                      << mc::block::blockName(g_level->getBlockState(p)) << "\n";
        }
        g_level->setLeafDistance(p, d);
    };
    treeHooks->putVineFace = [](BlockPos p, int d) { g_level->putVineFaces(p, static_cast<std::uint8_t>(1u << d)); };
    treeHooks->isLeavesTag = [](const std::string& s) { return g_tags->isInTag(mc::block::blockName(s), "minecraft:leaves"); };
    treeHooks->isLogsTag = [](const std::string& s) { return g_tags->isInTag(mc::block::blockName(s), "minecraft:logs"); };
    treeHooks->putCocoaFacing = [](BlockPos p, int d) { g_level->putFacing(p, d); };
    treeHooks->updateShapeFace = [](BlockPos pos, int dir, BlockPos neighborPos) {
        // StructureTemplate.updateShapeAtEdge body (StructureTemplate.java:416-436),
        // updateMode 3 -> writes with 3 & -2 == 2.
        static constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };
        MultiChunkLevel& level = *g_level;
        const std::string state = level.getBlockState(pos);
        const std::string newState = updateShapeOnce(level, state, pos, dir, neighborPos);
        if (newState != state) level.setBlockChecked(pos, newState, 2);
        const std::string nstate = level.getBlockState(neighborPos);
        const std::string newN = updateShapeOnce(level, nstate, neighborPos, OPP[dir], pos);
        if (newN != nstate) level.setBlockChecked(neighborPos, newN, 2);
    };
    treeHooks->levelMinY = mc::CHUNK_MIN_Y;
    treeHooks->levelMaxY = mc::CHUNK_MAX_Y - 1;

    monsterHooks = std::make_shared<mc::levelgen::feature::MonsterRoomHooks>();
    monsterHooks->isSolid = [](const std::string& s) {
        bool defaulted = false;
        const bool r = mc::block::isSolid(s, &defaulted);
        if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(s));
        return r;
    };
    monsterHooks->isAir = [](const std::string& s) { return mc::block::isAirBlock(s); };
    monsterHooks->featuresCannotReplace = [](const std::string& s) {
        return g_tags->isInTag(mc::block::blockName(s), "minecraft:features_cannot_replace");
    };
    monsterHooks->levelMinY = mc::CHUNK_MIN_Y;

    // ---- hooks for freeze_top_layer / lake / geode ----
    snowFreezeHooks = std::make_shared<mc::levelgen::feature::SnowFreezeHooks>();
    snowFreezeHooks->getBiome = [](BlockPos pos) { return g_biomeCtx->zoomBiome(pos); };
    snowFreezeHooks->climate = [](const std::string& biome) -> const mc::levelgen::feature::BiomeClimate& {
        auto it = g_biomeClimate.find(biome);
        if (it == g_biomeClimate.end()) throw std::logic_error("no climate loaded for biome " + biome);
        return it->second;
    };
    snowFreezeHooks->isAir = [](const std::string& s) { return mc::block::isAirBlock(s); };
    snowFreezeHooks->snowCanSurvive = [](BlockPos pos) {
        // SnowLayerBlock.canSurvive — one implementation, on the level (also used
        // by SnowLayerBlock.updateShape during tree-edge/postprocess revalidation).
        return g_level->canSurvive("minecraft:snow", pos);
    };
    snowFreezeHooks->hasSnowyProperty = [](const std::string& s) {
        // SnowyBlock.SNOWY holders: the SnowyDirtBlock registrations (Blocks.java:
        // grass_block GrassBlock, podzol SnowyDirtBlock, mycelium MyceliumBlock).
        const std::string b = mc::block::blockName(s);
        return b == "minecraft:grass_block" || b == "minecraft:podzol" || b == "minecraft:mycelium";
    };
    snowFreezeHooks->seaLevel = 63;   // NoiseGeneratorSettings.overworld sea_level
    snowFreezeHooks->levelMinY = mc::CHUNK_MIN_Y;
    snowFreezeHooks->levelMaxY = mc::CHUNK_MAX_Y;

    lakeHooks = std::make_shared<mc::levelgen::feature::LakeHooks>();
    lakeHooks->featuresCannotReplace = [](const std::string& s) {
        return g_tags->isInTag(mc::block::blockName(s), "minecraft:features_cannot_replace");
    };
    lakeHooks->lavaPoolStoneCannotReplace = [](const std::string& s) {
        return g_tags->isInTag(mc::block::blockName(s), "minecraft:lava_pool_stone_cannot_replace");
    };
    lakeHooks->isSolid = [](const std::string& s) {
        bool defaulted = false;
        const bool r = mc::block::isSolid(s, &defaulted);
        if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(s));
        return r;
    };
    lakeHooks->isLiquid = [](const std::string& s) {
        // BlockStateBase.liquid(): the LiquidBlock states only.
        return s == "minecraft:water" || s == "minecraft:lava";
    };
    lakeHooks->isAir = [](const std::string& s) { return mc::block::isAirBlock(s); };
    lakeHooks->markAboveForPostProcessing = [](BlockPos placePos) {
        // Feature.markAboveForPostProcessing (Feature.java:206-217).
        BlockPos pos = placePos;
        for (int i = 0; i < 2; ++i) {
            pos = BlockPos{ pos.x, pos.y + 1, pos.z };
            if (mc::block::isAirBlock(g_level->getBlockState(pos))) return;
            g_level->markPosForPostprocessing(pos);
        }
    };
    lakeHooks->countSkippedScheduleTick = [] { ++g_skippedScheduleTicks; };
    lakeHooks->snowFreeze = snowFreezeHooks;

    geodeHooks = std::make_shared<mc::levelgen::feature::GeodeHooks>();
    geodeHooks->isAir = [](const std::string& s) { return mc::block::isAirBlock(s); };
    geodeHooks->levelSeed = [] { return static_cast<std::int64_t>(g_curLevelSeed); };
    geodeHooks->countSkippedScheduleTick = [] { ++g_skippedScheduleTicks; };

    caveHooks = std::make_shared<mc::levelgen::feature::CaveFeatureHooks>();
    caveHooks->isAir = [](const std::string& s) { return mc::block::isAirBlock(s); };
    caveHooks->isFaceSturdyFull = [](const std::string& s, int dir) {
        bool defaulted = false;
        const bool r = mc::block::isFaceSturdyFull(s, dir, &defaulted);
        if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(s));
        return r;
    };
    caveHooks->isSolid = [](const std::string& s) {
        bool defaulted = false;
        const bool r = mc::block::isSolid(s, &defaulted);
        if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(s));
        return r;
    };
    caveHooks->isWaterFluid = [](const std::string& s) {
        return mc::material::fluidStateOf(s).is(*g_fluidTags, "minecraft:water");
    };
    caveHooks->isLavaFluid = [](const std::string& s) {
        return mc::material::fluidStateOf(s).is(*g_fluidTags, "minecraft:lava");
    };

    dripstoneHooks = std::make_shared<mc::levelgen::feature::DripstoneHooks>();
    dripstoneHooks->isAir = [](const std::string& s) { return mc::block::isAirBlock(s); };
    dripstoneHooks->dripstoneReplaceable = [](const std::string& s) {
        return g_tags->isInTag(mc::block::blockName(s), "minecraft:dripstone_replaceable_blocks");
    };
    dripstoneHooks->baseStoneOverworld = [](const std::string& s) {
        return g_tags->isInTag(mc::block::blockName(s), "minecraft:base_stone_overworld");
    };
    dripstoneHooks->isWaterFluid = [](const std::string& s) {
        return mc::material::fluidStateOf(s).is(*g_fluidTags, "minecraft:water");
    };

    // ---- hooks for huge mushrooms / bamboo / corals / sculk ----
    hugeMushroomHooks = std::make_shared<mc::levelgen::feature::HugeMushroomHooks>();
    hugeMushroomHooks->isAir = [](const std::string& s) { return mc::block::isAirBlock(s); };
    hugeMushroomHooks->isLeavesTag = [](const std::string& s) {
        return g_tags->isInTag(mc::block::blockName(s), "minecraft:leaves");
    };
    hugeMushroomHooks->replaceableByMushrooms = [](const std::string& s) {
        return g_tags->isInTag(mc::block::blockName(s), "minecraft:replaceable_by_mushrooms");
    };
    hugeMushroomHooks->levelMinY = mc::CHUNK_MIN_Y;
    hugeMushroomHooks->levelMaxY = mc::CHUNK_MAX_Y - 1;

    // Ordered tag lists for Registry.getRandomElementOf (HolderSet order = tag-file
    // order with nested refs expanded in place; the coral tags are flat except
    // #corals = #coral_plants + the fans — see CoralFeatures.h header).
    auto orderedTagList = [this](const std::string& tagPath) {
        std::vector<std::string> out;
        std::set<std::string> seen;
        std::function<void(const std::string&)> expand = [&](const std::string& path) {
            std::ifstream f(dataDir + "/tags/block/" + path + ".json");
            if (!f) throw std::runtime_error("no block tag " + path);
            json tj; f >> tj;
            for (const auto& v : tj.at("values")) {
                const std::string e = v.get<std::string>();
                if (!e.empty() && e[0] == '#') expand(stripNs(e.substr(1)));
                else if (seen.insert("minecraft:" + stripNs(e)).second) out.push_back("minecraft:" + stripNs(e));
            }
        };
        expand(tagPath);
        return out;
    };
    coralHooks = std::make_shared<mc::levelgen::feature::CoralHooks>();
    coralHooks->coralBlocksTag = orderedTagList("coral_blocks");
    coralHooks->coralsTag = orderedTagList("corals");
    coralHooks->wallCoralsTag = orderedTagList("wall_corals");
    coralHooks->isCoralsTag = [](const std::string& s) {
        return g_tags->isInTag(mc::block::blockName(s), "minecraft:corals");
    };
    coralHooks->putWallFanFacing = [](BlockPos p, int d) { g_level->putFacing(p, d); };

    // Sculk spreader configs (SculkVeinBlock.java:24-28): both use the
    // SculkVeinSpreaderConfig replaced-state rules; sameSpace = SAME_POSITION only.
    auto makeSculkVeinSpreaderConfig = [&](bool sameSpaceOnly) {
        auto cfg = std::make_shared<mc::levelgen::feature::multiface_detail::Config>();
        cfg->blockId = "minecraft:sculk_vein";
        cfg->sculkVeinConfig = true;
        if (sameSpaceOnly) cfg->spreadTypes = { 0 };
        cfg->hooks.getState = [](BlockPos p) {
            auto s = g_level->multifaceStateAt(p);
            s.isPlaceBlock = s.isPlaceBlock && s.block == "minecraft:sculk_vein";
            return s;
        };
        cfg->hooks.putState = [](BlockPos p, std::uint8_t faces, bool waterlogged) {
            g_level->putMultifaceState(p, faces, waterlogged);
        };
        cfg->isFireTag = [](const std::string& s) {
            return g_tags->isInTag(mc::block::blockName(s), "minecraft:fire");
        };
        cfg->canBeReplaced = [](const std::string& s) {
            return g_tags->isInTag(mc::block::blockName(s), "minecraft:replaceable");
        };
        return cfg;
    };
    sculkHooks = std::make_shared<mc::levelgen::feature::SculkHooks>();
    sculkHooks->isAir = [](const std::string& s) { return mc::block::isAirBlock(s); };
    sculkHooks->sculkReplaceableWorldGen = [](const std::string& s) {
        return g_tags->isInTag(mc::block::blockName(s), "minecraft:sculk_replaceable_world_gen");
    };
    sculkHooks->isCollisionShapeFullBlock = [](const std::string& s) {
        bool defaulted = false;
        const bool r = mc::block::isCollisionShapeFullBlock(s, &defaulted);
        if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(s));
        return r;
    };
    sculkHooks->isFaceSturdy = [](const std::string& s, int dir) {
        bool defaulted = false;
        const bool r = mc::block::isFaceSturdyFull(s, dir, &defaulted);
        if (defaulted) g_blocksMotionDefaulted.insert(mc::block::blockName(s));
        return r;
    };
    sculkHooks->isWaterFluid = [](const std::string& s) {
        return mc::material::fluidStateOf(s).is(*g_fluidTags, "minecraft:water");
    };
    sculkHooks->veinSpreader = makeSculkVeinSpreaderConfig(false);
    sculkHooks->sameSpaceSpreader = makeSculkVeinSpreaderConfig(true);

    loadTreeDecorator = [&](const json& dj) -> mc::levelgen::feature::TreeDecoratorConfig {
        mc::levelgen::feature::TreeDecoratorConfig dec;
        const std::string dtype = stripNs(dj.at("type").get<std::string>());
        if (dtype == "beehive") {
            dec.kind = mc::levelgen::feature::TreeDecoratorConfig::Kind::Beehive;
            dec.probability = dj.at("probability").get<float>();
        } else if (dtype == "place_on_ground") {
            // PlaceOnGroundDecorator codec defaults: tries 128, radius 2, height 1.
            dec.kind = mc::levelgen::feature::TreeDecoratorConfig::Kind::PlaceOnGround;
            dec.tries = dj.value("tries", 128);
            dec.radius = dj.value("radius", 2);
            dec.height = dj.value("height", 1);
            dec.provider = loadStateProvider(dj.at("block_state_provider"));
        } else if (dtype == "alter_ground") {
            // AlterGroundDecorator codec: provider (AlterGroundDecorator.java:11).
            dec.kind = mc::levelgen::feature::TreeDecoratorConfig::Kind::AlterGround;
            dec.provider = loadStateProvider(dj.at("provider"));
        } else if (dtype == "leave_vine") {
            // LeaveVineDecorator codec: probability (LeaveVineDecorator.java:11-13).
            dec.kind = mc::levelgen::feature::TreeDecoratorConfig::Kind::LeaveVine;
            dec.probability = dj.at("probability").get<float>();
        } else if (dtype == "cocoa") {
            // CocoaDecorator codec: probability (CocoaDecorator.java:14-16).
            dec.kind = mc::levelgen::feature::TreeDecoratorConfig::Kind::Cocoa;
            dec.probability = dj.at("probability").get<float>();
        } else if (dtype == "trunk_vine") {
            // TrunkVineDecorator: unit codec (TrunkVineDecorator.java:9-10).
            dec.kind = mc::levelgen::feature::TreeDecoratorConfig::Kind::TrunkVine;
        } else if (dtype == "attached_to_leaves") {
            // AttachedToLeavesDecorator codec (AttachedToLeavesDecorator.java:19-29).
            dec.kind = mc::levelgen::feature::TreeDecoratorConfig::Kind::AttachedToLeaves;
            dec.probability = dj.at("probability").get<float>();
            dec.exclusionRadiusXZ = dj.at("exclusion_radius_xz").get<int>();
            dec.exclusionRadiusY = dj.at("exclusion_radius_y").get<int>();
            dec.provider = loadStateProvider(dj.at("block_provider"));
            dec.requiredEmptyBlocks = dj.at("required_empty_blocks").get<int>();
            for (const auto& d : dj.at("directions")) {
                const std::string ds = d.get<std::string>();
                int dirIdx;
                if (ds == "down") dirIdx = 0; else if (ds == "up") dirIdx = 1;
                else if (ds == "north") dirIdx = 2; else if (ds == "south") dirIdx = 3;
                else if (ds == "west") dirIdx = 4; else if (ds == "east") dirIdx = 5;
                else throw std::runtime_error("attached_to_leaves: bad direction " + ds);
                dec.directions.push_back(dirIdx);
            }
            if (dec.directions.empty()) throw std::runtime_error("attached_to_leaves: empty directions");
        } else if (dtype == "pale_moss") {
            // PaleMossDecorator codec (PaleMossDecorator.java:21-27); the ground
            // patch is the PALE_MOSS_PATCH configured feature placed directly
            // (PaleMossDecorator.java:55-60) — resolved here once.
            dec.kind = mc::levelgen::feature::TreeDecoratorConfig::Kind::PaleMoss;
            dec.probability = dj.at("leaves_probability").get<float>();
            dec.trunkProbability = dj.at("trunk_probability").get<float>();
            dec.groundProbability = dj.at("ground_probability").get<float>();
            json patchJson;
            {
                std::ifstream f(dataDir + "/worldgen/configured_feature/pale_moss_patch.json");
                if (!f) throw std::runtime_error("no configured_feature pale_moss_patch");
                f >> patchJson;
            }
            dec.paleMossPatch = loadConfiguredPlacer(patchJson);
        } else if (dtype == "creaking_heart") {
            // CreakingHeartDecorator codec: probability (CreakingHeartDecorator.java:18-21).
            dec.kind = mc::levelgen::feature::TreeDecoratorConfig::Kind::CreakingHeart;
            dec.probability = dj.at("probability").get<float>();
        } else {
            throw std::runtime_error("unsupported tree decorator: " + dtype);
        }
        return dec;
    };
    loadFallenTreeDecorator = [&](const json& dj) -> mc::levelgen::feature::FallenTreeDecoratorConfig {
        mc::levelgen::feature::FallenTreeDecoratorConfig dec;
        const std::string dtype = stripNs(dj.at("type").get<std::string>());
        if (dtype == "trunk_vine") {
            dec.kind = mc::levelgen::feature::FallenTreeDecoratorConfig::Kind::TrunkVine;
        } else if (dtype == "attached_to_logs") {
            dec.kind = mc::levelgen::feature::FallenTreeDecoratorConfig::Kind::AttachedToLogs;
            dec.probability = dj.at("probability").get<float>();
            dec.provider = loadStateProvider(dj.at("block_provider"));
            for (const auto& d : dj.at("directions")) dec.directions.push_back(parseDirection(d.get<std::string>()));
            if (dec.directions.empty()) throw std::runtime_error("attached_to_logs: empty directions");
        } else {
            throw std::runtime_error("unsupported fallen-tree decorator: " + dtype);
        }
        return dec;
    };

    // Mutually recursive loaders (declared above loadTreeDecorator): a configured
    // feature can reference placed features (random_selector entries are registry
    // refs; simple_random_selector entries are inline placed features). An UNPORTED
    // nested feature aborts the whole parent (the parent becomes a hard no-op —
    // never a partial run with desynced RNG).
    loadPlacedFromJson = [&](const json& placedJson, const std::string& biomeKey) -> std::shared_ptr<PlacedFeature> {
        PlacedFeature::FeaturePlacer placer;
        const json& f = placedJson.at("feature");
        if (f.is_string()) {
            json cfgJson;
            std::ifstream ff(dataDir + "/worldgen/configured_feature/" + stripNs(f.get<std::string>()) + ".json");
            if (!ff) throw std::runtime_error("no configured_feature " + f.get<std::string>());
            ff >> cfgJson;
            placer = loadConfiguredPlacer(cfgJson);
        } else {
            placer = loadConfiguredPlacer(f);
        }
        std::vector<std::shared_ptr<const PlacementModifier>> mods;
        if (placedJson.contains("placement"))
            for (const auto& m : placedJson.at("placement")) mods.push_back(loadModifier(m, biomeKey));
        return std::make_shared<PlacedFeature>(placer, mods);
    };

    // Placed-feature reference inside a configured feature: a registry id string or
    // an inline placed-feature object.
    resolvePlacedRef = [&](const json& ref, const std::string& parentKey) -> std::shared_ptr<PlacedFeature> {
        if (ref.is_string()) {
            const std::string id = "minecraft:" + stripNs(ref.get<std::string>());
            std::shared_ptr<PlacedFeature> child = resolveFeature(id);
            if (!child) throw std::runtime_error("nested placed feature unported: " + id);
            return child;
        }
        return loadPlacedFromJson(ref, parentKey);
    };

    loadConfiguredPlacer = [&](const json& cfgJson) -> PlacedFeature::FeaturePlacer {
        const std::string type = stripNs(cfgJson.at("type").get<std::string>());
        // Bisection aid: MCPP_DISABLE_FEATURES="tree,fallen_tree,..." turns the listed
        // types back into hard no-ops.
        if (const char* dis = std::getenv("MCPP_DISABLE_FEATURES");
            dis != nullptr && (std::string(",") + dis + ",").find("," + type + ",") != std::string::npos) {
            throw UnportedFeatureType{ type };
        }
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
                // OreFeature writes through BulkSectionAccess (OreFeature.java:110)
                // — raw section writes that never prime/update heightmaps. Flag the
                // level so its FINAL heightmaps stay stale until the next resync.
                placer = [inner = mc::levelgen::feature::makeOrePlacer(std::move(targets), size, discard)](
                             WorldGenLevel& level, RandomSource& random, BlockPos origin) {
                    g_level->setBulkWriting(true);
                    const bool any = inner(level, random, origin);
                    g_level->setBulkWriting(false);
                    return any;
                };
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
                // false, chance_of_spreading orElse 0.5. glow_lichen uses the
                // default spreader; sculk_vein uses SculkVeinBlock.veinSpreader
                // (SculkVeinSpreaderConfig) for the chance_of_spreading step
                // (MultifaceGrowthFeature.java:76: config.placeBlock.getSpreader()).
                const json& cc = cfgJson.at("config");
                const std::string block = "minecraft:" + stripNs(cc.value("block", std::string("minecraft:glow_lichen")));
                if (block != "minecraft:glow_lichen" && block != "minecraft:sculk_vein")
                    throw std::runtime_error("multiface block not ported: " + block);
                mc::levelgen::feature::MultifaceLevelHooks hooks;
                hooks.getState = [block](BlockPos p) {
                    auto s = g_level->multifaceStateAt(p);
                    s.isPlaceBlock = s.isPlaceBlock && s.block == block;   // relative to placeBlock
                    return s;
                };
                hooks.putState = [](BlockPos p, std::uint8_t faces, bool waterlogged) {
                    g_level->putMultifaceState(p, faces, waterlogged);
                };
                // SculkVeinSpreaderConfig dependencies (BlockStateBase.canBeReplaced
                // == the #replaceable tag, generated from the replaceable property).
                hooks.isFireTag = [](const std::string& s) {
                    return g_tags->isInTag(mc::block::blockName(s), "minecraft:fire");
                };
                hooks.canBeReplaced = [](const std::string& s) {
                    return g_tags->isInTag(mc::block::blockName(s), "minecraft:replaceable");
                };
                placer = mc::levelgen::feature::makeMultifaceGrowthPlacer(
                    block,
                    loadIdSet(cc.at("can_be_placed_on")),
                    cc.value("search_range", 10),
                    cc.value("can_place_on_floor", false),
                    cc.value("can_place_on_ceiling", false),
                    cc.value("can_place_on_wall", false),
                    cc.value("chance_of_spreading", 0.5f),
                    std::move(hooks));
            } else if (type == "simple_block") {
                // SimpleBlockConfiguration: to_place + schedule_tick. schedule_tick
                // (pale garden eyeblossoms) is level.scheduleTick(origin, block, 1)
                // after the write (SimpleBlockFeature.java:39-41) — a ServerLevel
                // tick, hard no-op during worldgen exactly like the GT proxy's
                // scheduleTick (FullChunkDecorateParity.java:729-732); counted.
                const json& cc = cfgJson.at("config");
                const bool scheduleTick = cc.value("schedule_tick", false);
                auto inner = mc::levelgen::feature::makeSimpleBlockPlacer(
                    loadStateProvider(cc.at("to_place")),
                    [](const std::string& s) { return mc::block::isDoublePlant(s); },
                    [](const std::string& s) { return mc::block::isAirBlock(s); },
                    [](const std::string& s) { return mc::block::blockName(s) == "minecraft:pale_moss_carpet"; },
                    [](WorldGenLevel&, BlockPos origin) {
                        // MossyCarpetBlock.placeAt(level, origin, level.getRandom(), 2)
                        // (SimpleBlockFeature.java:33-35; MossyCarpetBlock.java:166-176).
                        mossy_carpet::placeAt(*g_level, origin, [](const std::string& s) {
                            return g_tags->isInTag(mc::block::blockName(s), "minecraft:replaceable");
                        });
                    });
                if (!scheduleTick) {
                    placer = std::move(inner);
                } else {
                    placer = [inner = std::move(inner)](WorldGenLevel& level, RandomSource& random, BlockPos origin) {
                        const bool ok = inner(level, random, origin);
                        if (ok) ++g_skippedScheduleTicks;
                        return ok;
                    };
                }
            } else if (type == "random_selector") {
                const json& cc = cfgJson.at("config");
                std::vector<mc::levelgen::feature::WeightedPlacedFeatureEntry> entries;
                for (const auto& fj : cc.at("features")) {
                    mc::levelgen::feature::WeightedPlacedFeatureEntry e;
                    e.chance = fj.at("chance").get<float>();
                    e.feature = resolvePlacedRef(fj.at("feature"), "?nested");
                    entries.push_back(std::move(e));
                }
                placer = mc::levelgen::feature::makeRandomSelectorPlacer(
                    std::move(entries), resolvePlacedRef(cc.at("default"), "?nested"), genMinY, genDepth);
            } else if (type == "simple_random_selector") {
                const json& cc = cfgJson.at("config");
                std::vector<std::shared_ptr<PlacedFeature>> feats;
                const json& fl = cc.at("features");
                if (fl.is_array()) {
                    for (const auto& fj : fl) feats.push_back(resolvePlacedRef(fj, "?nested"));
                } else {
                    feats.push_back(resolvePlacedRef(fl, "?nested"));
                }
                if (feats.empty()) throw std::runtime_error("simple_random_selector: empty features");
                placer = mc::levelgen::feature::makeSimpleRandomSelectorPlacer(std::move(feats), genMinY, genDepth);
            } else if (type == "tree") {
                const json& cc = cfgJson.at("config");
                auto tc = std::make_shared<mc::levelgen::feature::TreeConfig>();
                tc->trunkProvider = loadStateProvider(cc.at("trunk_provider"));
                tc->foliageProvider = loadStateProvider(cc.at("foliage_provider"));
                tc->belowTrunkProvider = cc.contains("below_trunk_provider")
                    ? loadStateProvider(cc.at("below_trunk_provider"))
                    : defaultBelowTrunkProvider();
                if (cc.contains("root_placer")) {
                    // MangroveRootPlacer codec (RootPlacer.rootPlacerParts +
                    // MangroveRootPlacement.CODEC + AboveRootPlacement.CODEC).
                    const json& rp = cc.at("root_placer");
                    if (stripNs(rp.at("type").get<std::string>()) != "mangrove_root_placer")
                        throw std::runtime_error("unsupported root_placer: " + rp.at("type").get<std::string>());
                    auto rc = std::make_shared<mc::levelgen::feature::MangroveRootConfig>();
                    rc->trunkOffsetY = loadIntProvider(rp.at("trunk_offset_y"));
                    rc->rootProvider = loadStateProvider(rp.at("root_provider"));
                    if (rp.contains("above_root_placement")) {
                        rc->hasAboveRootPlacement = true;
                        rc->aboveRootProvider = loadStateProvider(rp.at("above_root_placement").at("above_root_provider"));
                        rc->aboveRootPlacementChance = rp.at("above_root_placement").at("above_root_placement_chance").get<float>();
                    }
                    const json& mr = rp.at("mangrove_root_placement");
                    {   auto member = loadHolderSet(mr.at("can_grow_through"), g_tags);
                        rc->canGrowThrough = [member](const std::string& s) { return member(mc::block::blockName(s)); }; }
                    {   auto member = loadHolderSet(mr.at("muddy_roots_in"), g_tags);
                        rc->muddyRootsIn = [member](const std::string& s) { return member(mc::block::blockName(s)); }; }
                    rc->muddyRootsProvider = loadStateProvider(mr.at("muddy_roots_provider"));
                    rc->maxRootWidth = mr.at("max_root_width").get<int>();
                    rc->maxRootLength = mr.at("max_root_length").get<int>();
                    rc->randomSkewChance = mr.at("random_skew_chance").get<float>();
                    tc->rootPlacer = std::move(rc);
                }
                const json& tp = cc.at("trunk_placer");
                const std::string tpt = stripNs(tp.at("type").get<std::string>());
                if (tpt == "straight_trunk_placer") tc->trunkKind = mc::levelgen::feature::TreeConfig::Trunk::Straight;
                else if (tpt == "fancy_trunk_placer") tc->trunkKind = mc::levelgen::feature::TreeConfig::Trunk::Fancy;
                else if (tpt == "giant_trunk_placer") tc->trunkKind = mc::levelgen::feature::TreeConfig::Trunk::Giant;
                else if (tpt == "forking_trunk_placer") tc->trunkKind = mc::levelgen::feature::TreeConfig::Trunk::Forking;
                else if (tpt == "dark_oak_trunk_placer") tc->trunkKind = mc::levelgen::feature::TreeConfig::Trunk::DarkOak;
                else if (tpt == "mega_jungle_trunk_placer") tc->trunkKind = mc::levelgen::feature::TreeConfig::Trunk::MegaJungle;
                else if (tpt == "bending_trunk_placer") {
                    // BendingTrunkPlacer codec: min_height_for_leaves orElse 1 + bend_length.
                    tc->trunkKind = mc::levelgen::feature::TreeConfig::Trunk::Bending;
                    tc->minHeightForLeaves = tp.value("min_height_for_leaves", 1);
                    tc->bendLength = loadIntProvider(tp.at("bend_length"));
                } else if (tpt == "cherry_trunk_placer") {
                    // CherryTrunkPlacer codec: branch_count, branch_horizontal_length,
                    // branch_start_offset_from_top (UniformInt MAP form), branch_end_offset_from_top.
                    tc->trunkKind = mc::levelgen::feature::TreeConfig::Trunk::Cherry;
                    tc->branchCount = loadIntProvider(tp.at("branch_count"));
                    tc->branchHorizontalLength = loadIntProvider(tp.at("branch_horizontal_length"));
                    tc->branchStartOffsetMin = tp.at("branch_start_offset_from_top").at("min_inclusive").get<int>();
                    tc->branchStartOffsetMax = tp.at("branch_start_offset_from_top").at("max_inclusive").get<int>();
                    tc->branchEndOffsetFromTop = loadIntProvider(tp.at("branch_end_offset_from_top"));
                } else if (tpt == "upwards_branching_trunk_placer") {
                    // UpwardsBranchingTrunkPlacer codec.
                    tc->trunkKind = mc::levelgen::feature::TreeConfig::Trunk::UpwardsBranching;
                    tc->extraBranchSteps = loadIntProvider(tp.at("extra_branch_steps"));
                    tc->placeBranchPerLogProbability = tp.at("place_branch_per_log_probability").get<float>();
                    tc->extraBranchLength = loadIntProvider(tp.at("extra_branch_length"));
                    auto member = loadHolderSet(tp.at("can_grow_through"), g_tags);
                    tc->canGrowThrough = [member](const std::string& s) { return member(mc::block::blockName(s)); };
                }
                else throw std::runtime_error("unsupported trunk_placer: " + tpt);
                tc->baseHeight = tp.at("base_height").get<int>();
                tc->heightRandA = tp.at("height_rand_a").get<int>();
                tc->heightRandB = tp.at("height_rand_b").get<int>();
                const json& fp = cc.at("foliage_placer");
                const std::string fpt = stripNs(fp.at("type").get<std::string>());
                // blob/fancy/bush/jungle carry a constant int "height"; spruce/pine/
                // mega_pine/random_spread/cherry carry an IntProvider field sampled in
                // foliageHeight (the respective FoliagePlacer codecs).
                if (fpt == "blob_foliage_placer") {
                    tc->foliageKind = mc::levelgen::feature::TreeConfig::Foliage::Blob;
                    tc->foliageHeightParam = fp.at("height").get<int>();
                } else if (fpt == "fancy_foliage_placer") {
                    tc->foliageKind = mc::levelgen::feature::TreeConfig::Foliage::Fancy;
                    tc->foliageHeightParam = fp.at("height").get<int>();
                } else if (fpt == "spruce_foliage_placer") {
                    tc->foliageKind = mc::levelgen::feature::TreeConfig::Foliage::Spruce;
                    tc->foliageHeightProvider = loadIntProvider(fp.at("trunk_height"));
                } else if (fpt == "pine_foliage_placer") {
                    tc->foliageKind = mc::levelgen::feature::TreeConfig::Foliage::Pine;
                    tc->foliageHeightProvider = loadIntProvider(fp.at("height"));
                } else if (fpt == "mega_pine_foliage_placer") {
                    tc->foliageKind = mc::levelgen::feature::TreeConfig::Foliage::MegaPine;
                    tc->foliageHeightProvider = loadIntProvider(fp.at("crown_height"));
                } else if (fpt == "random_spread_foliage_placer") {
                    // RandomSpreadFoliagePlacer codec: foliage_height + leaf_placement_attempts.
                    tc->foliageKind = mc::levelgen::feature::TreeConfig::Foliage::RandomSpread;
                    tc->foliageHeightProvider = loadIntProvider(fp.at("foliage_height"));
                    tc->leafPlacementAttempts = fp.at("leaf_placement_attempts").get<int>();
                } else if (fpt == "acacia_foliage_placer") {
                    tc->foliageKind = mc::levelgen::feature::TreeConfig::Foliage::Acacia;
                } else if (fpt == "bush_foliage_placer") {
                    // BushFoliagePlacer extends Blob: same codec fields (blobParts).
                    tc->foliageKind = mc::levelgen::feature::TreeConfig::Foliage::Bush;
                    tc->foliageHeightParam = fp.at("height").get<int>();
                } else if (fpt == "cherry_foliage_placer") {
                    // CherryFoliagePlacer codec — with the vanilla ENCODE BUG: the
                    // "corner_hole_chance" field's forGetter reads
                    // wideBottomLayerHoleChance (CherryFoliagePlacer.java:22-23), so
                    // the SHIPPED JSON's corner_hole_chance (0.25) is a mis-encoded
                    // COPY of wide_bottom_layer_hole_chance. The runtime registry the
                    // server actually runs (TreeFeatures.java:216, via
                    // VanillaRegistries — proven by the CherryTreeProbe draw trace:
                    // corner draw 0.4847 SKIPS, so the threshold is 0.5, not 0.25)
                    // constructs CherryFoliagePlacer(..., wideBottom=0.25F,
                    // corner=0.5F, ...). Use the runtime corner value; fail closed if
                    // a non-vanilla JSON ever disagrees with the mis-encode pattern.
                    tc->foliageKind = mc::levelgen::feature::TreeConfig::Foliage::Cherry;
                    tc->foliageHeightProvider = loadIntProvider(fp.at("height"));
                    tc->wideBottomLayerHoleChance = fp.at("wide_bottom_layer_hole_chance").get<float>();
                    const float jsonCorner = fp.at("corner_hole_chance").get<float>();
                    if (jsonCorner != tc->wideBottomLayerHoleChance)
                        throw std::runtime_error("cherry corner_hole_chance does not match the vanilla mis-encode (codec bug model)");
                    tc->cornerHoleChance = 0.5f;   // TreeFeatures.java:216
                    tc->hangingLeavesChance = fp.at("hanging_leaves_chance").get<float>();
                    tc->hangingLeavesExtensionChance = fp.at("hanging_leaves_extension_chance").get<float>();
                } else if (fpt == "dark_oak_foliage_placer") {
                    tc->foliageKind = mc::levelgen::feature::TreeConfig::Foliage::DarkOak;
                } else if (fpt == "jungle_foliage_placer") {
                    // MegaJungleFoliagePlacer registers as "jungle_foliage_placer"
                    // (FoliagePlacerType.java).
                    tc->foliageKind = mc::levelgen::feature::TreeConfig::Foliage::MegaJungle;
                    tc->foliageHeightParam = fp.at("height").get<int>();
                } else {
                    throw std::runtime_error("unsupported foliage_placer: " + fpt);
                }
                tc->foliageRadius = loadIntProvider(fp.at("radius"));
                tc->foliageOffset = loadIntProvider(fp.at("offset"));
                const json& ms = cc.at("minimum_size");
                const std::string mst = stripNs(ms.at("type").get<std::string>());
                if (mst == "two_layers_feature_size") {
                    tc->sizeKind = mc::levelgen::feature::TreeConfig::SizeKind::TwoLayers;
                    tc->sizeLimit = ms.value("limit", 1);          // TwoLayersFeatureSize codec orElse
                    tc->lowerSize = ms.value("lower_size", 0);
                    tc->upperSize = ms.value("upper_size", 1);
                } else if (mst == "three_layers_feature_size") {
                    // ThreeLayersFeatureSize codec orElse defaults (ThreeLayersFeatureSize.java:10-19).
                    tc->sizeKind = mc::levelgen::feature::TreeConfig::SizeKind::ThreeLayers;
                    tc->sizeLimit = ms.value("limit", 1);
                    tc->upperLimit = ms.value("upper_limit", 1);
                    tc->lowerSize = ms.value("lower_size", 0);
                    tc->middleSize = ms.value("middle_size", 1);
                    tc->upperSize = ms.value("upper_size", 1);
                } else {
                    throw std::runtime_error("unsupported minimum_size: " + mst);
                }
                if (ms.contains("min_clipped_height")) tc->minClippedHeight = ms.at("min_clipped_height").get<int>();
                tc->ignoreVines = cc.value("ignore_vines", false);
                for (const auto& dj : cc.at("decorators")) tc->decorators.push_back(loadTreeDecorator(dj));
                placer = mc::levelgen::feature::makeTreePlacer(std::move(tc), treeHooks);
            } else if (type == "fallen_tree") {
                const json& cc = cfgJson.at("config");
                auto fc = std::make_shared<mc::levelgen::feature::FallenTreeConfig>();
                fc->trunkProvider = loadStateProvider(cc.at("trunk_provider"));
                fc->logLength = loadIntProvider(cc.at("log_length"));
                if (cc.contains("stump_decorators"))
                    for (const auto& dj : cc.at("stump_decorators")) fc->stumpDecorators.push_back(loadFallenTreeDecorator(dj));
                if (cc.contains("log_decorators"))
                    for (const auto& dj : cc.at("log_decorators")) fc->logDecorators.push_back(loadFallenTreeDecorator(dj));
                placer = mc::levelgen::feature::makeFallenTreePlacer(std::move(fc), treeHooks, isFaceSturdyUpState);
            } else if (type == "monster_room") {
                placer = mc::levelgen::feature::makeMonsterRoomPlacer(monsterHooks);
            } else if (type == "freeze_top_layer") {
                placer = mc::levelgen::feature::makeSnowAndFreezePlacer(snowFreezeHooks);
            } else if (type == "lake") {
                const json& cc = cfgJson.at("config");
                placer = mc::levelgen::feature::makeLakePlacer(
                    loadStateProvider(cc.at("fluid")), loadStateProvider(cc.at("barrier")), lakeHooks);
            } else if (type == "block_column") {
                // BlockColumnConfiguration: layers [{height, provider}], direction,
                // allowed_placement, prioritize_tip (BlockColumnConfiguration codec).
                const json& cc = cfgJson.at("config");
                std::vector<mc::levelgen::feature::BlockColumnLayerFn> layers;
                for (const auto& lj : cc.at("layers"))
                    layers.push_back({ loadIntProvider(lj.at("height")), loadStateProvider(lj.at("provider")) });
                const int dirIdx = parseDirection(cc.at("direction").get<std::string>());
                const BlockPos dir{ mc::levelgen::feature::TREE_DIR_DX[dirIdx],
                                    mc::levelgen::feature::TREE_DIR_DY[dirIdx],
                                    mc::levelgen::feature::TREE_DIR_DZ[dirIdx] };
                placer = mc::levelgen::feature::makeBlockColumnPlacer(
                    std::move(layers), dir, cc.at("prioritize_tip").get<bool>(),
                    loadBlockPredicate(cc.at("allowed_placement")));
            } else if (type == "spike") {
                // SpikeConfiguration: can_place_on, can_replace, state.
                const json& cc = cfgJson.at("config");
                placer = mc::levelgen::feature::makeSpikePlacer(
                    loadBlockPredicate(cc.at("can_place_on")),
                    loadBlockPredicate(cc.at("can_replace")),
                    stateName(cc.at("state")),
                    [](const std::string& s) { return mc::block::isAirBlock(s); });
            } else if (type == "block_blob") {
                // BlockBlobConfiguration: state, can_place_on.
                const json& cc = cfgJson.at("config");
                placer = mc::levelgen::feature::makeBlockBlobPlacer(
                    loadBlockPredicate(cc.at("can_place_on")), stateName(cc.at("state")));
            } else if (type == "geode") {
                const json& cc = cfgJson.at("config");
                const json& bj = cc.at("blocks");
                const json& lj = cc.at("layers");
                const json& kj = cc.at("crack");
                auto gc = std::make_shared<mc::levelgen::feature::GeodeConfig>();
                gc->fillingProvider = loadStateProvider(bj.at("filling_provider"));
                gc->innerLayerProvider = loadStateProvider(bj.at("inner_layer_provider"));
                gc->alternateInnerLayerProvider = loadStateProvider(bj.at("alternate_inner_layer_provider"));
                gc->middleLayerProvider = loadStateProvider(bj.at("middle_layer_provider"));
                gc->outerLayerProvider = loadStateProvider(bj.at("outer_layer_provider"));
                for (const auto& s : bj.at("inner_placements")) gc->innerPlacements.push_back(stateName(s));
                if (gc->innerPlacements.empty()) throw std::runtime_error("geode: empty inner_placements");
                { auto member = loadHolderSet(bj.at("cannot_replace"), g_tags);
                  gc->cannotReplace = [member](const std::string& s) { return member(mc::block::blockName(s)); }; }
                { auto member = loadHolderSet(bj.at("invalid_blocks"), g_tags);
                  gc->invalidBlocks = [member](const std::string& s) { return member(mc::block::blockName(s)); }; }
                gc->filling = lj.value("filling", 1.7);
                gc->innerLayer = lj.value("inner_layer", 2.2);
                gc->middleLayer = lj.value("middle_layer", 3.2);
                gc->outerLayer = lj.value("outer_layer", 4.2);
                gc->baseCrackSize = kj.value("base_crack_size", 2.0);
                gc->generateCrackChance = kj.value("generate_crack_chance", 1.0);
                gc->crackPointOffset = kj.value("crack_point_offset", 2);
                // Codec defaults: GeodeConfiguration.java (0.35 / 0.0 / true / U(4,5) /
                // U(3,4) / U(1,2) / -16 / 16 / 0.05), GeodeCrackSettings.java (1.0 / 2.0 / 2).
                gc->noiseMultiplier = cc.value("noise_multiplier", 0.05);
                gc->usePotentialPlacementsChance = cc.value("use_potential_placements_chance", 0.35);
                gc->useAlternateLayer0Chance = cc.value("use_alternate_layer0_chance", 0.0);
                gc->placementsRequireLayer0Alternate = cc.value("placements_require_layer0_alternate", true);
                gc->outerWallDistance = cc.contains("outer_wall_distance")
                    ? loadIntProvider(cc.at("outer_wall_distance")) : UniformInt::of(4, 5);
                // crackSizeAdjustment divides by outerWallDistance.maxInclusive() (GeodeFeature.java:48).
                gc->outerWallDistanceMax = cc.contains("outer_wall_distance")
                    ? cc.at("outer_wall_distance").at("max_inclusive").get<int>() : 5;
                gc->distributionPoints = cc.contains("distribution_points")
                    ? loadIntProvider(cc.at("distribution_points")) : UniformInt::of(3, 4);
                gc->pointOffset = cc.contains("point_offset")
                    ? loadIntProvider(cc.at("point_offset")) : UniformInt::of(1, 2);
                gc->minGenOffset = cc.value("min_gen_offset", -16);
                gc->maxGenOffset = cc.value("max_gen_offset", 16);
                gc->invalidBlocksThreshold = cc.at("invalid_blocks_threshold").get<int>();
                placer = mc::levelgen::feature::makeGeodePlacer(std::move(gc), geodeHooks);
            } else if (type == "desert_well") {
                placer = mc::levelgen::feature::makeDesertWellPlacer();
            } else if (type == "random_boolean_selector") {
                // RandomBooleanSelectorFeature (RandomBooleanSelectorFeature.java):
                // one nextBoolean picks feature_true/feature_false.
                const json& cc = cfgJson.at("config");
                auto featureTrue = resolvePlacedRef(cc.at("feature_true"), "?nested");
                auto featureFalse = resolvePlacedRef(cc.at("feature_false"), "?nested");
                // (genMinY/genDepth are namespace-scope constexpr now: no capture needed)
                placer = [featureTrue, featureFalse](
                             WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
                    const bool result = random.nextBoolean();
                    return (result ? featureTrue : featureFalse)->place(level, random, origin, genMinY, genDepth);
                };
            } else if (type == "vegetation_patch" || type == "waterlogged_vegetation_patch") {
                const json& cc = cfgJson.at("config");
                auto vc = std::make_shared<mc::levelgen::feature::VegetationPatchConfig>();
                {   // TagKey.hashedCodec: the field is a "#tag" string.
                    const std::string tagRef = cc.at("replaceable").get<std::string>();
                    if (tagRef.empty() || tagRef[0] != '#') throw std::runtime_error("vegetation_patch replaceable: expected #tag");
                    const std::string tag = tagRef.substr(1);
                    vc->replaceable = [tag](const std::string& st) { return g_tags->isInTag(mc::block::blockName(st), tag); };
                }
                vc->groundState = loadStateProvider(cc.at("ground_state"));
                vc->vegetationFeature = resolvePlacedRef(cc.at("vegetation_feature"), "?nested");
                const std::string surface = stripNs(cc.at("surface").get<std::string>());
                if (surface == "floor") vc->surfaceDirection = 0;        // CaveSurface.FLOOR -> DOWN
                else if (surface == "ceiling") vc->surfaceDirection = 1; // CaveSurface.CEILING -> UP
                else throw std::runtime_error("unsupported cave surface: " + surface);
                vc->depth = loadIntProvider(cc.at("depth"));
                vc->extraBottomBlockChance = cc.at("extra_bottom_block_chance").get<float>();
                vc->verticalRange = cc.at("vertical_range").get<int>();
                vc->vegetationChance = cc.at("vegetation_chance").get<float>();
                vc->xzRadius = loadIntProvider(cc.at("xz_radius"));
                vc->extraEdgeColumnChance = cc.at("extra_edge_column_chance").get<float>();
                vc->waterlogged = type == "waterlogged_vegetation_patch";
                vc->genMinY = genMinY;
                vc->genDepth = genDepth;
                placer = mc::levelgen::feature::makeVegetationPatchPlacer(std::move(vc), caveHooks);
            } else if (type == "root_system") {
                const json& cc = cfgJson.at("config");
                auto rc = std::make_shared<mc::levelgen::feature::RootSystemConfig>();
                rc->treeFeature = resolvePlacedRef(cc.at("feature"), "?nested");
                rc->requiredVerticalSpaceForTree = cc.at("required_vertical_space_for_tree").get<int>();
                rc->allowedVerticalWaterForTree = cc.at("allowed_vertical_water_for_tree").get<int>();
                {   auto pred = loadBlockPredicate(cc.at("allowed_tree_position"));
                    rc->allowedTreePosition = [pred](WorldGenLevel& level, BlockPos p) { return pred(level, p); }; }
                rc->rootRadius = cc.at("root_radius").get<int>();
                {   const std::string tagRef = cc.at("root_replaceable").get<std::string>();
                    if (tagRef.empty() || tagRef[0] != '#') throw std::runtime_error("root_system root_replaceable: expected #tag");
                    const std::string tag = tagRef.substr(1);
                    rc->rootReplaceable = [tag](const std::string& st) { return g_tags->isInTag(mc::block::blockName(st), tag); }; }
                rc->rootStateProvider = loadStateProvider(cc.at("root_state_provider"));
                rc->rootPlacementAttempts = cc.at("root_placement_attempts").get<int>();
                rc->rootColumnMaxHeight = cc.at("root_column_max_height").get<int>();
                rc->hangingRootRadius = cc.at("hanging_root_radius").get<int>();
                rc->hangingRootsVerticalSpan = cc.at("hanging_roots_vertical_span").get<int>();
                rc->hangingRootStateProvider = loadStateProvider(cc.at("hanging_root_state_provider"));
                rc->hangingRootPlacementAttempts = cc.at("hanging_root_placement_attempts").get<int>();
                rc->genMinY = genMinY;
                rc->genDepth = genDepth;
                placer = mc::levelgen::feature::makeRootSystemPlacer(std::move(rc), caveHooks);
            } else if (type == "vines") {
                // VineBlock.isAcceptableNeighbour == MultifaceBlock.canAttachTo
                // (VineBlock.java:118-120) — direction-independent full-face test in
                // the multiface port.
                placer = mc::levelgen::feature::makeVinesPlacer(
                    [](WorldGenLevel& level, BlockPos neighbour, int direction) {
                        static constexpr int OPP[6] = { 1, 0, 3, 2, 5, 4 };
                        return mc::levelgen::feature::multiface_detail::canAttachTo(level, neighbour, OPP[direction]);
                    },
                    [](BlockPos at, int faceDir) { g_level->putVineFaces(at, static_cast<std::uint8_t>(1u << faceDir)); });
            } else if (type == "pointed_dripstone") {
                const json& cc = cfgJson.at("config");
                mc::levelgen::feature::PointedDripstoneConfig pc;
                // PointedDripstoneConfiguration codec defaults all 4 chances (orElse).
                pc.chanceOfTallerDripstone = cc.value("chance_of_taller_dripstone", 0.2f);
                pc.chanceOfDirectionalSpread = cc.value("chance_of_directional_spread", 0.7f);
                pc.chanceOfSpreadRadius2 = cc.value("chance_of_spread_radius2", 0.5f);
                pc.chanceOfSpreadRadius3 = cc.value("chance_of_spread_radius3", 0.5f);
                placer = mc::levelgen::feature::makePointedDripstonePlacer(pc, dripstoneHooks);
            } else if (type == "large_dripstone") {
                const json& cc = cfgJson.at("config");
                auto lc = std::make_shared<mc::levelgen::feature::LargeDripstoneConfig>();
                lc->floorToCeilingSearchRange = cc.value("floor_to_ceiling_search_range", 30);
                lc->columnRadiusMin = cc.at("column_radius").at("min_inclusive").get<int>();
                lc->columnRadiusMax = cc.at("column_radius").at("max_inclusive").get<int>();
                lc->heightScale = loadFloatProvider(cc.at("height_scale"));
                lc->maxColumnRadiusToCaveHeightRatio = cc.at("max_column_radius_to_cave_height_ratio").get<float>();
                lc->stalactiteBluntness = loadFloatProvider(cc.at("stalactite_bluntness"));
                lc->stalagmiteBluntness = loadFloatProvider(cc.at("stalagmite_bluntness"));
                lc->windSpeed = loadFloatProvider(cc.at("wind_speed"));
                lc->minRadiusForWind = cc.at("min_radius_for_wind").get<int>();
                lc->minBluntnessForWind = cc.at("min_bluntness_for_wind").get<float>();
                placer = mc::levelgen::feature::makeLargeDripstonePlacer(std::move(lc), dripstoneHooks);
            } else if (type == "dripstone_cluster") {
                const json& cc = cfgJson.at("config");
                auto dc = std::make_shared<mc::levelgen::feature::DripstoneClusterConfig>();
                dc->floorToCeilingSearchRange = cc.at("floor_to_ceiling_search_range").get<int>();
                dc->height = loadIntProvider(cc.at("height"));
                dc->radius = loadIntProvider(cc.at("radius"));
                dc->maxStalagmiteStalactiteHeightDiff = cc.at("max_stalagmite_stalactite_height_diff").get<int>();
                dc->heightDeviation = cc.at("height_deviation").get<int>();
                dc->dripstoneBlockLayerThickness = loadIntProvider(cc.at("dripstone_block_layer_thickness"));
                dc->density = loadFloatProvider(cc.at("density"));
                dc->wetness = loadFloatProvider(cc.at("wetness"));
                dc->chanceOfDripstoneColumnAtMaxDistanceFromCenter = cc.at("chance_of_dripstone_column_at_max_distance_from_center").get<float>();
                dc->maxDistanceFromEdgeAffectingChanceOfDripstoneColumn = cc.at("max_distance_from_edge_affecting_chance_of_dripstone_column").get<int>();
                dc->maxDistanceFromCenterAffectingHeightBias = cc.at("max_distance_from_center_affecting_height_bias").get<int>();
                placer = mc::levelgen::feature::makeDripstoneClusterPlacer(std::move(dc), dripstoneHooks);
            } else if (type == "huge_brown_mushroom" || type == "huge_red_mushroom") {
                // HugeMushroomFeatureConfiguration: cap_provider, stem_provider,
                // foliage_radius (orElse 2), can_place_on (BlockPredicate).
                const json& cc = cfgJson.at("config");
                auto mc_ = std::make_shared<mc::levelgen::feature::HugeMushroomConfig>();
                mc_->capProvider = loadStateProvider(cc.at("cap_provider"));
                mc_->stemProvider = loadStateProvider(cc.at("stem_provider"));
                {   auto pred = loadBlockPredicate(cc.at("can_place_on"));
                    mc_->canPlaceOn = [pred](WorldGenLevel& level, BlockPos p) { return pred(level, p); }; }
                mc_->foliageRadius = cc.value("foliage_radius", 2);
                mc_->red = type == "huge_red_mushroom";
                placer = mc::levelgen::feature::makeHugeMushroomPlacer(std::move(mc_), hugeMushroomHooks);
            } else if (type == "bamboo") {
                // ProbabilityFeatureConfiguration (BambooFeature).
                mc::levelgen::feature::BambooHooks bh;
                bh.beneathBambooPodzolReplaceable = [](const std::string& s) {
                    return g_tags->isInTag(mc::block::blockName(s), "minecraft:beneath_bamboo_podzol_replaceable");
                };
                placer = mc::levelgen::feature::makeBambooPlacer(
                    cfgJson.at("config").at("probability").get<double>(), std::move(bh));
            } else if (type == "iceberg") {
                // BlockStateConfiguration: state (packed_ice / blue_ice).
                placer = mc::levelgen::feature::makeIcebergPlacer(
                    stateName(cfgJson.at("config").at("state")), 63,
                    [](const std::string& s) { return mc::block::isAirBlock(s); });
            } else if (type == "blue_ice") {
                placer = mc::levelgen::feature::makeBlueIcePlacer(
                    63, [](const std::string& s) { return mc::block::isAirBlock(s); });
            } else if (type == "sea_pickle") {
                // CountConfiguration.
                placer = mc::levelgen::feature::makeSeaPicklePlacer(
                    loadIntProvider(cfgJson.at("config").at("count")));
            } else if (type == "coral_tree" || type == "coral_claw" || type == "coral_mushroom") {
                placer = mc::levelgen::feature::makeCoralPlacer(
                    type == "coral_tree" ? 0 : type == "coral_claw" ? 1 : 2, coralHooks);
            } else if (type == "sculk_patch") {
                // SculkPatchConfiguration.
                const json& cc = cfgJson.at("config");
                auto sc = std::make_shared<mc::levelgen::feature::SculkPatchConfig>();
                sc->chargeCount = cc.at("charge_count").get<int>();
                sc->amountPerCharge = cc.at("amount_per_charge").get<int>();
                sc->spreadAttempts = cc.at("spread_attempts").get<int>();
                sc->growthRounds = cc.at("growth_rounds").get<int>();
                sc->spreadRounds = cc.at("spread_rounds").get<int>();
                sc->extraRareGrowths = loadIntProvider(cc.at("extra_rare_growths"));
                sc->catalystChance = cc.at("catalyst_chance").get<float>();
                placer = mc::levelgen::feature::makeSculkPatchPlacer(
                    std::move(sc), sculkHooks,
                    [](const std::string& s) {
                        // SculkVeinBlock.hasSubstrateAccess uses the LEVEL tag
                        // #sculk_replaceable (SculkVeinBlock.java:131-145).
                        return g_tags->isInTag(mc::block::blockName(s), "minecraft:sculk_replaceable");
                    });
            } else if (type == "fossil") {
                // FossilFeatureConfiguration: structure id lists + processor-list
                // refs + max_empty_corners_allowed.
                const json& cc = cfgJson.at("config");
                auto fc = std::make_shared<mc::levelgen::feature::FossilConfig>();
                for (const auto& s : cc.at("fossil_structures")) fc->fossilStructures.push_back(s.get<std::string>());
                for (const auto& s : cc.at("overlay_structures")) fc->overlayStructures.push_back(s.get<std::string>());
                if (fc->fossilStructures.empty() || fc->fossilStructures.size() != fc->overlayStructures.size())
                    throw std::runtime_error("fossil structure lists invalid");
                auto loadProcessorList = [&](const json& ref) {
                    std::vector<mc::levelgen::feature::fossil_detail::Processor> out;
                    json plJson;
                    {
                        std::ifstream f(dataDir + "/worldgen/processor_list/" + stripNs(ref.get<std::string>()) + ".json");
                        if (!f) throw std::runtime_error("no processor_list " + ref.get<std::string>());
                        f >> plJson;
                    }
                    for (const auto& pj : plJson.at("processors")) {
                        const std::string pt = stripNs(pj.at("processor_type").get<std::string>());
                        mc::levelgen::feature::fossil_detail::Processor proc;
                        if (pt == "block_rot") {
                            // BlockRotProcessor codec: optional rottable_blocks +
                            // integrity; the fossil lists carry no rottable filter.
                            if (pj.contains("rottable_blocks"))
                                throw std::runtime_error("block_rot rottable_blocks not ported");
                            proc.kind = 0;
                            proc.integrity = pj.at("integrity").get<float>();
                        } else if (pt == "protected_blocks") {
                            // ProtectedBlockProcessor: hashed tag (#features_cannot_replace).
                            if (stripNs(pj.at("value").get<std::string>()) != "#minecraft:features_cannot_replace"
                                && pj.at("value").get<std::string>() != "#minecraft:features_cannot_replace")
                                throw std::runtime_error("protected_blocks tag not ported: " + pj.at("value").get<std::string>());
                            proc.kind = 1;
                        } else if (pt == "rule") {
                            // RuleProcessor with block_match input + always_true
                            // location rules only (the fossil_diamonds list); these
                            // rules consume NO draws (the rule random is positional
                            // and untouched by block_match/always_true).
                            proc.kind = 2;
                            for (const auto& rj : pj.at("rules")) {
                                if (stripNs(rj.at("input_predicate").at("predicate_type").get<std::string>()) != "block_match")
                                    throw std::runtime_error("rule input_predicate not ported");
                                if (stripNs(rj.at("location_predicate").at("predicate_type").get<std::string>()) != "always_true")
                                    throw std::runtime_error("rule location_predicate not ported");
                                if (rj.contains("position_predicate_type") && stripNs(rj.at("position_predicate_type").get<std::string>()) != "always_true")
                                    throw std::runtime_error("rule position predicate not ported");
                                mc::levelgen::feature::fossil_detail::Processor::Rule rule;
                                rule.input = "minecraft:" + stripNs(rj.at("input_predicate").at("block").get<std::string>());
                                rule.output = stateName(rj.at("output_state"));
                                proc.rules.push_back(std::move(rule));
                            }
                        } else {
                            throw std::runtime_error("processor not ported: " + pt);
                        }
                        out.push_back(std::move(proc));
                    }
                    return out;
                };
                fc->fossilProcessors = loadProcessorList(cc.at("fossil_processors"));
                fc->overlayProcessors = loadProcessorList(cc.at("overlay_processors"));
                fc->maxEmptyCornersAllowed = cc.at("max_empty_corners_allowed").get<int>();
                auto fh = std::make_shared<mc::levelgen::feature::FossilHooks>();
                fh->isAir = [](const std::string& s) { return mc::block::isAirBlock(s); };
                fh->featuresCannotReplace = [](const std::string& s) {
                    return g_tags->isInTag(mc::block::blockName(s), "minecraft:features_cannot_replace");
                };
                fh->updateShapeFace = treeHooks->updateShapeFace;
                fh->levelMinY = mc::CHUNK_MIN_Y;
                fh->levelMaxY = mc::CHUNK_MAX_Y - 1;
                fh->structureDir = dataDir + "/structure";
                placer = mc::levelgen::feature::makeFossilPlacer(std::move(fc), std::move(fh));
            } else {
                // Not yet ported: throw — resolveFeature records the hard no-op.
                throw UnportedFeatureType{ type };
            }
            return placer;
    };

    resolveFeature = [&](const std::string& featureKey) -> std::shared_ptr<PlacedFeature> {
        auto it = cache.find(featureKey);
        if (it != cache.end()) return it->second;
        cache[featureKey] = nullptr;   // pre-seed: cycles/nested failures resolve to unported
        std::shared_ptr<PlacedFeature> result;
        try {
            const std::string pname = stripNs(featureKey);
            json placedJson; { std::ifstream f(dataDir + "/worldgen/placed_feature/" + pname + ".json"); if (!f) throw std::runtime_error("no placed_feature"); f >> placedJson; }
            result = loadPlacedFromJson(placedJson, featureKey);
        } catch (const UnportedFeatureType& u) {
            // Hard no-op: not yet ported. Counted per run at the call site; never "pass".
            g_unportedType[featureKey] = u.type;
            return nullptr;
        } catch (const std::exception& e) {
            std::cerr << "FAILED-TO-PORT " << featureKey << ": " << e.what() << "\n";
            return nullptr;
        }
        cache[featureKey] = result;
        return result;
    };
    }   // DecorationResolver ctor
};

// ---- PASS A unit: ONE chunk's quart-biome palette, in vanilla ChunkAccess
// .fillBiomesFromNoise order (sections ascending; LevelChunkSection
// .fillBiomesFromNoise x,y,z inner). ORDER MATTERS: Climate.ParameterList
// .findValue routes through the RTree whose lastResult seed makes distance-TIE
// results depend on the previous query (Climate.java:250,390-391) — see the
// PASS-A comment in main. The harness fills a fixed 7x7 in dx OUTER / dz INNER
// order; the ENGINE fills chunks on demand in neighbour-availability order, so
// tie quarts at chunk borders may differ from the batch fill (documented delta).
void fillBiomeStoreChunk(const NoiseBasedChunkGenerator& gen,
                         std::unordered_map<std::int64_t, std::vector<std::string>>& biomeStore,
                         int ncx, int ncz) {
    const int minY = mc::CHUNK_MIN_Y, maxY = mc::CHUNK_MAX_Y;
    const int qyMin = minY >> 2, qyMax = (maxY >> 2) - 1;
    std::vector<std::string>& cell = biomeStore[packChunk(ncx, ncz)];
    cell.resize(static_cast<std::size_t>(qyMax - qyMin + 1) * 16);
    for (int sy = minY >> 4; sy <= (maxY - 1) >> 4; ++sy) {           // sections ascending
        const int quartMinY = sy << 2;                                 // QuartPos.fromSection
        for (int x = 0; x < 4; ++x) for (int y = 0; y < 4; ++y) for (int z = 0; z < 4; ++z) {
            const int qx = ncx * 4 + x, qy = quartMinY + y, qz = ncz * 4 + z;
            cell[static_cast<std::size_t>(qy - qyMin) * 16 + x * 4 + z] = gen.getNoiseBiome(qx, qy, qz);
        }
    }
}
// ChunkAccess.getNoiseBiome (ChunkAccess.java:425-437): quartY CLAMPED to the
// chunk's section range, then the stored palette; outside the filled grid fall
// back to the raw sampler (a live findValue query), as the Java proxy does.
std::string storeNoiseBiomeLookup(const NoiseBasedChunkGenerator& gen,
                                  const std::unordered_map<std::int64_t, std::vector<std::string>>& biomeStore,
                                  int qx, int qy, int qz) {
    const int qyMin = mc::CHUNK_MIN_Y >> 2, qyMax = (mc::CHUNK_MAX_Y >> 2) - 1;
    const int scx = qx >> 2, scz = qz >> 2;   // QuartPos.toSection
    auto it = biomeStore.find(packChunk(scx, scz));
    if (it == biomeStore.end()) return gen.getNoiseBiome(qx, qy, qz);
    const int cqy = std::max(qyMin, std::min(qyMax, qy));
    return it->second[static_cast<std::size_t>(cqy - qyMin) * 16 + (qx & 3) * 4 + (qz & 3)];
}

// One chunk's FEATURES turn (the body of the harness's inner-3x3 loop, verbatim):
// setDecorating, fresh per-region random, re-prime the four non-WG heightmaps,
// possibleBiomes = distinct section biomes over the 3x3 ∩ overworld set,
// setDecorationSeed/setFeatureSeed, placeWithBiomeCheck of every resolved feature
// in FeatureSorter order (unported = counted hard no-op). PRECONDITIONS exactly as
// in the harness: g_level/g_biomeCtx/g_curLevelSeed bound, the chunk and its 8
// neighbours generated (post-carvers) and WG-frozen, biome store filled for the
// 3x3 around (nx,nz).
void decorateOneChunk(MultiChunkLevel& level, int nx, int nz, long long seed,
                      DecorationResolver& resolver,
                      const std::function<std::string(int, int, int)>& storeNoiseBiome,
                      PositionalRandomFactory& regionRandomFactory,
                      bool primeHeightmaps = true) {
    const int minY = mc::CHUNK_MIN_Y, maxY = mc::CHUNK_MAX_Y;
    level.setDecorating(nx, nz);
    // Fresh per-region random for this chunk's FEATURES turn at the chunk's
    // getWorldPosition() (WorldGenRegion.java:86; GT: FullChunkDecorateParity
    // .java decorate() REGION_RANDOM reset).
    g_regionRandom = regionRandomFactory.at(nx * 16, 0, nz * 16);
    if (const char* wh = std::getenv("MCPP_WATCH_HEIGHT")) {
        int wx, wz;
        if (std::sscanf(wh, "%d,%d", &wx, &wz) == 2)
            std::cerr << "WATCH-HEIGHT turn=" << nx << "," << nz << " OCEAN_FLOOR(" << wx << "," << wz
                      << ") = " << level.getHeight(Heightmap::Types::OCEAN_FLOOR, wx, wz) << "\n";
    }
    // ChunkStatusTasks.generateFeatures / the Java GT decorate() (:533-536):
    // prime (overwrite) THIS chunk's four non-WG heightmaps at its turn start
    // — the snapshot includes spill already written by earlier turns.
    if (primeHeightmaps) level.primeNonWgHeightmaps(nx, nz);

    // possibleBiomes for chunk N = distinct section biomes over the 3x3 around N
    // (ChunkPos.rangeClosed(N,1)) intersected with the overworld possible set.
    std::set<std::string> pbSet;
    for (int ddz = -1; ddz <= 1; ++ddz) for (int ddx = -1; ddx <= 1; ++ddx) {
        const int qx0 = (nx + ddx) * 4, qz0 = (nz + ddz) * 4;
        for (int qy = (minY >> 2); qy < (maxY >> 2); ++qy)
            for (int qx = qx0; qx < qx0 + 4; ++qx)
                for (int qz = qz0; qz < qz0 + 4; ++qz) {
                    const std::string b = storeNoiseBiome(qx, qy, qz);
                    if (resolver.sourcesSet.count(b)) pbSet.insert(b);
                }
    }
    const std::vector<std::string> possibleBiomes(pbSet.begin(), pbSet.end());

    WorldgenRandom random(std::make_shared<XoroshiroRandomSource>(static_cast<std::uint64_t>(seed)));
    const std::int64_t deco = random.setDecorationSeed(static_cast<std::int64_t>(seed), nx * 16, nz * 16);
    const auto& stepData = resolver.stepData;
    const int genSteps = std::max(static_cast<int>(stepData.size()), (int)GenerationStep::COUNT);
    for (int step = 0; step < genSteps && step < static_cast<int>(stepData.size()); ++step) {
        const std::vector<int> indices = FeatureSorter::selectFeatureIndicesForStep(possibleBiomes, resolver.biomeFeatures, stepData[step], step);
        for (int index : indices) {
            const std::string& featureKey = stepData[step].features[static_cast<std::size_t>(index)];
            const std::string normKey = "minecraft:" + stripNs(featureKey);
            std::shared_ptr<PlacedFeature> placed = resolver.resolveFeature(normKey);
            if (!placed) {   // unported type (hard no-op; reseeded next feature)
                auto ut = g_unportedType.find(normKey);
                ++g_unportedSkips[ut != g_unportedType.end() ? ut->second : "load-failed"];
                continue;
            }
            random.setFeatureSeed(deco, index, step);
            g_curFeatureKey = normKey; g_curTurnCx = nx; g_curTurnCz = nz;
            const bool any = placed->place(level, random, BlockPos{ nx * 16, 0, nz * 16 }, minY, maxY - minY);
            // Debug: per-feature-run result, diffable vs the Java GT's MCPP_DBG_OK.
            if (std::getenv("MCPP_DBG_OK") != nullptr)
                std::cerr << "OK\t" << nx << "," << nz << "\t" << step << "\t" << index
                          << "\t" << normKey << "\t" << (any ? 1 : 0) << "\n";
            if (step == (int)GenerationStep::VEGETAL_DECORATION) { ++g_vegRuns; if (any) ++g_vegPlacedOk; }
            else { ++g_oreRuns; if (any) ++g_orePlacedOk; }
        }
    }
}

} // namespace

#ifndef MCPP_DECORATE_NO_MAIN
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
    // All loading machinery (tags, biomes, climate, step data, hooks, the cached
    // fail-closed placed/configured-feature loaders) — shared 1:1 with the engine.
    DecorationResolver resolver(dataDir);
    BiomeFeatures& biomeFeatures = resolver.biomeFeatures;
    std::set<std::string>& oreFamily = resolver.oreFamily;
    const std::set<std::string>& vegetalFamily = resolver.vegetalFamily;

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
        g_curLevelSeed = seed;   // level.getSeed() for cached placers (geode noise)

        NoiseBasedChunkGenerator gen(static_cast<std::uint64_t>(seed));

        // ---- PASS A: the vanilla BIOMES step (mirrors the certified Java GT,
        // FullChunkDecorateParity.java:370-406). Every chunk's quart-biome palette is
        // filled BEFORE any terrain/surface work, chunk loop dx OUTER / dz INNER over
        // biomeRadius = terrainRadius+1 = 3, per chunk in ChunkAccess.fillBiomesFromNoise
        // order (sections ascending; LevelChunkSection.fillBiomesFromNoise x,y,z inner).
        // ORDER MATTERS: Climate.ParameterList.findValue routes through the RTree whose
        // lastResult seed makes distance-TIE results depend on the previous query
        // (Climate.java:250,390-391). The server surface at biome boundaries (e.g.
        // beach/stony_shore in chunk (11,9)) reads these STORED biomes — sampling in any
        // other order flips tie quarts and the surface material with it.
        // (Values are order-independent otherwise: NoiseChunk.cachedClimateSampler ==
        // raw sampler on these chunks, verified raw-vs-cached diffs=0 over 7 chunks.)
        const int qyMin = minY >> 2, qyMax = (maxY >> 2) - 1;
        std::unordered_map<std::int64_t, std::vector<std::string>> biomeStore;   // (cx,cz) -> [ (qy-qyMin)*16 + (qx&3)*4 + (qz&3) ]
        for (int dx = -3; dx <= 3; ++dx) for (int dz = -3; dz <= 3; ++dz)
            fillBiomeStoreChunk(gen, biomeStore, Cx + dx, Cz + dz);
        // Debug: dump the PASS-A store (diffable against the Java GT fill).
        if (std::getenv("MCPP_DUMP_BIOME_STORE") != nullptr) {
            for (int dx = -3; dx <= 3; ++dx) for (int dz = -3; dz <= 3; ++dz) {
                const std::vector<std::string>& cell = biomeStore[packChunk(Cx + dx, Cz + dz)];
                for (int qx = (Cx + dx) * 4; qx < (Cx + dx) * 4 + 4; ++qx)
                    for (int qz = (Cz + dz) * 4; qz < (Cz + dz) * 4 + 4; ++qz)
                        for (int qy = qyMin; qy <= qyMax; ++qy)
                            std::cout << "STORE\t" << qx << "\t" << qy << "\t" << qz << "\t"
                                      << cell[static_cast<std::size_t>(qy - qyMin) * 16 + (qx & 3) * 4 + (qz & 3)] << "\n";
            }
        }
        // ChunkAccess.getNoiseBiome (ChunkAccess.java:425-437): quartY CLAMPED to the
        // chunk's section range, then the stored palette; outside the filled grid the
        // Java proxy falls back to the raw sampler (a live findValue query).
        auto storeNoiseBiome = [&gen, &biomeStore](int qx, int qy, int qz) -> std::string {
            return storeNoiseBiomeLookup(gen, biomeStore, qx, qy, qz);
        };
        // WorldGenRegion.getBiomeManager zoom over the chunk-cached biomes — the exact
        // biome the vanilla SURFACE step reads (NoiseBasedChunkGenerator.buildSurface
        // receives region.getBiomeManager(); its zoomed lookups resolve to the CHUNK-
        // CACHED noise biome via LevelReader.getNoiseBiome -> ChunkAccess).
        const std::int64_t zoomSeed = BiomeManager::obfuscateSeed(seed);
        auto storeZoomBiome = [&](int bx, int by, int bz) -> std::string {
            const std::array<int, 3> q = BiomeManager::debugSelectQuart(zoomSeed, bx, by, bz);
            return storeNoiseBiome(q[0], q[1], q[2]);
        };
        // Debug: zoomed lookups (diffable vs the Java GT's MCPP_DBG_ZOOM).
        if (const char* zenv = std::getenv("MCPP_DBG_ZOOM")) {
            std::stringstream zss(zenv);
            std::string cell;
            while (std::getline(zss, cell, ';')) {
                int zx, zy, zz;
                if (std::sscanf(cell.c_str(), "%d,%d,%d", &zx, &zy, &zz) == 3)
                    std::cout << "ZOOM\t" << zx << "\t" << zy << "\t" << zz << "\t"
                              << storeZoomBiome(zx, zy, zz) << "\n";
            }
        }

        // ---- PASS B: 5x5 terrain (inner 3x3 decorated; outer ring for neighbour
        // reads), dx OUTER / dz INNER exactly as the Java GT terrain pass (:408-423).
        // buildSurface reads the PASS-A store (chunk-cached biomes); the carvers'
        // topMaterial biome gate keeps the raw zoomed manager, mirroring
        // FullChunkDecorateParity.applyCarvers' withDifferentSource(raw sampler).
        std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>> chunks;
        std::vector<std::vector<BlockPos>> genMarks;   // per-chunk noise+carver marks, generation order
        for (int dx = -2; dx <= 2; ++dx) for (int dz = -2; dz <= 2; ++dz) {
            auto chunk = std::make_unique<mc::LevelChunk>(mc::ChunkPos{ Cx + dx, Cz + dz });
            std::vector<BlockPos> marks;   // NOISE-step marks first, then carver marks
            gen.fillFromNoise(*chunk, &marks);
            gen.buildSurface(*chunk, storeZoomBiome);
            gen.applyCarvers(*chunk, &marks);
            genMarks.push_back(std::move(marks));
            chunks.emplace(packChunk(Cx + dx, Cz + dz), std::move(chunk));
        }
        MultiChunkLevel level(&chunks, minY, maxY);
        // The NOISE step's aquifer-fluid marks (NoiseBasedChunkGenerator.java:
        // 442-446) and the carvers' marks (WorldCarver.java:147-158) precede every
        // decoration mark — inject them first, preserving generation order.
        for (const auto& marks : genMarks)
            for (const BlockPos& p : marks) level.markPosForPostprocessing(p);
        // Snapshot the *_WG heightmaps: setPersistedStatus(CARVERS) flips the chunk
        // to FINAL_HEIGHTMAPS maintenance, freezing the WG pair at its post-carver
        // values for the whole FEATURES phase (ChunkStatus.java:18,27).
        level.freezeHeights();

        // The biome filter's level.getBiome goes through the chunk-cached biomes
        // (clamped store reads), exactly like the surface above.
        DecoBiomeContext biomeCtx;
        biomeCtx.zoomSeed = zoomSeed;
        biomeCtx.features = &biomeFeatures;
        biomeCtx.noiseBiome = storeNoiseBiome;
        g_biomeCtx = &biomeCtx;
        g_level = &level;
        if (const char* w = std::getenv("MCPP_WATCH")) {
            int wx, wy, wz;
            if (std::sscanf(w, "%d,%d,%d", &wx, &wy, &wz) == 3)
                std::cerr << "WATCH-INIT (" << wx << "," << wy << "," << wz << ") = "
                          << level.getBlockState(BlockPos{ wx, wy, wz }) << "\n";
        }

        // Decorate the inner 3x3 in xz order (x outer asc, z inner asc) — the order under
        // which the Java ground truth (FullChunkDecorateParity.java) byte-matches the real
        // server .mca (6/6 primary chunks, commit 2772bdb6). Cross-chunk spill overlap is
        // last-writer-wins, so the order must match the ground truth exactly.
        // NOTE: while only part of the families is ported, mismatch counts vs the FULL
        // ground truth are confounded at borders by unported families overwriting cells;
        // they converge to a true 1:1 measure only as the remaining families are ported.
        // Do not tune the order against these numbers.
        // RandomState.getOrCreateRandomFactory("minecraft:worldgen_region_random")
        // (RandomState.java; WorldGenRegion.java:77,86): root Xoroshiro(seed)
        // .forkPositional().fromHashOf(id).forkPositional(). Overworld settings use
        // the Xoroshiro source (useLegacyRandomSource=false).
        const std::shared_ptr<PositionalRandomFactory> regionRandomFactory =
            std::make_shared<XoroshiroRandomSource>(static_cast<std::uint64_t>(seed))
                ->forkPositional()->fromHashOf("minecraft:worldgen_region_random")->forkPositional();
        for (int dx = -1; dx <= 1; ++dx) for (int dz = -1; dz <= 1; ++dz)
            decorateOneChunk(level, Cx + dx, Cz + dz, seed, resolver, storeNoiseBiome, *regionRandomFactory);
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
    if (g_skippedScheduleTicks > 0)
        std::cout << "SKIPPED-SCHEDULE-TICKS (need a ServerLevel; hard no-op as in the Java GT): "
                  << g_skippedScheduleTicks << "\n";
    if (!g_blocksMotionDefaulted.empty()) {
        std::cout << "BLOCKSMOTION-DEFAULTED (verify each vs Blocks.java and add to the table):";
        for (const auto& b : g_blocksMotionDefaulted) std::cout << " " << b;
        std::cout << "\n";
    }
    if (!g_solidRenderDefaulted.empty()) {
        std::cout << "SOLIDRENDER-DEFAULTED (verify each vs Blocks.java and add to the table):";
        for (const auto& b : g_solidRenderDefaulted) std::cout << " " << b;
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
#endif  // MCPP_DECORATE_NO_MAIN

// ====================== engine integration API (EngineDecoration.h) ======================
// The engine's streaming pipeline drives the EXACT machinery the parity gates
// certify, with the streaming-model deltas handled here:
//  (i)  *_WG heightmaps: the harness snapshots ALL chunks before any decoration;
//       the engine snapshots each chunk at generation time (engineFreezeWgHeights,
//       post-carvers, before the chunk is reachable by any decoration turn).
//  (ii) the PASS-A biome store fills on demand per 3x3 around the decorated
//       chunk, cached for the world's lifetime (ONE shared climate RTree).
//  (iii) MultiChunkLevel is LONG-LIVED over the engine's own chunk map
//       (setDecorating per chunk, exactly as in the harness).
// ORDER DELTA (documented): the ENGINE decorates chunks in neighbour-availability
// order (and runs the FULL-promotion postprocess right after each chunk's own
// decoration), not the ground truth's fixed xz batch order. Cross-chunk OVERLAP
// cells at chunk borders are last-writer-wins, so they can differ from the
// byte-certified dumps; the per-feature content is the certified code. A future
// engine-dump gate will pin the streaming order.
namespace mc::levelgen::feature {

struct EngineDecorationContext {
    DecorationResolver resolver;     // certified loaders + hooks + placed-feature cache
    NoiseBasedChunkGenerator gen;    // ONE shared climate RTree for the biome store
    std::uint64_t worldSeed;
    std::unordered_map<std::int64_t, std::vector<std::string>> biomeStore;   // PASS-A palettes
    std::function<std::string(int, int, int)> storeNoiseBiome;
    DecoBiomeContext biomeCtx;
    MultiChunkLevel level;           // long-lived region view over the engine chunk map
    std::shared_ptr<PositionalRandomFactory> regionRandomFactory;
    std::set<std::int64_t> begunTurns;

    EngineDecorationContext(const std::string& dataDir, std::uint64_t seed,
                            std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>>* chunks)
        : resolver(dataDir), gen(seed), worldSeed(seed),
          level(chunks, mc::CHUNK_MIN_Y, mc::CHUNK_MAX_Y) {
        g_curLevelSeed = static_cast<long long>(seed);   // level.getSeed() for cached placers (geode noise)
        storeNoiseBiome = [this](int qx, int qy, int qz) {
            return storeNoiseBiomeLookup(gen, biomeStore, qx, qy, qz);
        };
        // The biome filter's level.getBiome goes through the chunk-cached biomes
        // (clamped store reads), exactly like the harness.
        biomeCtx.zoomSeed = BiomeManager::obfuscateSeed(static_cast<std::int64_t>(seed));
        biomeCtx.noiseBiome = storeNoiseBiome;
        biomeCtx.features = &resolver.biomeFeatures;
        // RandomState.getOrCreateRandomFactory("minecraft:worldgen_region_random")
        // (RandomState.java; WorldGenRegion.java:77,86): root Xoroshiro(seed)
        // .forkPositional().fromHashOf(id).forkPositional().
        regionRandomFactory = std::make_shared<XoroshiroRandomSource>(seed)
            ->forkPositional()->fromHashOf("minecraft:worldgen_region_random")->forkPositional();
        g_biomeCtx = &biomeCtx;
        g_level = &level;
    }
};

static void prepareFeatureTurn(EngineDecorationContext* ctx, int cx, int cz) {
    if (!ctx) return;
    // PASS-A store for the 3x3 around the decorated chunk (possibleBiomes scan +
    // every zoomed biome read of this turn stays inside it), filled on demand.
    for (int dx = -1; dx <= 1; ++dx) for (int dz = -1; dz <= 1; ++dz) {
        const std::int64_t key = packChunk(cx + dx, cz + dz);
        if (ctx->biomeStore.find(key) == ctx->biomeStore.end())
            fillBiomeStoreChunk(ctx->gen, ctx->biomeStore, cx + dx, cz + dz);
    }
    g_biomeCtx = &ctx->biomeCtx;
    g_level = &ctx->level;
    g_curLevelSeed = static_cast<long long>(ctx->worldSeed);
    g_regionRandom = ctx->regionRandomFactory->at(cx * 16, 0, cz * 16);
    ctx->level.setDecorating(cx, cz);

    const std::int64_t turnKey = packChunk(cx, cz);
    if (ctx->begunTurns.insert(turnKey).second) {
        // ChunkStatusTasks.generateFeatures primes the non-WG heightmaps once at
        // the start of the FEATURES turn, before structures and biome features.
        ctx->level.primeNonWgHeightmaps(cx, cz);
    }
}

EngineDecorationContext* engineDecorationCreate(const std::string& dataDir, std::uint64_t worldSeed,
        std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>>* chunks) {
    try {
        auto ctx = std::make_unique<EngineDecorationContext>(dataDir, worldSeed, chunks);
        // Warm the placed-feature cache over the global step order (the same
        // fail-closed loader the gates certify; resolution touches no world RNG,
        // so eager-vs-lazy is behaviourally identical). Unported types stay
        // counted hard no-ops.
        for (const auto& sd : ctx->resolver.stepData)
            for (const std::string& fk : sd.features)
                (void)ctx->resolver.resolveFeature("minecraft:" + stripNs(fk));
        std::size_t resolved = 0, unported = 0;
        for (const auto& [key, placed] : ctx->resolver.cache) (placed ? resolved : unported) += 1;
        std::cout << "[INF] decoration context ready (" << resolved << " placed features resolved, "
                  << unported << " unported hard no-ops); worldgen data from disk: " << dataDir << "\n";
        std::cout.flush();
        return ctx.release();
    } catch (const std::exception& e) {
        std::cout << "[ERR] decoration context creation failed: " << e.what() << "\n";
        std::cout.flush();
        return nullptr;
    }
}

void engineDecorationDestroy(EngineDecorationContext* ctx) {
    if (!ctx) return;
    g_biomeCtx = nullptr;
    g_level = nullptr;
    g_tags = nullptr;
    g_fluidTags = nullptr;
    g_regionRandom.reset();
    delete ctx;
}

void engineFreezeWgHeights(EngineDecorationContext* ctx, mc::LevelChunk* chunk, int cx, int cz,
                           const std::vector<mc::BlockPos>* genMarks) {
    if (!ctx || !chunk) return;
    // Re-generation of previously seen coordinates (engine unload + regen): drop
    // the stale per-chunk state first (no-op for first-time chunks).
    ctx->level.resetChunkGenState(cx, cz);
    // setPersistedStatus(CARVERS) flips the chunk to FINAL_HEIGHTMAPS maintenance,
    // freezing the WG pair at its post-carver values for the whole FEATURES phase
    // (ChunkStatus.java:18,27).
    ctx->level.freezeChunkHeights(chunk, cx, cz);
    // The NOISE step's aquifer-fluid marks (NoiseBasedChunkGenerator.java:442-446)
    // and the carvers' marks (WorldCarver.java:147-158) precede every decoration
    // mark — inject them now, preserving generation order (all are within this chunk).
    if (genMarks)
        for (const mc::BlockPos& p : *genMarks) ctx->level.markPosForPostprocessing(p);
}

void engineDecorateChunk(EngineDecorationContext* ctx, int cx, int cz) {
    if (!ctx) return;
    prepareFeatureTurn(ctx, cx, cz);
    decorateOneChunk(ctx->level, cx, cz, static_cast<long long>(ctx->worldSeed),
                     ctx->resolver, ctx->storeNoiseBiome, *ctx->regionRandomFactory,
                     false);
    // FULL-promotion post-processing for this chunk — the same postProcessChunk the
    // server byte-match certifies (fluid SPREAD ticks stay counted hard no-ops).
    (void)postProcessChunk(ctx->level, cx, cz);
}

void engineBeginFeatureTurn(EngineDecorationContext* ctx, int cx, int cz) {
    prepareFeatureTurn(ctx, cx, cz);
}

bool enginePlaceStructurePoolFeature(EngineDecorationContext* ctx, const std::string& featureId,
                                     mc::levelgen::RandomSource& random, mc::BlockPos origin,
                                     int decoratingCx, int decoratingCz) {
    if (!ctx) return false;
    prepareFeatureTurn(ctx, decoratingCx, decoratingCz);
    const std::string normKey = "minecraft:" + stripNs(featureId);
    std::shared_ptr<PlacedFeature> placed = ctx->resolver.resolveFeature(normKey);
    if (!placed) return false;
    g_curFeatureKey = normKey;
    g_curTurnCx = decoratingCx;
    g_curTurnCz = decoratingCz;
    return placed->place(ctx->level, random, origin, genMinY, genDepth);
}

} // namespace mc::levelgen::feature

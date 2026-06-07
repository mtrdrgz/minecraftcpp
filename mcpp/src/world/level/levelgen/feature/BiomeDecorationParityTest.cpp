// Per-feature biome DECORATION parity test (data-driven, isolated from base-
// terrain FP). Loads a placed_feature + its configured_feature from the worldgen
// JSON, assembles the ported PlacedFeature pipeline, and compares the blocks it
// writes against the vanilla ground truth (tools/BiomeDecorationParity.java):
//   PRE  - vanilla pre-decoration chunk (terrain + surface)
//   PUT  - every block the real vanilla feature wrote
// Terrain comes from PRE, so any mismatch is purely a feature/placement bug.
// Unsupported modifier/feature/provider types throw (never silently faked).
//
//   biome_decoration_parity --cases <tsv> --placed <id> [--datadir <dir>]

#include "../RandomSource.h"
#include "../IntProvider.h"
#include "../placement/PlacedFeature.h"
#include "../placement/PlacementModifier.h"
#include "../placement/NoiseCountPlacement.h"
#include "../placement/HeightmapPlacement.h"
#include "stateproviders/BlockStateProvider.h"
#include "stateproviders/NoiseBasedStateProviders.h"
#include "../../block/Blocks.h"
#include "../../block/BlockState.h"
#include "../../block/BlockTags.h"
#include "../../chunk/LevelChunk.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace mc::levelgen;
using namespace mc::levelgen::placement;
using namespace mc::valueproviders;
namespace sp = mc::levelgen::feature::stateproviders;
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

// Blocks that are DoublePlantBlock in vanilla (placed as lower+upper).
bool isDoublePlant(const std::string& name) {
    static const std::set<std::string> dp = {
        "minecraft:tall_grass", "minecraft:large_fern", "minecraft:sunflower",
        "minecraft:lilac", "minecraft:rose_bush", "minecraft:peony",
        "minecraft:tall_seagrass", "minecraft:small_dripleaf", "minecraft:pitcher_plant",
    };
    return dp.count(name) != 0;
}

const mc::block::BlockTags* g_tags = nullptr;

// Block survival rule (the block-behaviour boundary), per block family.
bool blockCanSurvive(const std::string& state, const std::string& below) {
    if (state == "minecraft:dead_bush")
        return g_tags->isInTag(below, "minecraft:dead_bush_may_place_on");
    // VegetationBlock (grass, ferns, flowers, double plants, ...): #supports_vegetation
    return g_tags->isInTag(below, "minecraft:supports_vegetation");
}

class ChunkLevel final : public WorldGenLevel {
public:
    ChunkLevel(mc::LevelChunk* chunk, int cx, int cz, int minY, int maxY)
        : m_chunk(chunk), m_cx(cx), m_cz(cz), m_minY(minY), m_maxY(maxY) {
        m_airId = mc::getDefaultBlockStateId("air", 0);
    }
    bool inChunk(BlockPos p) const { return floorDiv(p.x, 16) == m_cx && floorDiv(p.z, 16) == m_cz; }
    int getMinY() const override { return m_minY; }
    int getHeight(Heightmap::Types type, int x, int z) const override {
        if (floorDiv(x, 16) != m_cx || floorDiv(z, 16) != m_cz) return m_minY - 1;
        for (int y = m_maxY - 1; y >= m_minY; --y) {
            const std::uint32_t id = m_chunk->getBlock(x, y, z);
            if (id == m_airId) continue;
            const bool fluid = (name(id) == "minecraft:water" || name(id) == "minecraft:lava");
            if ((type == Heightmap::Types::OCEAN_FLOOR_WG || type == Heightmap::Types::OCEAN_FLOOR) && fluid) continue;
            return y; // topmost matching == getFirstAvailable-1
        }
        return m_minY - 1;
    }
    std::string getBlockState(BlockPos p) const override {
        if (!inChunk(p) || p.y < m_minY || p.y >= m_maxY) return "minecraft:air";
        return name(m_chunk->getBlock(p.x, p.y, p.z));
    }
    void setBlock(BlockPos p, const std::string& state, int) override {
        if (!inChunk(p) || p.y < m_minY || p.y >= m_maxY) return;
        m_chunk->setBlock(p.x, p.y, p.z, mc::getDefaultBlockStateId(stripNs(state), m_airId));
    }
    bool isEmptyBlock(BlockPos p) const override { return getBlockState(p) == "minecraft:air"; }
    bool canSurvive(const std::string& state, BlockPos pos) const override {
        return blockCanSurvive(state, getBlockState(BlockPos{ pos.x, pos.y - 1, pos.z }));
    }
    std::string name(std::uint32_t id) const {
        const mc::BlockState* s = mc::getBlockState(id);
        if (!s || !s->block || s->block->name.empty()) return "minecraft:air";
        return "minecraft:" + s->block->name;
    }
private:
    mc::LevelChunk* m_chunk; int m_cx, m_cz, m_minY, m_maxY; std::uint32_t m_airId{};
};

// ---- JSON -> object loaders (throw on unsupported; never fake) ----

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

std::string stateName(const json& s) { return s.at("Name").get<std::string>(); }

sp::BlockStateProviderPtr loadStateProvider(const json& j) {
    const std::string t = stripNs(j.at("type").get<std::string>());
    if (t == "simple_state_provider" || t == "rotated_block_provider")
        return sp::SimpleStateProvider::of(stateName(j.at("state")));
    if (t == "weighted_state_provider") {
        std::vector<sp::WeightedStateProvider::Entry> es;
        for (const auto& e : j.at("entries"))
            es.push_back({ stateName(e.at("data")), e.at("weight").get<int>() });
        return std::make_shared<sp::WeightedStateProvider>(std::move(es));
    }
    if (t == "noise_threshold_provider") {
        NoiseParameters params;
        params.firstOctave = j.at("noise").at("firstOctave").get<int>();
        for (const auto& a : j.at("noise").at("amplitudes")) params.amplitudes.push_back(a.get<double>());
        std::vector<sp::BlockState> low, high;
        for (const auto& s : j.at("low_states")) low.push_back(stateName(s));
        for (const auto& s : j.at("high_states")) high.push_back(stateName(s));
        return std::make_shared<sp::NoiseThresholdProvider>(
            j.at("seed").get<std::int64_t>(), std::move(params),
            j.at("scale").get<float>(), j.at("threshold").get<float>(), j.at("high_chance").get<float>(),
            stateName(j.at("default_state")), std::move(low), std::move(high));
    }
    throw std::runtime_error("unsupported state_provider: " + t);
}

// Block predicates (return a functor on (level,pos)).
using Pred = std::function<bool(WorldGenLevel&, BlockPos)>;
Pred loadPredicate(const json& j) {
    const std::string t = stripNs(j.at("type").get<std::string>());
    auto off = [&](const json& o) -> BlockPos {
        if (o.contains("offset")) { auto a = o.at("offset"); return BlockPos{ a[0].get<int>(), a[1].get<int>(), a[2].get<int>() }; }
        return BlockPos{ 0, 0, 0 };
    };
    if (t == "true") return [](WorldGenLevel&, BlockPos) { return true; };
    if (t == "matching_block_tag") {
        const std::string tag = j.at("tag").get<std::string>(); const BlockPos o = off(j);
        return [tag, o](WorldGenLevel& l, BlockPos p) { return g_tags->isInTag(l.getBlockState(BlockPos{p.x+o.x,p.y+o.y,p.z+o.z}), tag); };
    }
    if (t == "matching_blocks") {
        std::set<std::string> blocks; const auto& b = j.at("blocks");
        if (b.is_string()) blocks.insert(b.get<std::string>()); else for (auto& x : b) blocks.insert(x.get<std::string>());
        const BlockPos o = off(j);
        return [blocks, o](WorldGenLevel& l, BlockPos p) { return blocks.count(l.getBlockState(BlockPos{p.x+o.x,p.y+o.y,p.z+o.z})) != 0; };
    }
    if (t == "would_survive") {
        const std::string st = stateName(j.at("state")); const BlockPos o = off(j);
        return [st, o](WorldGenLevel& l, BlockPos p) { return l.canSurvive(st, BlockPos{p.x+o.x,p.y+o.y,p.z+o.z}); };
    }
    if (t == "not") { Pred inner = loadPredicate(j.at("predicate")); return [inner](WorldGenLevel& l, BlockPos p) { return !inner(l, p); }; }
    if (t == "all_of" || t == "any_of") {
        std::vector<Pred> ps; for (auto& x : j.at("predicates")) ps.push_back(loadPredicate(x));
        const bool all = (t == "all_of");
        return [ps, all](WorldGenLevel& l, BlockPos p) { for (auto& q : ps) { bool r = q(l, p); if (all && !r) return false; if (!all && r) return true; } return all; };
    }
    throw std::runtime_error("unsupported block_predicate: " + t);
}

std::shared_ptr<const PlacementModifier> loadModifier(const json& j); // fwd

// simple_block feature placer (SimpleBlockFeature.place): provider state, canSurvive,
// DoublePlant => place lower+upper.
PlacedFeature::FeaturePlacer loadFeature(const json& cfg) {
    const std::string t = stripNs(cfg.at("type").get<std::string>());
    if (t == "simple_random_selector") {
        // SimpleRandomSelectorFeature.place: nextInt(size), then run the chosen
        // inline placed feature (which has its own placement modifiers).
        auto subs = std::make_shared<std::vector<std::shared_ptr<PlacedFeature>>>();
        for (const auto& entry : cfg.at("config").at("features")) {
            std::vector<std::shared_ptr<const PlacementModifier>> mods;
            if (entry.contains("placement"))
                for (const auto& m : entry.at("placement")) { auto mod = loadModifier(m); if (mod) mods.push_back(mod); }
            subs->push_back(std::make_shared<PlacedFeature>(loadFeature(entry.at("feature")), mods));
        }
        return [subs](WorldGenLevel& lvl, RandomSource& rng, BlockPos p) -> bool {
            const int i = rng.nextInt(static_cast<int>(subs->size()));
            return (*subs)[i]->place(lvl, rng, p, mc::CHUNK_MIN_Y, mc::CHUNK_MAX_Y - mc::CHUNK_MIN_Y);
        };
    }
    if (t == "simple_block") {
        sp::BlockStateProviderPtr provider = loadStateProvider(cfg.at("config").at("to_place"));
        return [provider](WorldGenLevel& lvl, RandomSource& rng, BlockPos p) -> bool {
            const std::string state = provider->getState(rng, p);
            if (!lvl.canSurvive(state, p)) return false;
            if (isDoublePlant(state)) {
                if (!lvl.isEmptyBlock(BlockPos{ p.x, p.y + 1, p.z })) return false;
                lvl.setBlock(p, state, 2);
                lvl.setBlock(BlockPos{ p.x, p.y + 1, p.z }, state, 2);
            } else {
                lvl.setBlock(p, state, 2);
            }
            return true;
        };
    }
    throw std::runtime_error("unsupported feature type: " + t);
}

std::shared_ptr<const PlacementModifier> loadModifier(const json& j) {
    const std::string t = stripNs(j.at("type").get<std::string>());
    if (t == "noise_threshold_count")
        return std::make_shared<NoiseThresholdCountPlacement>(j.at("noise_level").get<double>(), j.at("below_noise").get<int>(), j.at("above_noise").get<int>());
    if (t == "noise_based_count")
        return std::make_shared<NoiseBasedCountPlacement>(j.at("noise_to_count_ratio").get<int>(), j.at("noise_factor").get<double>(), j.value("noise_offset", 0.0));
    if (t == "count") return std::make_shared<CountPlacement>(loadIntProvider(j.at("count")));
    if (t == "rarity_filter") return std::make_shared<RarityFilter>(j.at("chance").get<int>());
    if (t == "in_square") return std::make_shared<InSquarePlacement>();
    if (t == "heightmap") {
        const std::string hm = j.at("heightmap").get<std::string>();
        Heightmap::Types ty = Heightmap::Types::WORLD_SURFACE_WG;
        if (hm == "MOTION_BLOCKING") ty = Heightmap::Types::MOTION_BLOCKING;
        else if (hm == "OCEAN_FLOOR_WG") ty = Heightmap::Types::OCEAN_FLOOR_WG;
        else if (hm == "OCEAN_FLOOR") ty = Heightmap::Types::OCEAN_FLOOR;
        else if (hm == "MOTION_BLOCKING_NO_LEAVES") ty = Heightmap::Types::MOTION_BLOCKING_NO_LEAVES;
        return std::make_shared<HeightmapPlacement>(ty);
    }
    if (t == "random_offset")
        return std::make_shared<RandomOffsetPlacement>(loadIntProvider(j.at("xz_spread")), loadIntProvider(j.at("y_spread")));
    if (t == "biome") return nullptr; // single forced biome => pass-through (no RNG)
    if (t == "block_predicate_filter") {
        Pred p = loadPredicate(j.at("predicate"));
        return std::make_shared<BlockPredicateFilter>([p](WorldGenLevel& l, BlockPos pos) { return p(l, pos); });
    }
    throw std::runtime_error("unsupported placement modifier: " + t);
}

struct Cell { std::map<std::tuple<int,int,int>, std::string> pre, put; };
using Key = std::tuple<long long, int, int>;

} // namespace

int main(int argc, char** argv) {
    std::string casesPath, placedId, dataDir;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
        else if (a == "--placed" && i + 1 < argc) placedId = argv[++i];
        else if (a == "--datadir" && i + 1 < argc) dataDir = argv[++i];
    }
    if (casesPath.empty() || placedId.empty()) { std::cerr << "usage: biome_decoration_parity --cases <tsv> --placed <id>\n"; return 2; }
    if (dataDir.empty()) dataDir = findDataDir();

    installBlockStatesEnv();
    mc::initBlocks();
    static mc::block::BlockTags tags = mc::block::BlockTags::loadFromDirectory(dataDir + "/tags/block");
    g_tags = &tags;

    // Load the placed + configured feature JSON and assemble the PlacedFeature.
    const std::string pname = stripNs(placedId);
    json placedJson; { std::ifstream f(dataDir + "/worldgen/placed_feature/" + pname + ".json"); if (!f) { std::cerr << "no placed_feature " << pname << "\n"; return 2; } f >> placedJson; }
    const std::string cfgId = stripNs(placedJson.at("feature").get<std::string>());
    json cfgJson; { std::ifstream f(dataDir + "/worldgen/configured_feature/" + cfgId + ".json"); if (!f) { std::cerr << "no configured_feature " << cfgId << "\n"; return 2; } f >> cfgJson; }

    std::vector<std::shared_ptr<const PlacementModifier>> mods;
    try {
        for (const auto& m : placedJson.at("placement")) {
            auto mod = loadModifier(m);
            if (mod) mods.push_back(mod);
        }
        PlacedFeature::FeaturePlacer placer = loadFeature(cfgJson);
        (void)placer;
    } catch (const std::exception& e) {
        std::cerr << "UNSUPPORTED " << placedId << ": " << e.what() << "\n";
        return 3;
    }
    PlacedFeature::FeaturePlacer placer = loadFeature(cfgJson);

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
    std::map<Key, Cell> cells;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::vector<std::string> f; std::string t;
        while (std::getline(ss, t, '\t')) f.push_back(t);
        if (f[0] == "PRE" && f.size() == 8)
            cells[{std::stoll(f[1]), std::stoi(f[2]), std::stoi(f[3])}].pre[{std::stoi(f[4]), std::stoi(f[5]), std::stoi(f[6])}] = f[7];
        else if (f[0] == "PUT" && f.size() == 9)
            cells[{std::stoll(f[1]), std::stoi(f[2]), std::stoi(f[3])}].put[{std::stoi(f[5]), std::stoi(f[6]), std::stoi(f[7])}] = f[8];
    }
    if (cells.empty()) { std::cerr << "no parity data\n"; return 2; }

    const int minY = mc::CHUNK_MIN_Y, maxY = mc::CHUNK_MAX_Y;
    const std::uint32_t airId = mc::getDefaultBlockStateId("air", 0);
    int total = 0, mismatches = 0, shown = 0, chunksOut = 0;

    for (auto& [key, cell] : cells) {
        const long long seed = std::get<0>(key);
        const int cx = std::get<1>(key), cz = std::get<2>(key);
        mc::LevelChunk chunk(mc::ChunkPos{ cx, cz });
        for (const auto& [xyz, blk] : cell.pre)
            chunk.setBlock(std::get<0>(xyz), std::get<1>(xyz), std::get<2>(xyz), mc::getDefaultBlockStateId(stripNs(blk), airId));

        ChunkLevel level(&chunk, cx, cz, minY, maxY);
        PlacedFeature placed(placer, mods);
        WorldgenRandom random(std::make_shared<XoroshiroRandomSource>(seed));
        const std::int64_t deco = random.setDecorationSeed(seed, cx * 16, cz * 16);
        random.setFeatureSeed(deco, 0, 0);
        placed.place(level, random, BlockPos{ cx * 16, 0, cz * 16 }, minY, maxY - minY);

        std::map<std::tuple<int,int,int>, std::string> got;
        for (int lx = 0; lx < 16; ++lx) for (int lz = 0; lz < 16; ++lz) {
            const int bx = cx * 16 + lx, bz = cz * 16 + lz;
            for (int y = minY; y < maxY; ++y) {
                const std::string cur = level.name(chunk.getBlock(bx, y, bz));
                auto pit = cell.pre.find({bx, y, bz});
                const std::string base = (pit != cell.pre.end()) ? pit->second : "minecraft:air";
                if (cur != base) got[{bx, y, bz}] = cur;
            }
        }
        if (!cell.put.empty()) ++chunksOut;
        auto rep = [&](const char* k, std::tuple<int,int,int> p, const std::string& g, const std::string& e) {
            if (shown++ < 60) std::cerr << "MISMATCH " << k << " seed=" << seed << " chunk=" << cx << "," << cz
                << " pos=(" << std::get<0>(p) << "," << std::get<1>(p) << "," << std::get<2>(p) << ") got=" << g << " expected=" << e << "\n";
        };
        for (const auto& [xyz, e] : cell.put) { ++total; auto g = got.find(xyz);
            if (g == got.end()) { ++mismatches; rep("missing", xyz, "<none>", e); }
            else if (g->second != e) { ++mismatches; rep("wrong", xyz, g->second, e); } }
        for (const auto& [xyz, g] : got) if (!cell.put.count(xyz)) { ++mismatches; ++total; rep("extra", xyz, g, "<none>"); }
    }

    std::cout << "BiomeDecoration placed=" << placedId << " chunks=" << cells.size()
              << " chunks_with_output=" << chunksOut << " placed_cases=" << total << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

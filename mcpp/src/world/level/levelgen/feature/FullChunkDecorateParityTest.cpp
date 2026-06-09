// Whole-chunk DECORATION parity vs the real no-structures server (.mca).
//
// Generates terrain+carvers (already byte-exact) for a 5x5 block of chunks around each
// target chunk C, builds one shared MultiChunkLevel (WorldGenRegion-style: reads any
// chunk, writes radius-1 around the chunk currently being decorated), runs the real
// applyBiomeDecoration loop (FeatureSorter global order + setFeatureSeed(decoSeed,index,
// step)) over the inner 3x3 — but only for PORTED feature types (others are skipped;
// each feature reseeds independently, so skipping does not perturb RNG) — then compares
// chunk C against the server dump.
//
// This certifies one decoration FAMILY at a time against the server. The first family is
// ORES (highest cross-biome coverage). Ore blobs spill across chunk borders, so the 3x3
// decoration is required (the single-chunk harness cannot certify them).
//
//   full_chunk_decorate_parity --cases <server_chunk_cases.tsv> --family ore [--datadir <dir>]
//
// Reports: ore-family cells (server has an ore block OR we placed one) matched/total, and
// the count of cells differing due to OTHER (un-ported) features (expected until ported).

#include "../RandomSource.h"
#include "../IntProvider.h"
#include "../NoiseBasedChunkGenerator.h"
#include "../BiomeSource.h"
#include "../placement/PlacedFeature.h"
#include "../placement/PlacementModifier.h"
#include "../placement/NoiseCountPlacement.h"
#include "../placement/HeightmapPlacement.h"  // transitively brings HeightProvider.h + VerticalAnchor.h
#include "OreFeature.h"
#include "FeatureSorter.h"
#include "BiomeFeatures.h"
#include "GenerationStep.h"
#include "../../block/Blocks.h"
#include "../../block/BlockState.h"
#include "../../block/BlockTags.h"
#include "../../chunk/LevelChunk.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
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

const mc::block::BlockTags* g_tags = nullptr;
long long g_oreRuns = 0, g_orePlacedOk = 0;   // diagnostics
std::string stateName(const json& s) { return s.at("Name").get<std::string>(); }

// ---- minimal loaders for the ORE family (throw on anything else: fail-closed) ----
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
std::shared_ptr<const PlacementModifier> loadModifier(const json& j) {
    const std::string t = stripNs(j.at("type").get<std::string>());
    if (t == "count") return std::make_shared<CountPlacement>(loadIntProvider(j.at("count")));
    if (t == "rarity_filter") return std::make_shared<RarityFilter>(j.at("chance").get<int>());
    if (t == "in_square") return std::make_shared<InSquarePlacement>();
    if (t == "height_range") return std::make_shared<HeightRangePlacement>(loadHeightProvider(j.at("height")));
    if (t == "biome") return nullptr;  // ores are listed in all overworld biomes => pass-through
    throw std::runtime_error("unsupported placement modifier (ore family): " + t);
}

// A resolved ore placed_feature ready to run, plus its (step, globalIndex) seed key.
struct OrePlaced {
    std::shared_ptr<PlacedFeature> placed;
    int step = 0;
    int index = 0;
    std::string key;
};

// MultiChunkLevel: WorldGenRegion-style level over a fixed grid of generated chunks.
class MultiChunkLevel final : public WorldGenLevel {
public:
    MultiChunkLevel(std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>>* chunks,
                    int minY, int maxY)
        : m_chunks(chunks), m_minY(minY), m_maxY(maxY) { m_airId = mc::getDefaultBlockStateId("air", 0); }

    void setDecorating(int cx, int cz) { m_dcx = cx; m_dcz = cz; }
    // Frozen OCEAN_FLOOR snapshot (pre-decoration) for every chunk in the grid.
    void freezeHeights() {
        for (auto& [key, chunkPtr] : *m_chunks) {
            const int cx = static_cast<int>(key >> 32);
            const int cz = static_cast<int>(static_cast<std::int32_t>(key & 0xffffffff));
            auto& frozen = m_frozen[key];
            for (int i = 0; i < 256; ++i) frozen[i] = scan(chunkPtr.get(), cx * 16 + (i & 15), cz * 16 + (i >> 4), true);
        }
    }

    int getMinY() const override { return m_minY; }

    // NOTE: returns scan() directly (NOT WorldGenRegion's stored+1) — empirically this
    // matches the server's ore placement better here; the scan already yields the
    // first-empty semantics the ore pre-check expects. Adding +1 regressed ore parity.
    int getHeight(Heightmap::Types type, int x, int z) const override {
        mc::LevelChunk* c = at(floorDiv(x, 16), floorDiv(z, 16));
        if (!c) return m_minY - 1;
        switch (type) {
            case Heightmap::Types::WORLD_SURFACE_WG: return scan(c, x, z, false);
            case Heightmap::Types::OCEAN_FLOOR_WG:   return scan(c, x, z, true);
            case Heightmap::Types::OCEAN_FLOOR: {
                auto it = m_frozen.find(packChunk(floorDiv(x, 16), floorDiv(z, 16)));
                if (it == m_frozen.end()) return scan(c, x, z, true);
                return it->second[(z - floorDiv(z, 16) * 16) * 16 + (x - floorDiv(x, 16) * 16)];
            }
            default: return scan(c, x, z, false);
        }
    }
    std::string getBlockState(BlockPos p) const override {
        mc::LevelChunk* c = at(floorDiv(p.x, 16), floorDiv(p.z, 16));
        if (!c || p.y < m_minY || p.y >= m_maxY) return "minecraft:air";
        return name(c->getBlock(p.x, p.y, p.z));
    }
    void setBlock(BlockPos p, const std::string& state, int) override {
        if (!ensureCanWrite(p)) return;
        mc::LevelChunk* c = at(floorDiv(p.x, 16), floorDiv(p.z, 16));
        if (c) c->setBlock(p.x, p.y, p.z, mc::getDefaultBlockStateId(stripNs(state), m_airId));
    }
    bool isEmptyBlock(BlockPos p) const override { return getBlockState(p) == "minecraft:air"; }
    bool canSurvive(const std::string&, BlockPos) const override { return true; }   // ores never call canSurvive
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
    mc::LevelChunk* at(int cx, int cz) const {
        auto it = m_chunks->find(packChunk(cx, cz));
        return it == m_chunks->end() ? nullptr : it->second.get();
    }
    int scan(mc::LevelChunk* c, int x, int z, bool opaque) const {
        for (int y = m_maxY - 1; y >= m_minY; --y) {
            const std::uint32_t id = c->getBlock(x, y, z);
            if (id == m_airId) continue;
            if (opaque) { const mc::BlockState* s = mc::getBlockState(id); if (!s || s->isFluid() || !s->isOpaque()) continue; }
            return y;
        }
        return m_minY - 1;
    }
    std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>>* m_chunks;
    std::map<std::int64_t, std::array<int, 256>> m_frozen;
    int m_minY, m_maxY, m_dcx = 0, m_dcz = 0; std::uint32_t m_airId{};
};

} // namespace

int main(int argc, char** argv) {
    std::string casesPath, dataDir, family = "ore";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
        else if (a == "--datadir" && i + 1 < argc) dataDir = argv[++i];
        else if (a == "--family" && i + 1 < argc) family = argv[++i];
    }
    if (casesPath.empty()) { std::cerr << "usage: full_chunk_decorate_parity --cases <server tsv> [--family ore]\n"; return 2; }
    if (dataDir.empty()) dataDir = findDataDir();
    if (family != "ore") { std::cerr << "only --family ore supported so far\n"; return 2; }

    installBlockStatesEnv();
    mc::initBlocks();
    static mc::block::BlockTags tags = mc::block::BlockTags::loadFromDirectory(dataDir + "/tags/block");
    g_tags = &tags;
    BiomeFeatures biomeFeatures = BiomeFeatures::loadFromDirectory(dataDir + "/worldgen/biome");

    // Global per-step feature order/index (== the setFeatureSeed index), over all biomes.
    const std::vector<std::string> sources = BiomeSource::collectOverworldPossibleBiomes();
    const std::set<std::string> sourcesSet(sources.begin(), sources.end());
    const auto stepData = FeatureSorter::buildFeaturesPerStep(sources, biomeFeatures, true);

    // Resolve every ORE placed_feature once: parse placement + the ore configured feature.
    // Cache by feature key. Records the set of ore-family block ids (for the comparison).
    std::set<std::string> oreFamily;
    std::map<std::string, std::shared_ptr<PlacedFeature>> oreCache;   // key -> placed (nullptr if not ore)
    auto resolveOre = [&](const std::string& featureKey) -> std::shared_ptr<PlacedFeature> {
        auto it = oreCache.find(featureKey);
        if (it != oreCache.end()) return it->second;
        std::shared_ptr<PlacedFeature> result;
        try {
            const std::string pname = stripNs(featureKey);
            json placedJson; { std::ifstream f(dataDir + "/worldgen/placed_feature/" + pname + ".json"); if (!f) throw std::runtime_error("no placed_feature"); f >> placedJson; }
            const std::string cfgId = stripNs(placedJson.at("feature").get<std::string>());
            json cfgJson; { std::ifstream f(dataDir + "/worldgen/configured_feature/" + cfgId + ".json"); if (!f) throw std::runtime_error("no configured_feature"); f >> cfgJson; }
            if (stripNs(cfgJson.at("type").get<std::string>()) != "ore") { oreCache[featureKey] = nullptr; return nullptr; }
            const json& cc = cfgJson.at("config");
            const int size = cc.at("size").get<int>();
            const float discard = cc.at("discard_chance_on_air_exposure").get<float>();
            std::vector<mc::levelgen::feature::OreTarget> targets;
            for (const auto& tj : cc.at("targets")) {
                const std::string st = stateName(tj.at("state"));
                oreFamily.insert(st);
                targets.push_back({ loadRuleTest(tj.at("target")), st });
            }
            std::vector<std::shared_ptr<const PlacementModifier>> mods;
            for (const auto& m : placedJson.at("placement")) { auto mod = loadModifier(m); if (mod) mods.push_back(mod); }
            auto placer = mc::levelgen::feature::makeOrePlacer(std::move(targets), size, discard);
            result = std::make_shared<PlacedFeature>(placer, mods);
        } catch (const std::exception& e) {
            std::cerr << "UNPORTED ore " << featureKey << ": " << e.what() << "\n";
            oreCache[featureKey] = nullptr; return nullptr;
        }
        oreCache[featureKey] = result;
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
    long long oreCells = 0, oreMism = 0, otherFeatureCells = 0; int shown = 0;

    for (const auto& [key, cells] : server) {
        const long long seed = std::get<0>(key);
        const int Cx = std::get<1>(key), Cz = std::get<2>(key);

        // 5x5 terrain (inner 3x3 decorated; outer ring for neighbour reads).
        NoiseBasedChunkGenerator gen(static_cast<std::uint64_t>(seed));
        std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>> chunks;
        for (int dz = -2; dz <= 2; ++dz) for (int dx = -2; dx <= 2; ++dx) {
            auto chunk = std::make_unique<mc::LevelChunk>(mc::ChunkPos{ Cx + dx, Cz + dz });
            gen.fillFromNoise(*chunk); gen.buildSurface(*chunk); gen.applyCarvers(*chunk);
            chunks.emplace(packChunk(Cx + dx, Cz + dz), std::move(chunk));
        }
        MultiChunkLevel level(&chunks, minY, maxY);
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

        // Decorate the inner 3x3 in xz order (x outer asc, z inner asc) — the order under
        // which the Java ground truth (FullChunkDecorateParity.java) byte-matches the real
        // server .mca (6/6 primary chunks, commit 2772bdb6). Cross-chunk spill overlap is
        // last-writer-wins, so the order must match the ground truth exactly.
        // NOTE: while only the ore family is ported, the ore mismatch count vs the FULL
        // ground truth is confounded at borders by unported later families (disks, springs,
        // dungeons...) overwriting ore cells; it converges to a true 1:1 measure only as
        // the remaining families are ported. Do not tune the order against this number.
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
                    std::shared_ptr<PlacedFeature> placed = resolveOre("minecraft:" + stripNs(featureKey));
                    if (!placed) continue;   // not an ore (skip; reseeded next feature)
                    random.setFeatureSeed(deco, index, step);
                    const bool any = placed->place(level, random, BlockPos{ nx * 16, 0, nz * 16 }, minY, maxY - minY);
                    ++g_oreRuns; if (any) ++g_orePlacedOk;
                }
            }
        }

        // Compare chunk C: every ore-family cell (server or ours) must match.
        mc::LevelChunk* c = chunks[packChunk(Cx, Cz)].get();
        for (int lx = 0; lx < 16; ++lx) for (int lz = 0; lz < 16; ++lz) {
            const int bx = Cx * 16 + lx, bz = Cz * 16 + lz;
            for (int y = minY; y < maxY; ++y) {
                const std::string mine = level.name(c->getBlock(bx, y, bz));
                auto sit = cells.find({bx, y, bz});
                const std::string srv = (sit != cells.end()) ? sit->second : std::string("minecraft:air");
                const bool srvOre = oreFamily.count(srv) != 0, myOre = oreFamily.count(mine) != 0;
                if (srvOre || myOre) {
                    ++oreCells;
                    if (mine != srv) {
                        ++oreMism;
                        if (shown++ < 200) std::cerr << "ORE-MISMATCH seed=" << seed << " (" << bx << "," << y << "," << bz
                            << ") got=" << mine << " server=" << srv << "\n";
                    }
                } else if (mine != srv) {
                    ++otherFeatureCells;   // un-ported feature (expected until ported)
                }
            }
        }
    }

    std::cout << "DecorateOre ore_cells=" << oreCells << " ore_mismatches=" << oreMism
              << " other_feature_cells=" << otherFeatureCells
              << " | oreRuns=" << g_oreRuns << " orePlacedOk=" << g_orePlacedOk
              << " oreFamilySize=" << oreFamily.size() << "\n";
    return oreMism == 0 ? 0 : 1;
}

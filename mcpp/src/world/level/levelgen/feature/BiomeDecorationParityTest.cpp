// Per-feature biome DECORATION parity test (isolated from base-terrain FP).
//
// The expected TSV (tools/BiomeDecorationParity.java) emits, per (seed, chunk):
//   PRE  - the vanilla pre-decoration chunk (terrain + surface)
//   PUT  - every block the REAL vanilla PlacedFeature wrote
// This loads PRE into a LevelChunk, runs the ported feature with the identical
// decoration seed over a WorldGenLevel backed by that chunk, and compares the
// blocks it writes against PUT. Terrain comes from PRE, so any mismatch is purely
// a feature/placement port bug. Certifies decoration features one at a time.
//
//   biome_decoration_parity --cases <tsv> --feature <id>

#include "../RandomSource.h"
#include "../IntProvider.h"
#include "../placement/PlacedFeature.h"
#include "../placement/PlacementModifier.h"
#include "../placement/NoiseCountPlacement.h"
#include "../placement/HeightmapPlacement.h"
#include "../../block/Blocks.h"
#include "../../block/BlockState.h"
#include "../../block/BlockTags.h"
#include "../../chunk/LevelChunk.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace mc::levelgen;
using namespace mc::levelgen::placement;
using namespace mc::valueproviders;

namespace {

int floorDiv(int x, int y) { int q = x / y, r = x % y; if (r != 0 && ((r < 0) != (y < 0))) --q; return q; }
std::string stripNs(const std::string& id) { auto c = id.find(':'); return c == std::string::npos ? id : id.substr(c + 1); }

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
std::string dataRoot() {
    for (const char* p : { "26.1.2/data/minecraft", "../26.1.2/data/minecraft" }) {
        std::ifstream f(std::string(p) + "/tags/block/dirt.json");
        if (f) return p;
    }
    return "26.1.2/data/minecraft";
}

// WorldGenLevel over one LevelChunk; single-chunk isolation (air outside, drop
// out-of-chunk writes) to mirror the Java parity harness.
class ChunkLevel final : public WorldGenLevel {
public:
    ChunkLevel(mc::LevelChunk* chunk, int cx, int cz, int minY, int maxY, const mc::block::BlockTags* tags)
        : m_chunk(chunk), m_cx(cx), m_cz(cz), m_minY(minY), m_maxY(maxY), m_tags(tags) {
        m_airId = mc::getDefaultBlockStateId("air", 0);
    }

    bool inChunk(BlockPos p) const { return floorDiv(p.x, 16) == m_cx && floorDiv(p.z, 16) == m_cz; }

    int getMinY() const override { return m_minY; }

    int getHeight(Heightmap::Types type, int x, int z) const override {
        if (floorDiv(x, 16) != m_cx || floorDiv(z, 16) != m_cz) return m_minY - 1;
        for (int y = m_maxY - 1; y >= m_minY; --y) {
            const std::uint32_t id = m_chunk->getBlock(x, y, z);
            if (id == m_airId) continue;
            const bool isFluid = (name(id) == "minecraft:water" || name(id) == "minecraft:lava");
            if (type == Heightmap::Types::OCEAN_FLOOR_WG || type == Heightmap::Types::OCEAN_FLOOR) {
                if (isFluid) continue;
            }
            return y; // topmost matching block == getFirstAvailable-1
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
        // VegetationBlock.canSurvive: mayPlaceOn(state below) = below in
        // #minecraft:supports_vegetation. (short_grass / fern.)
        const std::string below = getBlockState(BlockPos{ pos.x, pos.y - 1, pos.z });
        if (state == "minecraft:short_grass" || state == "minecraft:fern") {
            return m_tags->isInTag(below, "minecraft:supports_vegetation");
        }
        return false; // only the plants this test exercises are modelled
    }

    std::string name(std::uint32_t id) const {
        const mc::BlockState* s = mc::getBlockState(id);
        if (!s || !s->block || s->block->name.empty()) return "minecraft:air";
        return "minecraft:" + s->block->name;
    }

private:
    mc::LevelChunk* m_chunk;
    int m_cx, m_cz, m_minY, m_maxY;
    const mc::block::BlockTags* m_tags;
    std::uint32_t m_airId{};
};

// block_predicate matching_block_tag #air  (air / cave_air / void_air)
class AirPredicateFilter final : public PlacementModifier {
public:
    std::vector<BlockPos> getPositions(PlacementContext* ctx, RandomSource&, BlockPos p) const override {
        const std::string b = ctx->getLevel()->getBlockState(p);
        const bool air = (b == "minecraft:air" || b == "minecraft:cave_air" || b == "minecraft:void_air");
        return air ? std::vector<BlockPos>{ p } : std::vector<BlockPos>{};
    }
};

using Key = std::tuple<long long, int, int>;
struct Cell { std::map<std::tuple<int,int,int>, std::string> pre, put; };

} // namespace

int main(int argc, char** argv) {
    std::string casesPath, feature = "minecraft:patch_grass_plain";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
        else if (a == "--feature" && i + 1 < argc) feature = argv[++i];
    }
    if (casesPath.empty()) { std::cerr << "usage: biome_decoration_parity --cases <tsv> [--feature id]\n"; return 2; }

    installBlockStatesEnv();
    mc::initBlocks();
    mc::block::BlockTags tags = mc::block::BlockTags::loadFromDirectory(dataRoot() + "/tags/block");

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
    std::map<Key, Cell> cells;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::vector<std::string> f; std::string t;
        while (std::getline(ss, t, '\t')) f.push_back(t);
        if (f[0] == "PRE" && f.size() == 8) {
            cells[{std::stoll(f[1]), std::stoi(f[2]), std::stoi(f[3])}]
                .pre[{std::stoi(f[4]), std::stoi(f[5]), std::stoi(f[6])}] = f[7];
        } else if (f[0] == "PUT" && f.size() == 9) {
            cells[{std::stoll(f[1]), std::stoi(f[2]), std::stoi(f[3])}]
                .put[{std::stoi(f[5]), std::stoi(f[6]), std::stoi(f[7])}] = f[8];
        }
    }
    if (cells.empty()) { std::cerr << "no parity data\n"; return 2; }

    const int minY = mc::CHUNK_MIN_Y, maxY = mc::CHUNK_MAX_Y;
    const std::uint32_t airId = mc::getDefaultBlockStateId("air", 0);

    int total = 0, mismatches = 0, shown = 0, chunksWithGrass = 0;

    for (auto& [key, cell] : cells) {
        const long long seed = std::get<0>(key);
        const int cx = std::get<1>(key), cz = std::get<2>(key);

        mc::LevelChunk chunk(mc::ChunkPos{ cx, cz });
        for (const auto& [xyz, blk] : cell.pre) {
            const auto [x, y, z] = xyz;
            chunk.setBlock(x, y, z, mc::getDefaultBlockStateId(stripNs(blk), airId));
        }

        ChunkLevel level(&chunk, cx, cz, minY, maxY, &tags);

        // Assemble patch_grass_plain: noise_threshold_count, in_square, heightmap,
        // (biome: pass-through), count(32), random_offset(trap xz +/-7, y +/-3),
        // block_predicate_filter(#air); feature = simple_block(short_grass).
        std::vector<std::shared_ptr<const PlacementModifier>> mods = {
            std::make_shared<NoiseThresholdCountPlacement>(-0.8, 5, 10),
            std::make_shared<InSquarePlacement>(),
            std::make_shared<HeightmapPlacement>(Heightmap::Types::WORLD_SURFACE_WG),
            std::make_shared<CountPlacement>(ConstantInt::of(32)),
            std::make_shared<RandomOffsetPlacement>(TrapezoidInt::of(-7, 7, 0), TrapezoidInt::of(-3, 3, 0)),
            std::make_shared<AirPredicateFilter>(),
        };
        PlacedFeature::FeaturePlacer grass = [](WorldGenLevel& lvl, RandomSource&, BlockPos p) -> bool {
            const std::string state = "minecraft:short_grass";
            if (lvl.canSurvive(state, p)) { lvl.setBlock(p, state, 2); return true; }
            return false;
        };
        PlacedFeature placed(grass, mods);

        WorldgenRandom random(std::make_shared<XoroshiroRandomSource>(seed));
        random.setFeatureSeed(seed, 0, 0);
        placed.place(level, random, BlockPos{ cx * 16, 0, cz * 16 }, minY, maxY - minY);

        // Diff chunk [0,16) vs PRE baseline -> got; compare to PUT.
        std::map<std::tuple<int,int,int>, std::string> got;
        for (int lx = 0; lx < 16; ++lx) for (int lz = 0; lz < 16; ++lz) {
            const int bx = cx * 16 + lx, bz = cz * 16 + lz;
            for (int y = minY; y < maxY; ++y) {
                const std::uint32_t id = chunk.getBlock(bx, y, bz);
                const std::string cur = level.name(id);
                auto pit = cell.pre.find({bx, y, bz});
                const std::string base = (pit != cell.pre.end()) ? pit->second : "minecraft:air";
                if (cur != base) got[{bx, y, bz}] = cur;
            }
        }
        if (!cell.put.empty()) ++chunksWithGrass;

        // Compare got vs expected put.
        auto report = [&](const std::string& kind, int x, int y, int z, const std::string& g, const std::string& e) {
            if (shown++ < 60)
                std::cerr << "MISMATCH " << kind << " seed=" << seed << " chunk=" << cx << "," << cz
                          << " pos=(" << x << "," << y << "," << z << ") got=" << g << " expected=" << e << "\n";
        };
        for (const auto& [xyz, e] : cell.put) {
            ++total;
            auto g = got.find(xyz);
            if (g == got.end()) { ++mismatches; report("missing", std::get<0>(xyz), std::get<1>(xyz), std::get<2>(xyz), "<none>", e); }
            else if (g->second != e) { ++mismatches; report("wrong", std::get<0>(xyz), std::get<1>(xyz), std::get<2>(xyz), g->second, e); }
        }
        for (const auto& [xyz, g] : got) {
            if (!cell.put.count(xyz)) { ++mismatches; ++total; report("extra", std::get<0>(xyz), std::get<1>(xyz), std::get<2>(xyz), g, "<none>"); }
        }
    }

    std::cout << "BiomeDecoration feature=" << feature << " chunks=" << cells.size()
              << " chunks_with_output=" << chunksWithGrass << " placed_cases=" << total
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
